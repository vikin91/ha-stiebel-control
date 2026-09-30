# WPF10M trial port on upstream v2.1.1

This branch starts at upstream `e8929d085f5321f1d384f50c97ffe67b01f652ab` and adds a separate configuration for the owner's ESP32-S3 plus MCP2515 installation. It does not replace upstream's `wpf10.yaml` profile. The fork's released `v0.1.6` code remains the working rollback reference.

## Trial configuration

- Entry point: [`esphome/heatingpump_wpf10m_local.yaml`](esphome/heatingpump_wpf10m_local.yaml).
- Hardware: ESP32-S3 DevKitC-1, ESP-IDF, MCP2515 at 20 kbps, SCK GPIO18, MOSI GPIO17, MISO GPIO16, CS GPIO4, client CAN ID `0x680`.
- Model package: [`wpf10m_local.yaml`](esphome/ha-stiebel-control/wpf10m_local.yaml) retains the S3 Home Assistant entity names, pump/FE7X comparison for storage setpoint, `+3 °C` storage actual correction, and the two error reads. Declared but unconnected entities remain declared.
- The request table retains upstream WPF10 probes and adds the requests from the working S3 configuration. Upstream's extra requests, derived sensors, writable controls, and COP logic are **not validated on this installation**. Inspect their behavior before relying on them.
- [`wpf10m_frame.h`](esphome/ha-stiebel-control/wpf10m_frame.h) handles the model-specific reply routing and passive manager `0x480` / Elster `0x005F` compressor indication. The raw examples are `a0 08 5f 00 00 00 00` (OFF) and `a0 08 5f 02 00 00 00` (ON). This is an observed command correlated with compressor operation, not an electrical measurement of the compressor. It also carries the fork's best-effort QoS-0 CAN MQTT diagnostic stream.
- The experimental `0x00F4` bedroom sensor and unverified table type changes are omitted. The inherited `0x02CA` and `0x01D6` names still need physical validation.
- Upstream's calculated `compressor_active` uses a different `VERDICHTER` signal and threshold. For this WPF10M installation, use the carried `COMPRESSOR_RUNNING` entity as the known signal until the two are compared on hardware.

The trial entry point expects the old ESPHome secrets `wifi_ssid`, `wifi_password`, `fallback_wifi_password`, `mqtt_username`, `mqtt_password`, and `ota_api_key`. Upstream's common package also refers to `mqtt_broker` and `api_encryption_key`; add them to `secrets.yaml` if ESPHome asks for them during package loading. The entry point overrides broker, API key, OTA password, Wi-Fi address, and fallback hotspot settings to match the released S3 configuration. Review the merged ESPHome config and OTA settings before flashing.

## Local validation

- Native tests: 429 assertions in 219 cases passed, including the compressor frames, sender-specific storage setpoint routing, signed temperatures, and bounded formatting.
- ESPHome 2026.9.1 accepted the merged configuration and compiled the ESP32-S3 firmware using **dummy CI secrets**. The generated binary is a build check, not a deployable image with the owner's credentials.
- The compiled image used 1,724,991 of 1,835,008 bytes of the default application flash partition (94.0%). The upstream feature set leaves limited space for more code on this partition.
- No firmware was installed on the heat pump. Upstream's extra CAN reads, calculated sensors, and writable controls still require on-device evaluation.

## Rollback

The original checkout at `/Users/prygiels/code/ha-stiebel-control` stays on `main` at `3d5c5ef555a29c6aefcabb141b7edb3c4b8593f6`; the annotated tag `v0.1.6` resolves to the same commit. The trial lives only on `codex/wpf10m-upstream-port` in a separate worktree. To return to the released firmware, build and install the original checkout's `s3.yaml` together with its `stiebeltools/` directory. No release tag or original file needs to be moved.

The trial branch is for source and firmware comparison. A successful compile does not confirm that the extra upstream values or controls are correct for this heat pump. Preserve a copy of the working `v0.1.6` ESPHome files on Home Assistant while testing.
