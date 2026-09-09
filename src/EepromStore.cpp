#include "EepromStore.h"
#include <Wire.h>

bool EepromStore::begin() {
  // Wire.begin() is called once centrally in main.cpp (shared with RTC).
  return true;
}

void EepromStore::writeBytes(uint16_t memAddr, const uint8_t *data, size_t len) {
  size_t written = 0;
  while (written < len) {
    // Never write across a page boundary in one I2C transaction.
    uint16_t addr = memAddr + written;
    uint8_t spaceInPage = PAGE_SIZE - (addr % PAGE_SIZE);
    size_t chunk = min((size_t)spaceInPage, len - written);

    Wire.beginTransmission(_addr);
    Wire.write((uint8_t)(addr >> 8));
    Wire.write((uint8_t)(addr & 0xFF));
    Wire.write(data + written, chunk);
    Wire.endTransmission();

    delay(5); // AT24C32 internal write cycle time
    written += chunk;
  }
}

void EepromStore::readBytes(uint16_t memAddr, uint8_t *data, size_t len) {
  Wire.beginTransmission(_addr);
  Wire.write((uint8_t)(memAddr >> 8));
  Wire.write((uint8_t)(memAddr & 0xFF));
  Wire.endTransmission(false);

  size_t received = 0;
  Wire.requestFrom((int)_addr, (int)len);
  while (received < len && Wire.available()) {
    data[received++] = (uint8_t)Wire.read();
  }
}

void EepromStore::serialize(const ChannelConfig &cfg, uint8_t out[CHANNEL_CONFIG_BYTES]) {
  size_t p = 0;
  out[p++] = (uint8_t)cfg.mode;
  out[p++] = cfg.weekdayMask;
  out[p++] = cfg.windowEnabled ? 1 : 0;
  out[p++] = (uint8_t)(cfg.windowStartMin & 0xFF);
  out[p++] = (uint8_t)(cfg.windowStartMin >> 8);
  out[p++] = (uint8_t)(cfg.windowEndMin & 0xFF);
  out[p++] = (uint8_t)(cfg.windowEndMin >> 8);
  out[p++] = (uint8_t)(cfg.intervalMinutes & 0xFF);
  out[p++] = (uint8_t)(cfg.intervalMinutes >> 8);
  out[p++] = (uint8_t)(cfg.onSeconds & 0xFF);
  out[p++] = (uint8_t)(cfg.onSeconds >> 8);
  out[p++] = (uint8_t)(cfg.fixedOnMin & 0xFF);
  out[p++] = (uint8_t)(cfg.fixedOnMin >> 8);
  out[p++] = (uint8_t)(cfg.fixedOffMin & 0xFF);
  out[p++] = (uint8_t)(cfg.fixedOffMin >> 8);
  out[p++] = (uint8_t)((uint16_t)cfg.sunriseOffsetMin & 0xFF);
  out[p++] = (uint8_t)((uint16_t)cfg.sunriseOffsetMin >> 8);
  out[p++] = (uint8_t)((uint16_t)cfg.sunsetOffsetMin & 0xFF);
  out[p++] = (uint8_t)((uint16_t)cfg.sunsetOffsetMin >> 8);
  out[p++] = cfg.onAtSunrise ? 1 : 0;
  // p == CHANNEL_CONFIG_BYTES
}

void EepromStore::deserialize(const uint8_t in[CHANNEL_CONFIG_BYTES], ChannelConfig &cfg) {
  size_t p = 0;
  cfg.mode = (ChannelMode)in[p++];
  cfg.weekdayMask = in[p++];
  cfg.windowEnabled = in[p++] != 0;
  cfg.windowStartMin = (uint16_t)in[p] | ((uint16_t)in[p + 1] << 8); p += 2;
  cfg.windowEndMin = (uint16_t)in[p] | ((uint16_t)in[p + 1] << 8); p += 2;
  cfg.intervalMinutes = (uint16_t)in[p] | ((uint16_t)in[p + 1] << 8); p += 2;
  cfg.onSeconds = (uint16_t)in[p] | ((uint16_t)in[p + 1] << 8); p += 2;
  cfg.fixedOnMin = (uint16_t)in[p] | ((uint16_t)in[p + 1] << 8); p += 2;
  cfg.fixedOffMin = (uint16_t)in[p] | ((uint16_t)in[p + 1] << 8); p += 2;
  cfg.sunriseOffsetMin = (int16_t)((uint16_t)in[p] | ((uint16_t)in[p + 1] << 8)); p += 2;
  cfg.sunsetOffsetMin = (int16_t)((uint16_t)in[p] | ((uint16_t)in[p + 1] << 8)); p += 2;
  cfg.onAtSunrise = in[p++] != 0;
}

bool EepromStore::loadAll(ChannelConfig configs[CHANNEL_COUNT]) {
  uint8_t header[HEADER_SIZE];
  readBytes(HEADER_ADDR, header, HEADER_SIZE);

  if (header[0] != MAGIC_0 || header[1] != MAGIC_1 || header[2] != LAYOUT_VERSION) {
    // Factory-fresh or incompatible layout: fall back to defaults and persist them.
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
      configs[i] = ChannelConfig();
    }
    saveAll(configs);
    return false;
  }

  uint8_t buf[CHANNEL_CONFIG_BYTES];
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    readBytes(channelAddr(i), buf, CHANNEL_CONFIG_BYTES);
    deserialize(buf, configs[i]);
  }
  return true;
}

void EepromStore::saveChannel(uint8_t index, const ChannelConfig &cfg) {
  uint8_t buf[CHANNEL_CONFIG_BYTES];
  serialize(cfg, buf);
  writeBytes(channelAddr(index), buf, CHANNEL_CONFIG_BYTES);
}

void EepromStore::saveAll(const ChannelConfig configs[CHANNEL_COUNT]) {
  uint8_t header[HEADER_SIZE] = {MAGIC_0, MAGIC_1, LAYOUT_VERSION};
  writeBytes(HEADER_ADDR, header, HEADER_SIZE);
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    saveChannel(i, configs[i]);
  }
}
