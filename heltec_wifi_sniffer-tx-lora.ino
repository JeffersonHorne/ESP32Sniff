// WiFi Probe Request Sniffer + LoRa SX1262 Transmitter (Heltec Wireless Stick V3)

#include <WiFi.h>
#include <RadioLib.h>
extern "C" {
  #include "esp_wifi.h"
}

// Create an instance of SX1262 driver (NSS, DIO1, RESET, BUSY)
SX1262 lora = new Module(8, 14, 12, 13);

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("[SX1262] Initializing...");
  int state = lora.begin(915.0);  // 915 US

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa Init success!");
  } else {
    Serial.print("LoRa Init failed, code: ");
    Serial.println(state);
    while (true);  // Halt
  }

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

    // Send via LoRa
    char loraMessage[128];
    snprintf(loraMessage, sizeof(loraMessage), "MAC:%s RSSI:%d SSID:%s", macStr, rssi, ssid);

    int state = lora.transmit(loraMessage);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("[LoRa] Transmitted successfully");
    } else {
      Serial.print("[LoRa] Transmit failed, code: ");
      Serial.println(state);
    }
  }
}

void loop() {
  delay(100);  // Idle loop, everything happens in callback
}
