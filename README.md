# ha-stiebel-control

ha-stiebel-control is a ESPhome / Home Assistant configuration to monitor & configure Stiebel Eltron Heating Pumps via a CAN Interface.
It requires setting up an ESP32 Microcontroller with a MCP2515 CAN-Tranceiver and some configuring in Home Assistant.
It is based on the great work of the Home Assistant community, especially the work of [roberreiters](https://community.home-assistant.io/t/configured-my-esphome-with-mcp2515-can-bus-for-stiebel-eltron-heating-pump/366053) and [Jürg Müller](http://juerg5524.ch/list_data.php).

## Installation

### ESPHome
* Set up a new ESPHome Project "heatingpump"
* Copy the Content of `heatingpump.yaml` to the new project
* copy the folder `stiebeltools`to your `/config/esphome` folder (full path should be `/config/esphome/stiebeltools`)
* Change the WiFi Credentials to yours
* Change the GPIO Pins under `spi` and `can` to your HW configuration
* You may want to check/change the CAN IDs of the Manager, Kessel, etc. In order to do so, you have to change them in two places:
  * `stiebeltools\heatingpump.h`:
    ```c
    static const CanMember CanMembers[] =
    {
    //  Name              CanId     ReadId          WriteId         ConfirmationID
      { "ESPCLIENT"     , 0x700,    {0x00, 0x00},   {0x00, 0x00},   {0xE2, 0x00}}, //The ESP Home Client, thus no valid read/write IDs
      { "KESSEL"        , 0x180,    {0x31, 0x00},   {0x30, 0x00},   {0x00, 0x00}},
      { "MANAGER"       , 0x480,    {0x91, 0x00},   {0x90, 0x00},   {0x00, 0x00}},
      { "HEIZMODUL"     , 0x500,    {0xA1, 0x00},   {0xA0, 0x00},   {0x00, 0x00}}
    };
    ```
  * `heatingpump.yaml`: Look for these blocks in the lower part of the file
    ```yaml
    #########################################
    #                                       #
    #   HEIZMODUL Nachrichten               #
    #                                       #
    #########################################
        - can_id: 0x500
          then:
            - lambda: |-
                unsigned short canId = 500;
    ```

### Home Assistant
#### Entities and Helpers
* place the file `packages/ha_stiebel_control.yaml` in your `/config/packages/` folder in Home Assistant.
* add the package folder to your `configuration.yaml` under `homeassistant` (if not already set up)
```yaml
homeassistant:
  packages: !include_dir_named packages
```
#### Dashboard
* Install the lovelace card [apexcharts-card](https://github.com/RomRider/apexcharts-card)
* Install the lovelace card [lovelace-mushroom](https://github.com/piitaya/lovelace-mushroom)
* Create a new Dashboard, switch to RAW mode and paste the content of `dashboard.yaml`. The result should look similar to this:
![Dashboard Screenshot](assets/img/dashboard.jpg "Dashboard Screenshot")

## Using

### CAN Message MQTT Logging (NEW)

The system now publishes all CAN messages to MQTT, allowing you to store them in **any database** on **any server** in your network. This is useful for:
- **Network-accessible storage** - Access from anywhere on your LAN
- **Any database** - PostgreSQL, MySQL, InfluxDB, or SQLite
- **Home Assistant integration** - Create automations based on CAN messages
- **Long-term monitoring** - Unlimited storage on your server
- **Real-time analysis** - Grafana dashboards, custom tools

**Quick Setup:**
1. Add MQTT configuration to your ESPHome YAML (see `mqtt_can_logger_addon.yaml`)
2. Deploy to ESP32 - **Done!** Messages now published to MQTT
3. (Optional) Run `mqtt_to_database.py` to store in database

**Example ESPHome Config:**
```yaml
mqtt:
  broker: 192.168.1.100  # Your Home Assistant IP
  username: !secret mqtt_username
  password: !secret mqtt_password
  id: mqtt_client

esphome:
  includes:
    - stiebeltools/CanMessageMqttLogger.h
```

**Store in Database (Optional):**
```bash
# Edit configuration
cp mqtt_logger_config.yaml my_config.yaml
nano my_config.yaml

# Install dependencies
pip install paho-mqtt pyyaml sqlalchemy psycopg2-binary

# Run logger
python3 mqtt_to_database.py --config my_config.yaml
```

**MQTT Topics:**
All messages published to: `homeassistant/stiebel/can_raw/<CAN_ID>/<PARAMETER_NAME>`

**Documentation:**
See `MQTT_CAN_LOGGER_GUIDE.md` for complete setup instructions, database options, Home Assistant integration, and Grafana dashboards.

## Contributing

Pull requests are welcome. For major changes, please open an issue first
to discuss what you would like to change.

Please make sure to update tests as appropriate.

## License

[GPLv3] (https://www.gnu.org/licenses/gpl-3.0.en.html)