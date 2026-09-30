# ha-stiebel-control

ESPHome and Home Assistant configuration for monitoring a Stiebel Eltron heat pump over CAN with an ESP32 and MCP2515 transceiver. This project builds on work by the [Home Assistant community](https://community.home-assistant.io/t/configured-my-esphome-with-mcp2515-can-bus-for-stiebel-eltron-heating-pump/366053) and [Jürg Müller](http://juerg5524.ch/list_data.php).

## ESPHome configuration

- [`s3.yaml`](s3.yaml) is the ESP32-S3 WPF10M configuration used for the v0.1.5 baseline. It is the configuration currently tested on the heat pump.
- [`heatingpump_en_prod.yaml`](heatingpump_en_prod.yaml) is an older ESP32 configuration for different hardware. Its pins, network settings, and sensor set differ from `s3.yaml`.
- The Elster table includes entries for several models. Many values have not been verified on WPF10M. Confirm an index against observed CAN traffic before using it to control the heat pump.

To install the S3 configuration in ESPHome Device Builder:

1. Put `s3.yaml` and the `stiebeltools/` directory in the ESPHome configuration directory (usually `/config/esphome` in the app; the same directory is available as `/homeassistant/esphome` through the Home Assistant SSH app).
2. Add `wifi_ssid`, `wifi_password`, `fallback_wifi_password`, `mqtt_username`, `mqtt_password`, and `ota_api_key` to your ESPHome `secrets.yaml`. The last key is used by the Home Assistant API encryption setting despite its name.
3. Check the SPI pins, MCP2515 chip-select pin, CAN speed, broker address, and CAN ID filters against your hardware. The receive lambdas pass ESPHome's actual `can_id` to the decoder.
4. Validate, compile, and install with ESPHome Device Builder. Copying source files alone does not update the firmware on the ESP.

The read/write CAN members are defined in [`stiebeltools/heatingpump.h`](stiebeltools/heatingpump.h). The CAN receive handlers and Home Assistant sensor routing are in `s3.yaml`.

## Home Assistant

Copy [`packages/ha_stiebel_control.yaml`](packages/ha_stiebel_control.yaml) into `/config/packages/` if you use the included helpers, and enable packages in `configuration.yaml`:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

The optional [`dashboard.yaml`](dashboard.yaml) uses the [ApexCharts](https://github.com/RomRider/apexcharts-card) and [Mushroom](https://github.com/piitaya/lovelace-mushroom) custom cards.

## CAN messages over MQTT

The S3 configuration publishes decoded CAN messages to `homeassistant/stiebel/can_raw/<CAN_ID_HEX>/<PARAMETER_NAME>` as JSON. Publishing is best effort with QoS 0: messages received while MQTT is disconnected or when publish fails are dropped. This stream is useful for diagnostics, but it is not a lossless record of CAN traffic.

For optional database storage, copy [`mqtt_logger_config.yaml`](mqtt_logger_config.yaml) to `mqtt_logger_config.local.yaml`, fill in your own broker and database credentials, and run:

```sh
python3 -m pip install paho-mqtt pyyaml sqlalchemy psycopg2-binary
python3 mqtt2db/mqtt_to_database.py --config mqtt_logger_config.local.yaml
```

The local configuration filename is ignored by Git. Install the database driver for the database you choose; the command above includes the PostgreSQL driver.

## License

[GPLv3](LICENSE)
