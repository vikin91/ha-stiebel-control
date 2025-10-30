# MQTT CAN Logger - Quick Start (2 Minutes)

## What You Get
✅ All CAN messages published to MQTT (JSON format)  
✅ Network accessible from any device  
✅ Store in ANY database (PostgreSQL, MySQL, SQLite, InfluxDB)  
✅ Home Assistant integration ready  
✅ Grafana dashboard support  
✅ No ESP32 storage limits  
✅ **Works with existing MQTT server**  
✅ **CAN processing unaffected if MQTT fails**  
✅ **Auto-reconnects when MQTT available**  

## Setup (Add to ESPHome YAML)

### 1. Add MQTT Configuration
```yaml
mqtt:
  broker: 192.168.1.100  # Your Home Assistant or MQTT broker
  port: 1883
  username: !secret mqtt_username
  password: !secret mqtt_password
  id: mqtt_client
```

### 2. Add Include File
```yaml
esphome:
  includes:
    - stiebeltools/CanMessageMqttLogger.h
```

### 3. Deploy to ESP32
```bash
esphome run your_config.yaml
```

**Done!** All CAN messages are now published to MQTT.

## MQTT Topics

Messages published to:
```
homeassistant/stiebel/can_raw/<CAN_ID>/<PARAMETER_NAME>
```

Example topics:
- `homeassistant/stiebel/can_raw/180/OUTSIDE_TEMP`
- `homeassistant/stiebel/can_raw/480/ERROR_MESSAGE`

## Message Format (JSON)

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

## View Messages

### Using mosquitto_sub
```bash
# View all CAN messages
mosquitto_sub -h 192.168.1.100 -t 'homeassistant/stiebel/can_raw/#' -u username -P password

# View specific parameter
mosquitto_sub -h 192.168.1.100 -t 'homeassistant/stiebel/can_raw/+/OUTSIDE_TEMP'
```

### Using MQTT Explorer (GUI)
1. Download from https://mqtt-explorer.com/
2. Connect to your broker
3. Browse topics visually

## Store in Database (Optional)

### 1. Edit Configuration
```bash
cp mqtt_logger_config.yaml my_config.yaml
nano my_config.yaml
```

### 2. Install Dependencies

**Debian/Ubuntu (Recommended):**
```bash
sudo apt update
sudo apt install python3-paho-mqtt python3-yaml python3-sqlalchemy python3-psycopg2
```

**Other Linux/macOS (using virtual environment):**
```bash
python3 -m venv venv
source venv/bin/activate
pip install paho-mqtt pyyaml sqlalchemy psycopg2-binary
```

### 3. Run Logger
```bash
python3 mqtt2db/mqtt_to_database.py --config mqtt_logger_config.yaml
```

## Database Options

- **PostgreSQL** - Production-ready, powerful
- **MySQL/MariaDB** - Popular, well-supported
- **SQLite** - Simple, single file
- **InfluxDB** - Time-series, perfect for Grafana

## Home Assistant Automations

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
        message: "Outside temp is {{ trigger.payload_json.value }}°C!"
```

## Files

- **`MQTT_CAN_LOGGER_GUIDE.md`** - Complete documentation
- **`mqtt_can_logger_addon.yaml`** - Full ESPHome config example
- **`mqtt_logger_config.yaml`** - Database logger configuration
- **`mqtt_to_database.py`** - Python script to store in database
- **`analyze_can_database.py`** - Database analysis tool

## Why MQTT?

✅ **Network accessible** - Not limited to ESP32  
✅ **Any database** - PostgreSQL, MySQL, InfluxDB, etc.  
✅ **Home Assistant ready** - Native MQTT support  
✅ **Real-time** - Subscribe from multiple clients  
✅ **Flexible** - Store, analyze, visualize anywhere  
✅ **Scalable** - Handle unlimited messages  

## Connection Safety

**The MQTT logger will NEVER crash your CAN processing:**
- ✅ MQTT not configured → Works fine, logs warning once
- ✅ MQTT server down → CAN processing continues, auto-reconnects
- ✅ Wrong credentials → Logs error, CAN processing continues
- ✅ Network issues → Graceful degradation, auto-recovery
- ✅ No log spam → Smart rate-limited logging

See `MQTT_SAFETY.md` for complete details.

## Troubleshooting

### No messages appearing?
```bash
# Test MQTT broker
mosquitto_sub -h 192.168.1.100 -t '#' -u username -P password

# Check ESP32 logs for "MQTT connected"
```

**Expected Log Messages:**
- Connected: `[I][MqttLogger] MQTT connected - publishing CAN messages`
- Disconnected: `[W][MqttLogger] MQTT disconnected - CAN messages not being published`
- Not configured: `[W][MqttLogger] MQTT not enabled in ESPHome configuration`

### Can't connect to MQTT?
1. Verify broker IP address
2. Check username/password
3. Ensure port 1883 is open
4. Test with mosquitto_pub
5. Check ESP32 WiFi connection

### Database not storing?
1. Check Python script logs
2. Verify database credentials
3. Test database connection separately

---

**Need More Details?** 
- Connection safety: `MQTT_SAFETY.md`
- Full guide: `MQTT_CAN_LOGGER_GUIDE.md`

