#define LWIP_INTERNAL

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WiFiGratuitous.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <ETH.h>
#include <SPI.h>
#include <Update.h>
#endif
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "src/common/webserver.h"
#include "dallas.h"
#include "s0.h"
#include "HeishaOT.h"
#include "gpio.h"

#define HEATPUMP_VALUE_LEN    16

#if defined(ESP32)
  #ifndef TLS_SUPPORT
    #define TLS_SUPPORT 1
  #endif
#endif

void log_message(char* string);

static IPAddress apIP(192, 168, 4, 1);

struct settingsStruct {
  uint16_t waitTime = 5; // how often data is read from heatpump
  uint16_t waitDallasTime = 5; // how often temps are read from 1wire
  uint16_t dallasResolution = 12; // dallas temp resolution (9 to 12)
  uint16_t updateAllTime = 300; // how often all data is resend to mqtt
  uint16_t updataAllDallasTime = 300; //how often all 1wire data is resent to mqtt
  uint16_t timezone = 0;

  #define PASSWORD_LENGTH 65

  const char* update_path = "/firmware";
  const char* update_username = "admin";
  char wifi_ssid[33] = "";
  char wifi_password[PASSWORD_LENGTH] = "";
  char wifi_hostname[40] = "HeishaMon";
  char ota_password[40] = "heisha";
  char mqtt_server[65];
  char mqtt_port[6] = "1883";
  char mqtt_username[65];
  char mqtt_password[PASSWORD_LENGTH];
  char mqtt_topic_base[128] = "panasonic_heat_pump";
  char ntp_servers[254] = "pool.ntp.org";
#ifdef TLS_SUPPORT
  bool mqtt_tls_enabled = false;
#endif

  bool force_rules = false; //force rules on boot, even after a crash
  bool listenonly = false; //listen only so heishamon can be installed parallel to cz-taw1, set commands will not work though
  bool optionalPCB = false; //do we emulate an optional PCB?
  bool use_1wire = false; //1wire enabled?
  bool use_s0 = false; //s0 enabled?
  bool logMqtt = false; //log to mqtt from start
  bool logHexdump = false; //log hexdump from start
  bool logSerial1 = true; //log to serial1 (gpio2) from start
  bool publishRawData = false; //publish raw heatpump mqtt debug data
  bool listenRawData = false; //listen to raw heatpump mqtt debug data
  bool opentherm = false; //opentherm enable flag
  bool hotspot = true; //enable wifi hotspot when wifi is not connected
#ifdef ESP32
  bool proxy = true; //cztaw proxy port enable flag
#endif
  s0SettingsStruct s0Settings[NUM_S0_COUNTERS];
  gpioSettingsStruct gpioSettings;
};

struct websettings_t {
  String name;
  String value;
  struct websettings_t *next;
};

void setupConditionals();
int getFreeMemory(void);
char *getUptime(void);
void setupWifi(settingsStruct *heishamonSettings);
int getWifiQuality(void);
int getFreeMemory(void);
void ntpReload(settingsStruct *heishamonSettings);

void log_message(char *string);
int8_t webserver_cb(struct webserver_t *client, void *data);
void getWifiScanResults(int numSsid);
int handleRoot(struct webserver_t *client, float readpercentage, int mqttReconnects, settingsStruct *heishamonSettings);
int handleJsonOutput(struct webserver_t *client, char* actData, char* actDataExtra, char* actOptData, settingsStruct *heishamonSettings, bool extraDataBlockAvailable);
int handleFactoryReset(struct webserver_t *client);
int handleReboot(struct webserver_t *client);
int handleDebug(struct webserver_t *client, char *hex, byte hex_len);
void settingsToJson(JsonDocument  &jsonDoc, settingsStruct *heishamonSettings);
void saveJsonToFile(JsonDocument  &jsonDoc, const char *filename);
void loadSettings(settingsStruct *heishamonSettings);
int getSettings(struct webserver_t *client, settingsStruct *heishamonSettings);
int getSettingsJson(struct webserver_t *client, settingsStruct *heishamonSettings);

int handleSettings(struct webserver_t *client);
int saveSettings(struct webserver_t *client, settingsStruct *heishamonSettings);
int settingsReconnectWifi(struct webserver_t *client, settingsStruct *heishamonSettings);
int settingsNewPassword(struct webserver_t *client, settingsStruct *heishamonSettings);
int cacheSettings(struct webserver_t *client, struct arguments_t * args);
int handleWifiScan(struct webserver_t *client);
int showRules(struct webserver_t *client);
int showFirmware(struct webserver_t *client);
int showFirmwareSuccess(struct webserver_t *client);
int showFirmwareFail(struct webserver_t *client);
#ifdef TLS_SUPPORT 
int handleCACert(struct webserver_t *client);
int showCACert(struct webserver_t *client);
#endif
