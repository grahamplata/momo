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

static QueueHandle_t frames;
static volatile uint32_t dropped;

// Runs in loop(), so slow work here (serial, relays, displays) never stalls the radio.
static void on_frame(const CanFrame &f) {
  Serial.printf("0x%03lX [%u]", (unsigned long)f.can_id, f.len);
  for (uint8_t i = 0; i < f.len; i++) Serial.printf(" %02X", f.data[i]);
  Serial.println();
}

// Runs on the Wi-Fi task: validate and queue only, never block.
static void on_recv(const uint8_t *mac, const uint8_t *data, int len) {
  CanFrame f;
  if (decode_frame(data, len, MESH_ID, f) && xQueueSend(frames, &f, 0) != pdTRUE) dropped++;
}

void setup() {
  Serial.begin(115200);
  frames = xQueueCreate(64, sizeof(CanFrame));
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  while (esp_now_init() != ESP_OK) delay(100);
  esp_now_register_recv_cb(on_recv);
  Serial.println("\nmomo gadget_sample ready");
}

void loop() {
  static uint32_t last_report_ms;
  CanFrame f;
  if (xQueueReceive(frames, &f, pdMS_TO_TICKS(100)) == pdTRUE) on_frame(f);

  if (dropped && millis() - last_report_ms >= 1000) {
    Serial.printf("dropped: queue=%lu\n", (unsigned long)dropped);
    last_report_ms = millis();
  }
}
