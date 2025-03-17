// WiFi Probe Request Sniffer that limits to unique probe requests with an array and counter

#include <WiFi.h>
extern "C" {
  #include "esp_wifi.h"
}

// Channel hopping setup
const int wifiChannels[] = {1, 6, 11};
int currentChannelIndex = 0;
unsigned long lastChannelSwitch = 0;
const unsigned long channelSwitchInterval = 5000; // switch every 5 seconds

// MAC address history setup
#define MAX_MAC_HISTORY 100  // Max number of MAC addresses to store
#define MAC_HISTORY_TIMEOUT 600000  // Timeout in milliseconds (1 minute)

struct MacEntry {
  uint8_t mac[6];
  unsigned long lastSeen;
};

MacEntry macHistory[MAX_MAC_HISTORY];
int macHistoryIndex = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  // Set Wi-Fi to promiscuous mode
  Serial.println("Starting Wi-Fi sniffer...");

  WiFi.mode(WIFI_MODE_STA);  // Set Wi-Fi mode to Station (default)
  esp_wifi_start();          // Start Wi-Fi (needed for promiscuous mode)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);  // Start with channel 6 (busy channel)

  Serial.println("Sniffer setup complete.");
}

bool isNewMac(uint8_t *mac) {
  unsigned long currentTime = millis();
  
  // Search through the history
  for (int i = 0; i < macHistoryIndex; i++) {
    // Compare MAC addresses and check if the entry is within the timeout window
    if (memcmp(macHistory[i].mac, mac, 6) == 0) {
      if (currentTime - macHistory[i].lastSeen < MAC_HISTORY_TIMEOUT) {
        return false;  // MAC address is within the timeout period, so it's a repeat
      }
    }
  }
  
  // Store new MAC address in history
  if (macHistoryIndex < MAX_MAC_HISTORY) {
    memcpy(macHistory[macHistoryIndex].mac, mac, 6);
    macHistory[macHistoryIndex].lastSeen = currentTime;
    macHistoryIndex++;
  } else {
    // Overwrite the oldest MAC address if we've reached max history size
    for (int i = 1; i < MAX_MAC_HISTORY; i++) {
      macHistory[i - 1] = macHistory[i];
    }
    memcpy(macHistory[MAX_MAC_HISTORY - 1].mac, mac, 6);
    macHistory[MAX_MAC_HISTORY - 1].lastSeen = currentTime;
  }
  
  return true;  // New MAC address
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

    // Check if MAC address is new
    if (isNewMac(&payload[10])) {
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
