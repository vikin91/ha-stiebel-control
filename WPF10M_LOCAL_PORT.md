# WPF10M trial port on upstream v2.1.1

This branch starts at upstream `e8929d085f5321f1d384f50c97ffe67b01f652ab` and adds a separate configuration for the owner's ESP32-S3 plus MCP2515 installation. It does not replace upstream's `wpf10.yaml` profile. The fork's released `v0.1.6` code remains the working rollback reference.

## Trial configuration

- Entry point: [`esphome/heatingpump_wpf10m_local.yaml`](esphome/heatingpump_wpf10m_local.yaml).
- Hardware: ESP32-S3 DevKitC-1, ESP-IDF, MCP2515 at 20 kbps, SCK GPIO18, MOSI GPIO17, MISO GPIO16, CS GPIO4, client CAN ID `0x680`.
- Model package: [`wpf10m_local.yaml`](esphome/ha-stiebel-control/wpf10m_local.yaml) retains the S3 Home Assistant entity names, pump/FE7X comparison for storage setpoint, `+3 °C` storage actual correction, and the two error reads. Declared but unconnected entities remain declared.
- The first-test request table uses the deployed S3 routes and intervals. Three unrelated upstream template polls (`PROGRAMMSCHALTER`, `SOMMERBETRIEB`, `WW_ECO`) are removed from this trial profile. The upstream WPF10 probes and universal requests can be enabled later with `-DWPF10M_UPSTREAM_PROBES` after comparing the known sensors. Upstream's derived sensors and COP logic are **not validated on this installation** and are inactive in the sensor-focused trial.
- [`wpf10m_frame.h`](esphome/ha-stiebel-control/wpf10m_frame.h) handles the model-specific reply routing and passive manager `0x480` / Elster `0x005F` compressor indication. The raw examples are `a0 08 5f 00 00 00 00` (OFF) and `a0 08 5f 02 00 00 00` (ON). This is an observed command correlated with compressor operation, not an electrical measurement of the compressor. The fork's best-effort QoS-0 CAN MQTT diagnostic stream is present as an opt-in (`-DWPF10M_RAW_MQTT`) and is off for the first test.
- The experimental `0x00F4` bedroom sensor and unverified table type changes are omitted. The inherited `0x02CA` and `0x01D6` names still need physical validation.
- `-DWPF10M_READ_ONLY` blocks every call to the upstream CAN parameter-write function. `-DWPF10M_SENSOR_ONLY` also removes upstream MQTT command subscriptions, skips generic CAN-to-MQTT publishing and calculated sensors, and stops automatic custom discovery. The three inactive upstream time/discovery buttons are removed from this profile; other shared device-management buttons may still be visible, but they cannot send CAN parameter writes. Scheduled *read* requests still transmit CAN frames.
- Upstream's calculated `compressor_active` uses a different `VERDICHTER` signal and threshold. For this WPF10M installation, use the carried `COMPRESSOR_RUNNING` entity as the known signal until the two are compared on hardware.

The trial entry point expects the old ESPHome secrets `wifi_ssid`, `wifi_password`, `fallback_wifi_password`, `mqtt_username`, `mqtt_password`, and `ota_api_key`. Upstream's common package also refers to `mqtt_broker` and `api_encryption_key`; add those two keys to the Home Assistant `secrets.yaml` if absent. Give `api_encryption_key` the same value already used by `ota_api_key`; the trial entry point overrides it with that existing key. The entry point also overrides broker, OTA password, Wi-Fi address, and fallback hotspot settings to match the released S3 configuration. The native ESPHome API supplies the carried S3 sensor entities to Home Assistant. The shared upstream package still configures an MQTT client, but the sensor-focused trial disables its custom CAN discovery, calculated MQTT publications, and command handlers. Native ESPHome MQTT discovery remains off; ESPHome may still publish the carried template sensors to MQTT state topics. The fork's raw CAN MQTT diagnostic stream is off.

## First hardware test on Home Assistant

