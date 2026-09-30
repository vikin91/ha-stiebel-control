#include "catch2/catch_amalgamated.hpp"
#include "../esphome/ha-stiebel-control/wpf10m_protocol.h"

TEST_CASE("WPF10M manager index 0x005F tracks the observed compressor states", "[wpf10m]") {
    Wpf10mValue reading{};
    REQUIRE(decodeWpf10mValue({0xA0, 0x08, 0x5F, 0x00, 0x00, 0x00, 0x00}, reading));
    CHECK(reading.index == 0x005F);
    CHECK(reading.raw == 0);
    CHECK_FALSE(wpf10mCompressorRunning(reading));

    REQUIRE(decodeWpf10mValue({0xA0, 0x08, 0x5F, 0x02, 0x00, 0x00, 0x00}, reading));
    CHECK(reading.raw == 512);
    CHECK(wpf10mCompressorRunning(reading));
}

TEST_CASE("WPF10M extended indices and negative tenths decode correctly", "[wpf10m]") {
    Wpf10mValue reading{};
    REQUIRE(decodeWpf10mValue({0x31, 0x00, 0xFA, 0x01, 0xD4, 0xFF, 0xCE}, reading));
    CHECK(reading.index == 0x01D4);
    CHECK(reading.raw == -50);
    CHECK(wpf10mTenths(reading.raw) == -5.0f);
    CHECK_FALSE(wpf10mCompressorRunning(reading));
}

TEST_CASE("WPF10M decoder rejects incomplete CAN values", "[wpf10m]") {
    Wpf10mValue reading{};
    CHECK_FALSE(decodeWpf10mValue({0xA0, 0x08, 0x5F, 0x02}, reading));
}
