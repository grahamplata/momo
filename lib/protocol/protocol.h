#pragma once

#include <stdint.h>

// Frame broadcast by the bridge over ESP-NOW, one per CAN message.
// Packed with fixed-width fields so every receiver (ESP32 or ESP8266) agrees on the layout.
struct __attribute__((packed)) CanFrame {
  uint32_t mesh_id;  // identifies the sending bridge; gadgets filter on it
  uint32_t can_id;
  uint8_t extd;      // 1 = 29-bit extended identifier
  uint8_t len;       // data length, 0..8
  uint8_t data[8];
};
static_assert(sizeof(CanFrame) == 18, "CanFrame layout changed: update every bridge and gadget together");

// Bridge and gadgets must sit on the same channel or frames are silently dropped.
constexpr uint8_t ESPNOW_CHANNEL = 1;
