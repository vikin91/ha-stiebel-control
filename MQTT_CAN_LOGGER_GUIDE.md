# MQTT CAN Message Logger - Complete Guide

## Overview

This system publishes all CAN messages from your Stiebel Eltron heat pump to MQTT, allowing you to:
- Store messages in **any database** (PostgreSQL, MySQL, SQLite, InfluxDB)
- Access from **any device** on your network
- Integrate with **Home Assistant** automations
- Analyze with **external tools** (Grafana, Jupyter, Excel)
- Store on a **separate server** (not on the ESP32)

## Architecture

```
ESP32 (ESPHome)
    │
    │ Publishes JSON via MQTT
    ↓
MQTT Broker (Home Assistant or Mosquitto)
    │
    │ Subscribes
    ↓
mqtt_to_database.py (Python Script)
    │
    │ Stores to database
    ↓
Database (PostgreSQL / MySQL / SQLite / InfluxDB)
```

## Quick Start (5 Minutes)

### 1. Add to ESP Home Configuration

Add this to your ESPHome YAML:

```yaml
mqtt:
  broker: 192.168.1.100  # Your MQTT broker (Home Assistant)
  port: 1883
  username: !secret mqtt_username
  password: !secret mqtt_password
  id: mqtt_client

esphome:
  includes:
    - stiebeltools/CanMessageMqttLogger.h
```

Deploy to your ESP32 - **Done!** All CAN messages are now published to MQTT.

### 2. Setup Database Logger (Optional)

If you want to store messages in a database:

```bash
# Install dependencies
pip install paho-mqtt pyyaml sqlalchemy psycopg2-binary

# Edit configuration
cp mqtt_logger_config.yaml my_config.yaml
nano my_config.yaml  # Edit with your settings

# Run the logger
python3 mqtt_to_database.py --config my_config.yaml
```

## ESPHome Configuration

### Basic MQTT Setup

```yaml
mqtt:
  broker: 192.168.1.100
  port: 1883
  username: !secret mqtt_username
  password: !secret mqtt_password
  id: mqtt_client
  birth_message:
    topic: homeassistant/stiebel/status
    payload: online
  will_message:
    topic: homeassistant/stiebel/status
    payload: offline

esphome:
  includes:
    - stiebeltools/heatingpump.h
    - stiebeltools/ElsterTable.h
    - stiebeltools/KElsterTable.h
    - stiebeltools/KElsterTable.cpp
    - stiebeltools/NUtils.cpp
    - stiebeltools/CanMessageMqttLogger.h  # ADD THIS LINE
```

### With TLS/SSL (Secure)

```yaml
mqtt:
  broker: 192.168.1.100
  port: 8883  # TLS port
  username: !secret mqtt_username
  password: !secret mqtt_password
  id: mqtt_client
  ssl_fingerprints:
    - "AA BB CC DD EE FF 00 11 22 33 44 55 66 77 88 99 AA BB CC DD"
```

### With Home Assistant MQTT Discovery

```yaml
mqtt:
  broker: !secret mqtt_broker
  discovery: true
  discovery_prefix: homeassistant
  id: mqtt_client
```

## MQTT Topics

All messages are published to:
```
homeassistant/stiebel/can_raw/<CAN_ID>/<PARAMETER_NAME>
```

### Examples:
- `homeassistant/stiebel/can_raw/180/OUTSIDE_TEMP`
- `homeassistant/stiebel/can_raw/180/RETURN_FLOW_INTERNAL_TEMP`
- `homeassistant/stiebel/can_raw/480/ERROR_MESSAGE`
- `homeassistant/stiebel/can_raw/301/FLOW_SETPOINT_TEMP`

### Message Payload (JSON)

```json
{
  "timestamp": 123456,
  "can_id": 384,
  "addressee": "PUMP",
  "elster_idx": 90,
  "name": "OUTSIDE_TEMP",
  "value": "23.5",
  "type": "dec_val",
  "raw_hex": "31 00 5a 00 e8",
  "raw_value": 1000
}
```

## Database Setup

### Option 1: PostgreSQL (Recommended)

**Install PostgreSQL:**
```bash
# Ubuntu/Debian
sudo apt install postgresql postgresql-contrib

# macOS
brew install postgresql
```

**Setup Database:**
```sql
CREATE DATABASE stiebel_can;
CREATE USER dbuser WITH PASSWORD 'dbpassword';
GRANT ALL PRIVILEGES ON DATABASE stiebel_can TO dbuser;
```

**Configuration:**
```yaml
database:
  type: postgresql
  host: 192.168.1.200
  port: 5432
  database: stiebel_can
  username: dbuser
  password: dbpassword
```

**Install Python driver:**
```bash
pip install psycopg2-binary
```

