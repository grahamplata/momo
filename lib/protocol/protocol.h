#pragma once

#include <stdint.h>
#include <string.h>

constexpr uint8_t FRAME_MAGIC = 0x4D;  // 'M': rejects unrelated ESP-NOW traffic on our channel
constexpr uint8_t FRAME_VERSION = 1;   // bump on any CanFrame change; every device must be reflashed

// Frame broadcast by the bridge over ESP-NOW, one per CAN message.
// Packed with fixed-width fields so every receiver (ESP32 or ESP8266) agrees on the layout.
struct __attribute__((packed)) CanFrame {
  uint8_t magic = FRAME_MAGIC;
  uint8_t version = FRAME_VERSION;
  uint32_t mesh_id;  // identifies the sending bridge; gadgets filter on it
  uint32_t can_id;
  uint8_t extd;      // 1 = 29-bit extended identifier
  uint8_t len;       // data length, 0..8
  uint8_t data[8];
};
static_assert(sizeof(CanFrame) == 20, "CanFrame layout changed: bump FRAME_VERSION and update this size");

// Bridge and gadgets must sit on the same channel or frames are silently dropped.
constexpr uint8_t ESPNOW_CHANNEL = 1;

// Validates an untrusted ESP-NOW packet. mesh_id 0 accepts any bridge.
inline bool decode_frame(const uint8_t *data, int len, uint32_t mesh_id, CanFrame &out) {
  if (len != (int)sizeof(CanFrame)) return false;
  memcpy(&out, data, sizeof(out));
  return out.magic == FRAME_MAGIC && out.version == FRAME_VERSION && out.len <= 8 &&
         (mesh_id == 0 || out.mesh_id == mesh_id);
}