1. In ESPHome Device Builder, compile/download a rollback image from the current `s3.yaml` while the original `stiebeltools/` directory and real `secrets.yaml` are still in place. Keep a separate copy of the working YAML and source directory. The Git tag preserves source, but a prebuilt image makes device recovery quicker.
2. Copy **only** the trial entry point and `ha-stiebel-control/` source directory into `/homeassistant/esphome/` on Home Assistant. Leave `s3.yaml`, `stiebeltools/`, and `secrets.yaml` in place. From the computer with this worktree, a suitable command is:

   ```sh
   scp -r /Users/prygiels/.codex/worktrees/wpf10m-upstream-port/ha-stiebel-control/esphome/heatingpump_wpf10m_local.yaml /Users/prygiels/.codex/worktrees/wpf10m-upstream-port/ha-stiebel-control/esphome/ha-stiebel-control root@192.168.178.11:/homeassistant/esphome/
   ```

3. Check the two extra upstream secret names above. Open `heatingpump_wpf10m_local.yaml` in Device Builder, validate and compile it **on Home Assistant with real secrets**. Do not install the locally compiled dummy-credential image. This creates a second configuration card for the same ESP device name `heatingpump`; choose the trial card deliberately.
4. Install the trial from that card over OTA. It retains the S3 API key and encrypted OTA setting. Observe the device logs and Home Assistant entities. The trial suppresses the upstream per-frame German INFO messages and logs mapped WPF10M values with English labels. It also logs each `0x01D4` source-actual poll so the next capture can show whether a response follows; some upstream entity names and warnings remain German. Compare the plausible readings (`OUTSIDE_TEMP`, return flow, HK1 flow setpoint, storage actual with its +3 °C correction) and watch `COMPRESSOR_RUNNING` through one actual OFF/ON cycle. The storage setpoints, error numbers, `0x02CA`, `0x01D6`, and extra upstream MQTT entities remain unverified; some produced implausible values on v0.1.6. The first-test build blocks CAN parameter writes, including from MQTT commands and upstream time/date buttons.
5. To return, select the original `s3.yaml` card in Device Builder and install its firmware over OTA. Its source and credentials stay in place. If OTA is unavailable, use the saved rollback image over USB.

Old custom MQTT entities may remain visible in Home Assistant because upstream retained their discovery messages in the broker. This firmware stops publishing new custom discovery messages; it does not erase existing broker topics or HA entities. Leave those entries alone during the hardware comparison.

Only after the known readings agree should you consider enabling `WPF10M_UPSTREAM_PROBES` and testing the extra upstream requests. Leave `WPF10M_READ_ONLY` and `WPF10M_SENSOR_ONLY` enabled while learning what the new controls do. The raw MQTT stream can stay off unless another discovery session is needed.

## Local validation

- Native tests: 429 assertions in 219 cases passed, including the compressor frames, sender-specific storage setpoint routing, signed temperatures, and bounded formatting.
- ESPHome 2026.9.1 validated and compiled the sensor-focused ESP32-S3 firmware using **dummy CI secrets**. The image uses 1,330,623 of 1,835,008 bytes of the application partition (72.5%) and 111,707 of 341,760 bytes of RAM (32.7%). It has not been installed; the dummy-credential image is not deployable. One pre-existing upstream time-format truncation warning remains in `ha-stiebel-control.h`.
- The owner installed the previous trial build on the heat pump. Its 3.5-minute log shows `OUTSIDE_TEMP`, `RETURN_FLOW_INTERNAL_TEMP`, `STORAGE_TANK_INTERNAL_TEMP`, `FLOW_SETPOINT_TEMP_HK1`, and `AUXILIARY_BOILER_SETPOINT` publishing, plus `COMPRESSOR_RUNNING = OFF`. `SOURCE_ACTUAL` was requested near the end but no response appears before the capture stops. Home Assistant subsequently showed `binary_sensor.heatingpump_compressor_running = off`, confirming that the carried sensor reached HA. Other HA entities and the ON compressor transition remain to be checked. The revised trial with English logs has not been installed.

## Rollback

The original checkout at `/Users/prygiels/code/ha-stiebel-control` stays on `main` at `3d5c5ef555a29c6aefcabb141b7edb3c4b8593f6`; the annotated tag `v0.1.6` resolves to the same commit. The trial lives only on `codex/wpf10m-upstream-port` in a separate worktree. To return to the released firmware, build and install the original checkout's `s3.yaml` together with its `stiebeltools/` directory. No release tag or original file needs to be moved.

The trial branch is for source and firmware comparison. A successful compile does not confirm that the extra upstream values or controls are correct for this heat pump. Preserve a copy of the working `v0.1.6` ESPHome files on Home Assistant while testing.
