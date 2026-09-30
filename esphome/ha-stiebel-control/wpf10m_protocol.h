#ifndef WPF10M_PROTOCOL_H
#define WPF10M_PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <vector>

struct Wpf10mValue {
  uint16_t index;
  int16_t raw;
};

// Elster short frames use byte 2 as index; extended frames use 0xFA and
// bytes 3-4 as index. Both carry a signed, big-endian 16-bit value.
inline bool decodeWpf10mValue(const std::vector<uint8_t> &bytes, Wpf10mValue &out) {
  if (bytes.size() < 7) return false;
  const bool extended = bytes[2] == 0xFA;
  const size_t value_offset = extended ? 5 : 3;
  out.index = extended ? static_cast<uint16_t>((bytes[3] << 8) | bytes[4]) : bytes[2];
  out.raw = static_cast<int16_t>((bytes[value_offset] << 8) | bytes[value_offset + 1]);
  return true;
}

inline bool wpf10mCompressorRunning(const Wpf10mValue &value) {
  return value.index == 0x005F && value.raw > 0;
}

inline float wpf10mTenths(int16_t raw) {
  return raw / 10.0f;
}

#endif
