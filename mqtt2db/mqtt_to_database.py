#!/usr/bin/env python3
"""
MQTT CAN Message Database Logger

This script subscribes to MQTT topics for CAN messages and stores them
in a database (PostgreSQL, MySQL, SQLite, or InfluxDB).

Usage:
    python3 mqtt_to_database.py --config config.yaml

Requirements:
    pip install paho-mqtt sqlalchemy psycopg2-binary pymysql influxdb-client pyyaml

Author: ha-stiebel-control project
License: GPLv3
"""

import json
import logging
import argparse
import sys
import os
from datetime import datetime
from pathlib import Path
import signal
import time

# Silence SQLAlchemy 2.0 deprecation warnings
os.environ['SQLALCHEMY_SILENCE_UBER_WARNING'] = '1'

try:
    import paho.mqtt.client as mqtt
    import yaml
except ImportError as e:
    print(f"Missing required package: {e}")
    print("Install with: pip install paho-mqtt pyyaml")
    sys.exit(1)

# Optional database imports
try:
    from sqlalchemy import create_engine, Column, Integer, String, DateTime, Text
    from sqlalchemy.orm import sessionmaker
    SQLALCHEMY_AVAILABLE = True
    
    # Try SQLAlchemy 2.0 API first
    try:
        from sqlalchemy.orm import DeclarativeBase
        class Base(DeclarativeBase):
            pass
    except ImportError:
        # Fall back to SQLAlchemy 1.x API
        from sqlalchemy.ext.declarative import declarative_base
        Base = declarative_base()
except ImportError:
    SQLALCHEMY_AVAILABLE = False
    Base = None

try:
    from influxdb_client import InfluxDBClient, Point
    from influxdb_client.client.write_api import SYNCHRONOUS
    INFLUXDB_AVAILABLE = True
except ImportError:
    INFLUXDB_AVAILABLE = False


# Database model for SQLAlchemy
if SQLALCHEMY_AVAILABLE:
    class CanMessage(Base):
        __tablename__ = 'can_messages'
        
        id = Column(Integer, primary_key=True)
        timestamp = Column(DateTime, nullable=False, index=True)
        esp_timestamp = Column(Integer)  # Milliseconds since ESP boot
        sender_can_id = Column(Integer, nullable=False, index=True)
        sender_name = Column(String(20), index=True)
        elster_idx = Column(Integer, index=True)
        name = Column(String(100), index=True)
        value = Column(String(50))
        value_type = Column(String(20))
        raw_hex = Column(Text)
        raw_value = Column(Integer)
        
        def __repr__(self):
            return f"<CanMessage({self.timestamp}, {self.sender_name}, {self.name}={self.value})>"


