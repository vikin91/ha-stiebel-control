#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct TestCanbus {
  bool send_data(uint32_t, bool, const std::vector<uint8_t> &) { return true; }
  bool is_connected() const { return true; }
  bool publish(const std::string &new_topic, const std::string &new_payload, int, bool) {
    topic = new_topic;
    payload = new_payload;
    ++publish_count;
    return true;
  }
  std::string topic;
  std::string payload;
  unsigned publish_count = 0;
};

TestCanbus test_canbus;
unsigned long millis() { return 0; }

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void test_log(const char *, const char *, ...) {}

#define USE_MQTT
#define id(name) test_canbus
#define ESP_LOGI(...) test_log(__VA_ARGS__)
#define ESP_LOGW(...) test_log(__VA_ARGS__)
#define ESP_LOGD(...) test_log(__VA_ARGS__)
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
  assert(test_canbus.topic == "homeassistant/stiebel/can_raw/180/OUTSIDE_TEMP");
  assert(test_canbus.payload.find("\"sender_can_id\":384") != std::string::npos);
  assert(test_canbus.payload.find("\"sender_name\":\"PUMP\"") != std::string::npos);
  assert(std::strcmp(getCanMemberName(180), "UNKNOWN") == 0);
  assert(rawBytesToHex({0x00, 0xa0, 0xff}) == "00 a0 ff");

  const unsigned published_before_oversized_value = test_canbus.publish_count;
  publishCanMessageToMqtt(0x180, {0xa0, 0x00, 0x0c, 0x00, 0xe8, 0x00, 0x00},
                          temperature, std::string(600, 'x'), 232);
  assert(test_canbus.publish_count == published_before_oversized_value);

  const auto *room_multiplexed = processCanMessage(
      0x480, signal_value, {0xa0, 0x73, 0xf4, 0x02, 0x4c, 0x00, 0x01});
  assert(room_multiplexed->Index == 0x00f4);
  assert(room_multiplexed->Type == et_default);
  assert(std::strcmp(room_multiplexed->EnglishName, "ROOM_MULTIPLEXED_RAW") == 0);
  assert(signal_value == "588");

  const auto *mode = processCanMessage(
      0x480, signal_value, {0xa0, 0x00, 0xfa, 0x01, 0x12, 0x02, 0x00});
  assert(mode->Type == et_betriebsart);
  assert(signal_value == "Automatik");

  const auto *schedule = processCanMessage(
      0x480, signal_value, {0xa0, 0x00, 0xfa, 0x14, 0x10, 0x80, 0x80});
  assert(schedule->Type == et_time_domain);
  assert(signal_value == "not used time domain");
}
