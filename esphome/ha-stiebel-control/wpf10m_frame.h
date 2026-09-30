#ifndef WPF10M_FRAME_H
#define WPF10M_FRAME_H

#include "ha-stiebel-control.h"
#include "wpf10m_protocol.h"

// Preserve the diagnostic MQTT names used by the deployed fork for known
// WPF10M values. All other indices keep the upstream table name.
inline const char *wpf10mDiagnosticName(uint16_t index, const ElsterIndex *ei) {
  switch (index) {
    case 0x0001: return "ERROR_MESSAGE";
    case 0x0003: return "STORAGE_TANK_SETPOINT_TEMP";
    case 0x0004: return "FLOW_SETPOINT_TEMP";
    case 0x000C: return "OUTSIDE_TEMP";
    case 0x000E: return "STORAGE_TANK_INTERNAL_TEMP";
    case 0x0016: return "RETURN_FLOW_INTERNAL_TEMP";
    case 0x005F: return "COMPRESSOR_STATUS";
    case 0x01D4: return "SOURCE_ACTUAL";
    case 0x01D5: return "BUFFER_SETPOINT";
    case 0x01D6: return "HEATING_RETURN_ACTUAL";
    case 0x01D7: return "AUXILIARY_BOILER_SETPOINT";
    case 0x02CA: return "FLOW_INTERNAL_TEMP_HK1";
    default: return ei->Name;
  }
}

inline void wpf10mJsonEscape(const char *source, char *dest, size_t capacity) {
  if (capacity == 0) return;
  size_t written = 0;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(source);
       *p && written + 1 < capacity; ++p) {
    if (*p == '"' || *p == '\\') {
      if (written + 2 >= capacity) break;
      dest[written++] = '\\';
      dest[written++] = static_cast<char>(*p);
    } else if (*p < 0x20) {
      dest[written++] = ' ';
    } else {
      dest[written++] = static_cast<char>(*p);
    }
  }
  dest[written] = '\0';
}

inline void publishWpf10mRawFrame(uint32_t can_id, const std::vector<uint8_t> &bytes,
                                  const Wpf10mValue &reading) {
#ifdef USE_MQTT
  if (!id(mqtt_client).is_connected()) return;

  const ElsterIndex *ei = GetElsterIndex(reading.index);
  const char *name = wpf10mDiagnosticName(reading.index, ei);
  const ElsterType type = reading.index == 0x005F ? et_little_endian :
                          reading.index == 0x02CA ? et_dec_val :
                          static_cast<ElsterType>(ei->Type);
  char decoded[32];
  SetValueType(decoded, sizeof(decoded), type, static_cast<uint16_t>(reading.raw));

  char raw_hex[32] = "";
  size_t used = 0;
  for (size_t i = 0; i < bytes.size(); ++i) {
    const int count = snprintf(raw_hex + used, sizeof(raw_hex) - used,
                               "%s%02x", i ? " " : "", static_cast<unsigned>(bytes[i]));
    if (count < 0 || static_cast<size_t>(count) >= sizeof(raw_hex) - used) return;
    used += static_cast<size_t>(count);
  }

  char safe_name[128];
  char safe_value[64];
  wpf10mJsonEscape(name, safe_name, sizeof(safe_name));
  wpf10mJsonEscape(decoded, safe_value, sizeof(safe_value));

  char topic[128];
  const int topic_len = snprintf(topic, sizeof(topic),
                                 "homeassistant/stiebel/can_raw/%03x/%s",
                                 static_cast<unsigned>(can_id), name);
  if (topic_len < 0 || static_cast<size_t>(topic_len) >= sizeof(topic)) return;

  const CanMember &sender = lookupCanMember(can_id);
  char json[512];
  const int json_len = snprintf(json, sizeof(json),
    "{\"timestamp\":%lu,\"sender_can_id\":%u,\"sender_name\":\"%s\","
    "\"elster_idx\":%u,\"name\":\"%s\",\"value\":\"%s\",\"type\":\"%s\","
    "\"raw_hex\":\"%s\",\"raw_value\":%d}",
    static_cast<unsigned long>(millis()), static_cast<unsigned>(can_id), sender.Name,
    static_cast<unsigned>(reading.index), safe_name, safe_value,
    ElsterTypeToName(type), raw_hex, static_cast<int>(reading.raw));
  if (json_len < 0 || static_cast<size_t>(json_len) >= sizeof(json)) return;

  id(mqtt_client).publish(std::string(topic), std::string(json), 0, false);
#else
  (void)can_id;
  (void)bytes;
  (void)reading;
#endif
}

inline void processWpf10mFrame(uint32_t can_id, const std::vector<uint8_t> &bytes) {
  Wpf10mValue reading;
  if (!decodeWpf10mValue(bytes, reading)) return;
  const float tenths = wpf10mTenths(reading.raw);

  if (can_id == 0x180) {
    switch (reading.index) {
      case 0x0001: id(ERROR_MESSAGE).publish_state(reading.raw); break;
      case 0x0003: id(STORAGE_TANK_SETPOINT_TEMP_PUMP).publish_state(tenths); break;
      case 0x000C: id(OUTSIDE_TEMP).publish_state(tenths); break;
      case 0x000E: id(STORAGE_TANK_INTERNAL_TEMP).publish_state(tenths + 3.0f); break;
      case 0x0016: id(RETURN_FLOW_INTERNAL_TEMP).publish_state(tenths); break;
      case 0x01D4: id(SOURCE_ACTUAL).publish_state(tenths); break;
      case 0x01D5: id(BUFFER_SETPOINT).publish_state(tenths); break;
      case 0x01D6: id(HEATING_RETURN_ACTUAL).publish_state(tenths); break;
      case 0x01D7: id(AUXILIARY_BOILER_SETPOINT).publish_state(tenths); break;
      case 0x02CA: id(FLOW_INTERNAL_TEMP_HK1).publish_state(tenths); break;
      default: break;
    }
  } else if (can_id == 0x301) {
    switch (reading.index) {
      case 0x0003: id(STORAGE_TANK_SETPOINT_TEMP).publish_state(tenths); break;
      case 0x0004: id(FLOW_SETPOINT_TEMP_HK1).publish_state(tenths); break;
      default: break;
    }
  } else if (can_id == 0x480) {
    switch (reading.index) {
      case 0x0001: id(ERROR_MESSAGE_MANAGER).publish_state(reading.raw); break;
      case 0x005F:
        // Manager command observed as 00 00 (OFF) and 02 00 (ON).
        id(COMPRESSOR_RUNNING).publish_state(wpf10mCompressorRunning(reading));
        break;
      default: break;
    }
  }

  // Same best-effort, QoS-0 diagnostic topic as the working v0.1.6 firmware.
  publishWpf10mRawFrame(can_id, bytes, reading);
}

#endif