class MqttCanLogger:
    def __init__(self, config_path):
        self.config = self.load_config(config_path)
        self.setup_logging()
        self.mqtt_client = None
        self.db_engine = None
        self.db_session = None
        self.influx_client = None
        self.influx_write_api = None
        self.message_count = 0
        self.running = True
        
    def load_config(self, config_path):
        """Load configuration from YAML file"""
        with open(config_path, 'r') as f:
            config = yaml.safe_load(f)
        return config
    
    def setup_logging(self):
        """Setup logging"""
        log_level = self.config.get('logging', {}).get('level', 'INFO')
        logging.basicConfig(
            level=getattr(logging, log_level),
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
        self.logger = logging.getLogger('mqtt_can_logger')
    
    def setup_database(self):
        """Setup database connection"""
        db_config = self.config.get('database', {})
        db_type = db_config.get('type', 'sqlite')
        
        if db_type == 'influxdb':
            if not INFLUXDB_AVAILABLE:
                self.logger.error("InfluxDB client not installed. pip install influxdb-client")
                return False
            return self.setup_influxdb()
        else:
            if not SQLALCHEMY_AVAILABLE:
                self.logger.error("SQLAlchemy not installed. pip install sqlalchemy")
                return False
            return self.setup_sql_database()
    
    def setup_sql_database(self):
        """Setup SQL database (PostgreSQL, MySQL, SQLite)"""
        db_config = self.config.get('database', {})
        db_type = db_config.get('type', 'sqlite')
        
        # Build connection string
        if db_type == 'sqlite':
            db_path = db_config.get('path', 'can_messages.db')
            conn_str = f'sqlite:///{db_path}'
        elif db_type == 'postgresql':
            conn_str = (f"postgresql://{db_config['username']}:{db_config['password']}"
                       f"@{db_config['host']}:{db_config.get('port', 5432)}"
                       f"/{db_config['database']}")
        elif db_type == 'mysql':
            conn_str = (f"mysql+pymysql://{db_config['username']}:{db_config['password']}"
                       f"@{db_config['host']}:{db_config.get('port', 3306)}"
                       f"/{db_config['database']}")
        else:
            self.logger.error(f"Unsupported database type: {db_type}")
            return False
        
        try:
            self.db_engine = create_engine(conn_str, echo=False)
            Base.metadata.create_all(self.db_engine)
            Session = sessionmaker(bind=self.db_engine)
            self.db_session = Session()
            self.logger.info(f"Connected to {db_type} database")
            return True
        except Exception as e:
            self.logger.error(f"Failed to connect to database: {e}")
            return False
    
    def setup_influxdb(self):
        """Setup InfluxDB connection"""
        db_config = self.config.get('database', {})
        
        try:
            self.influx_client = InfluxDBClient(
                url=db_config['url'],
                token=db_config['token'],
                org=db_config['org']
            )
            self.influx_write_api = self.influx_client.write_api(write_options=SYNCHRONOUS)
            self.logger.info("Connected to InfluxDB")
            return True
        except Exception as e:
            self.logger.error(f"Failed to connect to InfluxDB: {e}")
            return False
    
    def setup_mqtt(self):
        """Setup MQTT connection"""
        mqtt_config = self.config.get('mqtt', {})
        
        self.mqtt_client = mqtt.Client(client_id=mqtt_config.get('client_id', 'can_logger'))
        
        if 'username' in mqtt_config and 'password' in mqtt_config:
            self.mqtt_client.username_pw_set(mqtt_config['username'], mqtt_config['password'])
        
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        self.mqtt_client.on_disconnect = self.on_mqtt_disconnect
        
        try:
            broker = mqtt_config.get('broker', 'localhost')
            port = mqtt_config.get('port', 1883)
            self.mqtt_client.connect(broker, port, 60)
            self.logger.info(f"Connecting to MQTT broker at {broker}:{port}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to connect to MQTT broker: {e}")
            return False
    
    def on_mqtt_connect(self, client, userdata, flags, rc):
        """Callback when connected to MQTT"""
        if rc == 0:
            self.logger.info("Connected to MQTT broker")
            topic = self.config.get('mqtt', {}).get('topic', 'homeassistant/stiebel/can_raw/#')
            client.subscribe(topic)
            self.logger.info(f"Subscribed to topic: {topic}")
        else:
            self.logger.error(f"Failed to connect to MQTT broker. Return code: {rc}")
    
    def on_mqtt_disconnect(self, client, userdata, rc):
        """Callback when disconnected from MQTT"""
        if rc != 0:
            self.logger.warning(f"Unexpected MQTT disconnection. Will auto-reconnect. RC: {rc}")
    
    def on_mqtt_message(self, client, userdata, msg):
        """Callback when MQTT message received"""
        try:
            payload = json.loads(msg.payload.decode())
            self.store_message(payload)
            self.message_count += 1
            
            if self.message_count % 100 == 0:
                self.logger.info(f"Processed {self.message_count} messages")
                
        except json.JSONDecodeError as e:
            self.logger.error(f"Invalid JSON in message: {e}")
        except Exception as e:
            self.logger.error(f"Error processing message: {e}", exc_info=True)
    
    def store_message(self, payload):
        """Store message in database"""
        db_type = self.config.get('database', {}).get('type', 'sqlite')
        
        if db_type == 'influxdb':
            self.store_to_influxdb(payload)
        else:
            self.store_to_sql(payload)
    
    def store_to_sql(self, payload):
        """Store message in SQL database"""
        if not self.db_session:
            return
        
        try:
            msg = CanMessage(
                timestamp=datetime.now(),
                esp_timestamp=payload.get('timestamp'),
                sender_can_id=payload.get('sender_can_id'),
                sender_name=payload.get('sender_name'),
                elster_idx=payload.get('elster_idx'),
                name=payload.get('name'),
                value=payload.get('value'),
                value_type=payload.get('type'),
                raw_hex=payload.get('raw_hex'),
                raw_value=payload.get('raw_value')
            )
            self.db_session.add(msg)
            self.db_session.commit()
        except Exception as e:
            self.logger.error(f"Failed to store message in SQL: {e}")
            self.db_session.rollback()
    
    def store_to_influxdb(self, payload):
        """Store message in InfluxDB"""
        if not self.influx_write_api:
            return
        
        try:
            bucket = self.config.get('database', {}).get('bucket', 'can_messages')
            
            point = (Point("can_message")
                    .tag("sender_can_id", str(payload.get('sender_can_id')))
                    .tag("sender_name", payload.get('sender_name'))
                    .tag("name", payload.get('name'))
                    .tag("type", payload.get('type'))
                    .field("value", payload.get('value'))
                    .field("raw_value", payload.get('raw_value'))
                    .field("elster_idx", payload.get('elster_idx'))
                    .field("esp_timestamp", payload.get('timestamp')))
            
            self.influx_write_api.write(bucket=bucket, record=point)
        except Exception as e:
            self.logger.error(f"Failed to store message in InfluxDB: {e}")
    
    def run(self):
        """Main run loop"""
        if not self.setup_database():
            self.logger.error("Failed to setup database. Exiting.")
            return 1
        
        if not self.setup_mqtt():
            self.logger.error("Failed to setup MQTT. Exiting.")
            return 1
        
        # Setup signal handlers
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
        
        self.logger.info("Starting MQTT CAN logger...")
        self.mqtt_client.loop_start()
        
        try:
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
        
        self.shutdown()
        return 0
    
    def signal_handler(self, signum, frame):
        """Handle shutdown signals"""
        self.logger.info(f"Received signal {signum}. Shutting down...")
        self.running = False
    
    def shutdown(self):
        """Cleanup and shutdown"""
        self.logger.info(f"Shutting down. Total messages processed: {self.message_count}")
        
        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
        
        if self.db_session:
            self.db_session.close()
        
        if self.influx_client:
            self.influx_client.close()


def main():
    parser = argparse.ArgumentParser(
        description='MQTT CAN Message Database Logger',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Example config.yaml:

mqtt:
  broker: 192.168.1.100
  port: 1883
  username: mqtt_user
  password: mqtt_password
  topic: homeassistant/stiebel/can_raw/#

database:
  type: postgresql  # or mysql, sqlite, influxdb
  host: 192.168.1.200
  port: 5432
  database: stiebel
  username: db_user
  password: db_password

logging:
  level: INFO
        """
    )
    
    parser.add_argument('--config', type=str, required=True,
                       help='Path to configuration file (YAML)')
    parser.add_argument('--test', action='store_true',
                       help='Test configuration and exit')
    
    args = parser.parse_args()
    
    if not Path(args.config).exists():
        print(f"Error: Configuration file not found: {args.config}")
        return 1
    
    logger = MqttCanLogger(args.config)
    
    if args.test:
        print("Configuration loaded successfully!")
        print(f"MQTT Broker: {logger.config['mqtt']['broker']}")
        print(f"Database Type: {logger.config['database']['type']}")
        return 0
    
    return logger.run()


if __name__ == '__main__':
    sys.exit(main())

