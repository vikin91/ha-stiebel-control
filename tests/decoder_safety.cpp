#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct TestCanbus {
  bool send_data(uint32_t, bool, const std::vector<uint8_t> &) { return true; }
};

TestCanbus test_canbus;
unsigned long millis() { return 0; }

#define id(name) test_canbus
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#include "../stiebeltools/heatingpump.h"

int main() {
  char value[32];
  SetValueType(value, sizeof(value), et_time_domain, 0x8080);
  assert(std::strcmp(value, "not used time domain") == 0);
  SetValueType(value, sizeof(value), et_err_nr, 0x0005);
  assert(std::strcmp(value, "Verdampferfuehler") == 0);

  struct {
    char value[8];
    char guard[4];
  } small_buffer = {{}, {'X', 'X', 'X', 'X'}};
  SetValueType(small_buffer.value, sizeof(small_buffer.value), et_time_domain, 0x8080);
  assert(std::strcmp(small_buffer.value, "not use") == 0);
  assert(std::memcmp(small_buffer.guard, "XXXX", sizeof(small_buffer.guard)) == 0);

  std::string signal_value;
  const auto *temperature = processCanMessage(
      0x180, signal_value, {0xa0, 0x00, 0x0c, 0x00, 0xe8, 0x00, 0x00});
  assert(temperature->Index == 0x000c);
  assert(signal_value == "23.2");

  const auto *mode = processCanMessage(
      0x480, signal_value, {0xa0, 0x00, 0xfa, 0x01, 0x12, 0x02, 0x00});
  assert(mode->Type == et_betriebsart);
  assert(signal_value == "Automatik");

  const auto *schedule = processCanMessage(
      0x480, signal_value, {0xa0, 0x00, 0xfa, 0x14, 0x10, 0x80, 0x80});
  assert(schedule->Type == et_time_domain);
  assert(signal_value == "not used time domain");
}
