// Sample gadget: receives CAN frames from a bridge over ESP-NOW and prints them.
// Copy this to start a new gadget and replace the body of on_frame().

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "protocol.h"

#ifndef MESH_ID
#define MESH_ID 0  // 0 = accept frames from any bridge
#endif

static void on_frame(const CanFrame &f) {
  Serial.printf("0x%03lX [%u]", (unsigned long)f.can_id, f.len);
  for (uint8_t i = 0; i < f.len; i++) Serial.printf(" %02X", f.data[i]);
  Serial.println();
}

static void on_recv(const uint8_t *mac, const uint8_t *data, int len) {
  // Anything nearby can broadcast ESP-NOW: drop malformed packets.
  if (len != sizeof(CanFrame)) return;
  CanFrame f;
  memcpy(&f, data, sizeof(f));
  if (f.len > 8) return;
  if (MESH_ID != 0 && f.mesh_id != MESH_ID) return;
  on_frame(f);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  while (esp_now_init() != ESP_OK) delay(100);
  esp_now_register_recv_cb(on_recv);
  Serial.println("\nmomo gadget_sample ready");
}

void loop() {}
