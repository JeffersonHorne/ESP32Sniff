// Simple WiFi Probe Request Sniffer)

#include <WiFi.h>
extern "C" {
  #include "esp_wifi.h"
}

// Channel hopping setup
const int wifiChannels[] = {1, 6, 11};
int currentChannelIndex = 0;
unsigned long lastChannelSwitch = 0;
const unsigned long channelSwitchInterval = 5000; // switch every 5 seconds

void setup() {
  Serial.begin(115200);
  delay(500);

  // Set Wi-Fi to promiscuous mode
  Serial.println("Starting Wi-Fi sniffer...");

  WiFi.mode(WIFI_MODE_STA);  // Set Wi-Fi mode to Station (default)
  esp_wifi_start();          // Start Wi-Fi (needed for promiscuous mode)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);  // Use channel 6 (busy channel)

  Serial.println("Sniffer setup complete.");
}

void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *payload = pkt->payload;

  uint8_t frame_type = payload[0] & 0x0C;
  uint8_t frame_subtype = payload[0] & 0xF0;

  if (frame_type == 0x00 && frame_subtype == 0x40) {  // Probe Request
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            payload[10], payload[11], payload[12],
            payload[13], payload[14], payload[15]);

    int rssi = pkt->rx_ctrl.rssi;

    // Parse SSID
    int ssid_offset = 24;
    char ssid[33] = {0};
    if (ssid_offset + 2 < pkt->rx_ctrl.sig_len) {
      uint8_t tag_number = payload[ssid_offset];
      uint8_t tag_length = payload[ssid_offset + 1];

      if (tag_number == 0 && tag_length <= 32) {
        memcpy(ssid, &payload[ssid_offset + 2], tag_length);
      } else {
        strcpy(ssid, "<hidden>");
      }
    }

    // Print to Serial
    Serial.printf("MAC: %s RSSI: %d SSID: %s\n", macStr, rssi, ssid);

  }
}

void loop() {
    if (millis() - lastChannelSwitch > channelSwitchInterval) {
    currentChannelIndex = (currentChannelIndex + 1) % (sizeof(wifiChannels) / sizeof(wifiChannels[0]));
    esp_wifi_set_channel(wifiChannels[currentChannelIndex], WIFI_SECOND_CHAN_NONE);
    Serial.print("Switched to Wi-Fi channel: ");
    Serial.println(wifiChannels[currentChannelIndex]);
    lastChannelSwitch = millis();
  }
  delay(100);  // Idle loop, everything happens in callback
}