### Option 2: MySQL/MariaDB

**Install MySQL:**
```bash
# Ubuntu/Debian
sudo apt install mysql-server

# macOS
brew install mysql
```

**Setup Database:**
```sql
CREATE DATABASE stiebel_can;
CREATE USER 'dbuser'@'%' IDENTIFIED BY 'dbpassword';
GRANT ALL PRIVILEGES ON stiebel_can.* TO 'dbuser'@'%';
FLUSH PRIVILEGES;
```

**Configuration:**
```yaml
database:
  type: mysql
  host: 192.168.1.200
  port: 3306
  database: stiebel_can
  username: dbuser
  password: dbpassword
```

**Install Python driver:**
```bash
pip install pymysql
```

### Option 3: SQLite (Simplest)

**No installation needed!** Just configure:

```yaml
database:
  type: sqlite
  path: /home/user/can_messages.db
```

**Great for:**
- Testing and development
- Small deployments
- Local analysis
- Portable database files

### Option 4: InfluxDB (Time-Series)

**Install InfluxDB:**
```bash
# Ubuntu/Debian
wget https://dl.influxdata.com/influxdb/releases/influxdb2-2.7.1-amd64.deb
sudo dpkg -i influxdb2-2.7.1-amd64.deb

# macOS
brew install influxdb
```

**Setup:**
1. Access web UI: http://localhost:8086
2. Create organization and bucket 'can_messages'
3. Generate API token

**Configuration:**
```yaml
database:
  type: influxdb
  url: http://192.168.1.200:8086
  token: your-influxdb-token
  org: your-org
  bucket: can_messages
```

**Install Python driver:**
```bash
pip install influxdb-client
```

**Perfect for:**
- Grafana dashboards
- Time-series analysis
- Large-scale data
- Real-time monitoring

## Running the Logger

### Manual Start

```bash
python3 mqtt_to_database.py --config mqtt_logger_config.yaml
```

### Test Configuration

```bash
python3 mqtt_to_database.py --config mqtt_logger_config.yaml --test
```

### Run as Systemd Service (Linux)

Create `/etc/systemd/system/can-logger.service`:

```ini
[Unit]
Description=MQTT CAN Message Logger
After=network.target

[Service]
Type=simple
User=your-user
WorkingDirectory=/home/your-user/ha-stiebel-control
ExecStart=/usr/bin/python3 /home/your-user/ha-stiebel-control/mqtt_to_database.py --config /home/your-user/ha-stiebel-control/mqtt_logger_config.yaml
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable can-logger
sudo systemctl start can-logger
sudo systemctl status can-logger
```

View logs:
```bash
sudo journalctl -u can-logger -f
```

### Run with Docker

Create `Dockerfile`:

```dockerfile
FROM python:3.11-slim

WORKDIR /app

COPY mqtt_to_database.py .
COPY mqtt_logger_config.yaml .

RUN pip install paho-mqtt pyyaml sqlalchemy psycopg2-binary

CMD ["python3", "mqtt_to_database.py", "--config", "mqtt_logger_config.yaml"]
```

Build and run:
```bash
docker build -t can-logger .
docker run -d --name can-logger --restart unless-stopped can-logger
```

## Home Assistant Integration

### Subscribe to Messages in Automations

```yaml
automation:
  - alias: "Alert on High Temperature"
    trigger:
      platform: mqtt
      topic: homeassistant/stiebel/can_raw/+/OUTSIDE_TEMP
    condition:
      - "{{ trigger.payload_json.value | float > 30 }}"
    action:
      service: notify.mobile_app
      data:
        message: "Outside temperature is {{ trigger.payload_json.value }}°C!"
```

### Create MQTT Sensors

```yaml
mqtt:
  sensor:
    - name: "Heat Pump Outside Temp"
      state_topic: "homeassistant/stiebel/can_raw/180/OUTSIDE_TEMP"
      value_template: "{{ value_json.value }}"
      unit_of_measurement: "°C"
      device_class: temperature
```

### Monitor All Parameters

```yaml
mqtt:
  sensor:
    - name: "Last CAN Message"
      state_topic: "homeassistant/stiebel/can_raw/#"
      value_template: "{{ value_json.name }}: {{ value_json.value }}"
```

## Database Queries

### PostgreSQL/MySQL Examples

