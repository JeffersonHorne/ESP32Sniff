#include "WiFi.h"
extern "C" {
  #include "esp_wifi.h"
}

void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  // We only care about management packets (probe requests)
  if (type != WIFI_PKT_MGMT) return;

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *payload = pkt->payload;

  // 802.11 Management frame subtype for Probe Request is 0x40 (Type 0, Subtype 4)
  uint8_t frame_type = payload[0] & 0x0C;
  uint8_t frame_subtype = payload[0] & 0xF0;

  if (frame_type == 0x00 && frame_subtype == 0x40) {  // Probe Request
    // Print MAC address of the device sending the probe request
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            payload[10], payload[11], payload[12],
            payload[13], payload[14], payload[15]);

    int rssi = pkt->rx_ctrl.rssi;  // Signal strength (RSSI)

    // SSID Parsing (starts after 24-byte management header)
    int ssid_offset = 24;
    while (ssid_offset < pkt->rx_ctrl.sig_len) {
      uint8_t tag_number = payload[ssid_offset];    // Tag ID
      uint8_t tag_length = payload[ssid_offset + 1]; // Tag Length

      // SSID Tag (Tag ID 0)
      if (tag_number == 0) {
        char ssid[33] = {0};  // Max SSID length is 32 + null terminator
        if (tag_length > 0 && tag_length <= 32) {
          memcpy(ssid, &payload[ssid_offset + 2], tag_length); // Copy SSID
        } else {
          strcpy(ssid, "<hidden>");  // If SSID is hidden, mark it
        }

        // Print the found SSID and associated details
        Serial.printf("Probe Request from %s | SSID: %s | RSSI: %d dBm\n",
                      macStr, ssid, rssi);
        break;  // Stop at first SSID tag (could be multiple SSIDs in a probe)
      }

      ssid_offset += 2 + tag_length; // Move to next tag
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Let Serial settle

  Serial.println("Starting Wi-Fi sniffer...");

  WiFi.mode(WIFI_MODE_STA);  // Set Wi-Fi mode to Station (default)
  esp_wifi_start();          // Start Wi-Fi (needed for promiscuous mode)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);  // Use channel 6 (busy channel)

  Serial.println("Sniffer setup complete.");
}

void loop() {
  delay(1000);  // Keep the loop alive and responsive
}
