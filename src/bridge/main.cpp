// Bridge: reads the car's CAN bus through the MrDIY ESP32 CAN shield and
// broadcasts every frame over ESP-NOW. Never transmits on the bus.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "driver/twai.h"
#include "protocol.h"

#ifndef CAN_RX_PIN
#error "Set CAN_RX_PIN / CAN_TX_PIN in platformio.ini"
#endif

constexpr int LED_PIN = 26;
constexpr int CAN_LOOPBACK_PIN = 33;  // transceiver LBK, active HIGH: bus is cut off while set
constexpr uint32_t CAN_IDLE_TIMEOUT_MS = 500;    // no traffic this long -> car is off, go to sleep
constexpr uint64_t SLEEP_PERIOD_US = 6000000ULL; // wake every 6s to check for traffic again

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint32_t mesh_id;
static uint32_t last_rx_ms;
static uint32_t send_failures;

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
  // shortcut: frames are dropped when ESP-NOW's send queue is full (counted, see report_drops);
  // pack several CAN frames per packet if the counts are high on your car.
  if (esp_now_send(BROADCAST, (const uint8_t *)&frame, sizeof(frame)) != ESP_OK) send_failures++;
}

// Prints cumulative drop counts once a second, only once something has been lost.
static void report_drops() {
  static uint32_t last_report_ms;
  if (millis() - last_report_ms < 1000) return;
  last_report_ms = millis();
  twai_status_info_t s;
  if (twai_get_status_info(&s) != ESP_OK) return;
  if (send_failures || s.rx_missed_count || s.rx_overrun_count)
    Serial.printf("dropped: espnow=%lu can_queue=%lu can_overrun=%lu\n", (unsigned long)send_failures,
                  (unsigned long)s.rx_missed_count, (unsigned long)s.rx_overrun_count);
}

void setup() {
  Serial.begin(115200);
  pinMode(CAN_LOOPBACK_PIN, OUTPUT);
  digitalWrite(CAN_LOOPBACK_PIN, LOW);
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
  if (twai_receive(&m, pdMS_TO_TICKS(100)) == ESP_OK) {
    forward(m);
    last_rx_ms = millis();
  }
  report_drops();

  if (millis() - last_rx_ms > CAN_IDLE_TIMEOUT_MS) {
    Serial.println("bus idle, sleeping");
    esp_sleep_enable_timer_wakeup(SLEEP_PERIOD_US);
    esp_deep_sleep_start();
  }
}