```sql
-- Get recent messages
SELECT timestamp, addressee, name, value
FROM can_messages
ORDER BY timestamp DESC
LIMIT 100;

-- Temperature readings from last 24 hours
SELECT timestamp, name, value
FROM can_messages
WHERE name LIKE '%TEMP%'
  AND timestamp > NOW() - INTERVAL '24 hours'
ORDER BY timestamp DESC;

-- Message count by device
SELECT addressee, COUNT(*) as count
FROM can_messages
GROUP BY addressee;

-- Average outside temperature per hour
SELECT 
  DATE_TRUNC('hour', timestamp) as hour,
  AVG(CAST(value AS FLOAT)) as avg_temp
FROM can_messages
WHERE name = 'OUTSIDE_TEMP'
GROUP BY hour
ORDER BY hour DESC;

-- Find unknown parameters
SELECT DISTINCT name, elster_idx, COUNT(*) as occurrences
FROM can_messages
WHERE name LIKE '%UNKNOWN%' OR name LIKE '%INDEX_NOT_FOUND%'
GROUP BY name, elster_idx
ORDER BY occurrences DESC;
```

### InfluxDB Flux Examples

```flux
// Recent messages
from(bucket: "can_messages")
  |> range(start: -1h)
  |> filter(fn: (r) => r["_measurement"] == "can_message")

// Average temperature
from(bucket: "can_messages")
  |> range(start: -24h)
  |> filter(fn: (r) => r["name"] == "OUTSIDE_TEMP")
  |> aggregateWindow(every: 1h, fn: mean)
```

## Grafana Dashboard

Create a dashboard to visualize your CAN data:

1. **Add InfluxDB data source**
2. **Create panels for:**
   - Temperature trends
   - Message frequency
   - System status
   - Error counts

Example panel query (InfluxDB):
```flux
from(bucket: "can_messages")
  |> range(start: -7d)
  |> filter(fn: (r) => r["name"] =~ /TEMP/)
  |> aggregateWindow(every: 5m, fn: mean)
```

## Troubleshooting

### Messages Not Appearing

1. **Check MQTT broker:**
   ```bash
   mosquitto_sub -h 192.168.1.100 -t 'homeassistant/stiebel/can_raw/#' -u username -P password
   ```

2. **Check ESP32 logs:**
   - Look for "MQTT connected" message
   - Check for authentication errors

3. **Test MQTT publish:**
   ```bash
   mosquitto_pub -h 192.168.1.100 -t 'test' -m 'hello' -u username -P password
   ```

4. **Verify network connectivity:**
   ```bash
   ping 192.168.1.100
   telnet 192.168.1.100 1883
   ```

### Database Not Storing

1. **Check Python script logs:**
   ```bash
   python3 mqtt_to_database.py --config mqtt_logger_config.yaml
   ```

2. **Test database connection:**
   ```bash
   # PostgreSQL
   psql -h 192.168.1.200 -U dbuser -d stiebel_can
   
   # MySQL
   mysql -h 192.168.1.200 -u dbuser -p stiebel_can
   ```

3. **Verify tables created:**
   ```sql
   -- PostgreSQL/MySQL
   \dt  -- or SHOW TABLES;
   SELECT * FROM can_messages LIMIT 1;
   ```

### Performance Issues

**High message rate:**
- Use QoS 0 for MQTT (no guaranteed delivery)
- Batch database inserts
- Use InfluxDB for time-series data

**Network congestion:**
- Filter messages in ESPHome before publishing
- Reduce message frequency
- Use local MQTT broker

## Security Best Practices

1. **Use strong passwords** for MQTT and database
2. **Enable TLS/SSL** for MQTT (port 8883)
3. **Restrict network access** - use firewall rules
4. **Don't expose MQTT** to the internet
5. **Use Home Assistant's** built-in MQTT broker
6. **Store credentials** in secrets.yaml
7. **Regular backups** of database
8. **Monitor logs** for unauthorized access

## Performance

- **MQTT publish**: 2-5ms per message
- **Network bandwidth**: ~200 bytes/message
- **Database storage**: ~300 bytes/message
- **At 10 msg/sec**: ~2MB/hour, ~1.5GB/month

## Advantages Over SQLite

✅ **Network accessible** - Access from any device  
✅ **Centralized storage** - One database for multiple ESP32s  
✅ **Powerful queries** - Full SQL capabilities  
✅ **No ESP32 storage limits** - Store unlimited history  
✅ **Real-time access** - Query while logging  
✅ **Backup friendly** - Standard database backup tools  
✅ **Scalable** - Handle millions of messages  
✅ **Integration ready** - Works with all tools (Grafana, Jupyter, etc.)  

## Next Steps

1. Deploy ESPHome configuration
2. Verify messages in MQTT
3. Setup database
4. Run logger script
5. Create Grafana dashboard
6. Set up Home Assistant automations
7. Analyze your heat pump data!

## Support

- Check logs for detailed error messages
- Verify all connection settings
- Test components individually (MQTT, database, script)
- Review ESPHome documentation for MQTT

---

**Need Help?** Check the logs, test connections, and verify credentials first!

