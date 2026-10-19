// Bridge: reads the car's CAN bus through the MrDIY ESP32 CAN shield and
// broadcasts every frame over ESP-NOW. Never transmits on the bus.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "driver/twai.h"
#include "protocol.h"

#ifndef CAN_RX_PIN
#error "Set CAN_RX_PIN / CAN_TX_PIN in platformio.ini for your shield version"
#endif

constexpr int LED_PIN = 26;
constexpr uint32_t CAN_IDLE_TIMEOUT_MS = 500;    // no traffic this long -> car is off, go to sleep
constexpr uint64_t SLEEP_PERIOD_US = 6000000ULL; // wake every 6s to check for traffic again

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint32_t mesh_id;
static uint32_t last_rx_ms;

static bool init_espnow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) return false;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, sizeof(BROADCAST));
  peer.channel = ESPNOW_CHANNEL;
  return esp_now_add_peer(&peer) == ESP_OK;
}

static bool init_can() {
  // Listen-only: the bridge must never ACK or write to the car's bus.
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
  g.rx_queue_len = 32;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  return twai_driver_install(&g, &t, &f) == ESP_OK && twai_start() == ESP_OK;
}

static void forward(const twai_message_t &m) {
  CanFrame frame = {};
  frame.mesh_id = mesh_id;
  frame.can_id = m.identifier;
  frame.extd = m.extd;
  frame.len = m.data_length_code > 8 ? 8 : m.data_length_code;
  memcpy(frame.data, m.data, frame.len);
  // shortcut: send errors (ESP-NOW queue full on a busy bus) drop the frame; add a ring buffer if gadgets miss frames.
  esp_now_send(BROADCAST, (const uint8_t *)&frame, sizeof(frame));
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  mesh_id = (uint32_t)(ESP.getEfuseMac() >> 16);  // last 4 bytes of the MAC
  Serial.printf("\nmomo bridge, mesh_id=%lu\n", (unsigned long)mesh_id);

  if (!init_espnow() || !init_can()) {
    Serial.println("init failed, restarting in 60s");
    delay(60 * 1000);
    ESP.restart();
  }
  last_rx_ms = millis();
}

void loop() {
  twai_message_t m;
  while (twai_receive(&m, pdMS_TO_TICKS(100)) == ESP_OK) {
    forward(m);
    last_rx_ms = millis();
  }

  if (millis() - last_rx_ms > CAN_IDLE_TIMEOUT_MS) {
    Serial.println("bus idle, sleeping");
    esp_sleep_enable_timer_wakeup(SLEEP_PERIOD_US);
    esp_deep_sleep_start();
  }
}
