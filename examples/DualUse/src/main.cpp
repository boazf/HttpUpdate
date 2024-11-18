#include <Arduino.h>
#ifdef USE_WIFI
#include <WiFi.h>
#else
#include <Ethernet.h>
#endif
#include <HttpUpdate.h>

// Fill here your current application version
#define APP_VERSION "1.0.0.15"

// Fill here your API KEY for the firmware to be updated
#define API_KEY "00000000-0000-0000-0000-000000000000"

#ifndef USE_WIFI
// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

#define RESET_P	17				// Tie the W5500 reset pin to ESP32 GPIO17 pin.
#define CS_P 16

static void WizReset() {
    pinMode(RESET_P, OUTPUT);
    digitalWrite(RESET_P, HIGH);
    delay(250);
    digitalWrite(RESET_P, LOW);
    delay(50);
    digitalWrite(RESET_P, HIGH);
    delay(350);
}
#endif

String getChipId()
{
  String ChipIdHex = String((uint32_t)(ESP.getEfuseMac() >> 32), HEX);
  ChipIdHex += String((uint32_t)ESP.getEfuseMac(), HEX);

  return ChipIdHex;
}

void setup()
{
  //Initialize serial:
  Serial.begin(115200);
  while (!Serial);

#ifndef USE_WIFI
  // start the Ethernet connection:
  Ethernet.init(CS_P);
  WizReset();
  log_d("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0)
  {
    log_e("Failed to configure Ethernet using DHCP!");
    return;
  } 
  else
  {
    log_d("DHCP assigned IP: %s\n", Ethernet.localIP().toString().c_str());
  }

  EthernetClient client;
#else // USE_WIFI
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin("ssid", "******"); // Fill here your SSID and Password for your WiFi network
    unsigned long t0 = millis();
    // Wait 30 seconds for WiFi network to connect
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000);
    if (WiFi.status() != WL_CONNECTED)
    {
      log_e("Could not connect to WiFi network!");
      return;
    }

    log_d("WiFi Connected!");
    log_d("RSSI: %ddb, BSSID: %s\n", WiFi.RSSI(), WiFi.BSSIDstr().c_str());

    WiFiClient client;
#endif // USE_WIFI
  String url = String("http://otadrive.com/deviceapi/update?k=") + API_KEY + "&v=" + APP_VERSION + "&s=" + getChipId();
  log_d("URL: %s\n", url.c_str());
  HttpUpdateResult result = httpUpdate.update(client, url, APP_VERSION);
  log_d("Update returned: %d\n", result);
}

void loop() {
  delay(2000);
}
