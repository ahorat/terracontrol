#pragma once

#include <Arduino.h>
#include "ChannelConfig.h"
#include "Config.h"

// Minimal driver for the AT24C32 I2C EEPROM (4KB, 32-byte pages) that sits on
// the DS3231 RTC breakout, plus the fixed byte layout used to persist the
// 4 ChannelConfig structs.
//
// Layout:
//   offset 0..1   magic bytes 0xC0 0xFF
//   offset 2      layout version
//   offset 3..    CHANNEL_CONFIG_BYTES * CHANNEL_COUNT, one block per channel
class EepromStore {
public:
  explicit EepromStore(uint8_t i2cAddr = AT24C32_I2C_ADDR) : _addr(i2cAddr) {}

  bool begin();

  // Loads all channels from EEPROM. Returns false (and fills defaults) if no
  // valid magic/version was found, e.g. on a factory-fresh chip.
  bool loadAll(ChannelConfig configs[CHANNEL_COUNT]);

  // Persists a single channel immediately (used for "changes apply live").
  void saveChannel(uint8_t index, const ChannelConfig &cfg);

  // Persists all channels and (re-)writes the header.
  void saveAll(const ChannelConfig configs[CHANNEL_COUNT]);

private:
  uint8_t _addr;

  static const uint8_t PAGE_SIZE = 32;
  static const uint16_t HEADER_ADDR = 0;
  static const uint16_t HEADER_SIZE = 3;
  // Deliberately distinct from 0xFF, the byte value of an erased/blank EEPROM.
  static const uint8_t MAGIC_0 = 0x54; // 'T'
  static const uint8_t MAGIC_1 = 0x43; // 'C'
  static const uint8_t LAYOUT_VERSION = 1;

  uint16_t channelAddr(uint8_t index) const {
    return HEADER_ADDR + HEADER_SIZE + index * CHANNEL_CONFIG_BYTES;
  }

  void writeBytes(uint16_t memAddr, const uint8_t *data, size_t len);
  void readBytes(uint16_t memAddr, uint8_t *data, size_t len);

  static void serialize(const ChannelConfig &cfg, uint8_t out[CHANNEL_CONFIG_BYTES]);
  static void deserialize(const uint8_t in[CHANNEL_CONFIG_BYTES], ChannelConfig &cfg);
};
