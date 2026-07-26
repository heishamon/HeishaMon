#define LWIP_INTERNAL

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
  #define heatpumpSerial Serial
  #define loggingSerial Serial1
  #define ENABLEPIN 5
  #define LEDPIN 2
  #define BOOTPIN 0
#elif defined(ESP32)
  #define heatpumpSerial Serial1
  #define loggingSerial Serial //usb serial CDC
  #define uartSerial Serial0 //not used, 10x header pin
  #define proxySerial Serial2
  #define HEATPUMPRX 18
  #define HEATPUMPTX 17
  #define PROXYRX 9
  #define PROXYTX 8
  #define ENABLEPIN 5
  #define ENABLEOTPIN 4
  #define LEDPIN 42
  #define BOOTPIN 0
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Adafruit_NeoPixel.h>
#endif


#include <DNSServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

#include "lwip/apps/sntp.h"
#include "src/common/timerqueue.h"
#include "src/common/stricmp.h"
#include "src/common/log.h"
#include "src/common/progmem.h"
#include "src/rules/rules.h"

#include "webfunctions.h"
#include "decode.h"
#include "commands.h"
#include "rules.h"
#include "version.h"

DNSServer dnsServer;

//to read bus voltage in stats
#ifdef ESP8266
ADC_MODE(ADC_VCC);
#endif

const byte DNS_PORT = 53;

#define SERIALTIMEOUT 2000 // wait until all 203 bytes are read, must not be too long to avoid blocking the code

settingsStruct heishamonSettings;

uint32_t neoPixelState = 0; //running neoPixelState
bool inSetup; //bool to check if still booting
volatile bool sending = false; // mutex for sending data
bool mqttcallbackinprogress = false; // mutex for processing mqtt callback

bool extraDataBlockAvailable = false; // this will be set to true if, during boot, heishamon detects this heatpump has extra data block (like K and L series do)

#define MQTTRECONNECTTIMER 30000 //it takes 30 secs for each mqtt server reconnect attempt
unsigned long lastMqttReconnectAttempt = 0;

unsigned long bootButtonNotPressed = 0;

#define WIFIRETRYTIMER 15000 // switch between hotspot and configured SSID each 10 secs if SSID is lost
unsigned long lastWifiRetryTimer = 0;
bool doInitialWifiScan = true; //we want an initial wifi scan to fill in the dropbox on the wifi settings page

unsigned long lastRunTime = 0;

#ifdef ESP8266
unsigned long lastOptionalPCBRunTime = 0;
unsigned long lastOptionalPCBSave = 0;
#endif
volatile unsigned long sendCommandReadTime = 0; //set to millis value during send, allow to wait millis for answer

unsigned long goodreads = 0;
unsigned long totalreads = 0;
unsigned long badcrcread = 0;
unsigned long badheaderread = 0;
unsigned long tooshortread = 0;
unsigned long toolongread = 0;
unsigned long timeoutread = 0;
float readpercentage = 0;
static int uploadpercentage = 0;

// instead of passing array pointers between functions we just define this in the global scope
#define MAXDATASIZE 255
char data[MAXDATASIZE] = { '\0' };
byte data_length = 0;

#ifdef ESP32
//for received proxied data
char proxydata[MAXDATASIZE] = { '\0' };
byte proxydata_length = 0;
//for the neopixel
Adafruit_NeoPixel pixels(1, LEDPIN);
//for the vTask
QueueHandle_t pcbQueue = NULL;
QueueHandle_t cmdQueue = NULL;
QueueHandle_t logQueue = NULL;
#endif

// store actual data
char actData[DATASIZE] = { '\0' };
char actDataExtra[DATASIZE] = { '\0' };
char actOptData[OPTDATASIZE]  = { '\0' };

// log message to sprintf to
#define LOG_MSG_SIZE 256
char log_msg[LOG_MSG_SIZE];

// mqtt topic to sprintf and then publish to
char mqtt_topic[256];

static int mqttReconnects = 0;

// state for restoring known 1wire sensors from mqtt retained messages just after boot
bool dallasMqttRestorePending = false;
unsigned long dallasMqttRestoreStart = 0;
#define DALLAS_MQTT_RESTORE_TIMEOUT 3000

// can't have too much in buffer due to memory shortage
#define MAXCOMMANDSINBUFFER 10

// buffer for commands to send
struct cmdbuffer_t {
  uint8_t length;
  byte data[128];
} cmdbuffer[MAXCOMMANDSINBUFFER];

static uint8_t cmdstart = 0;
static uint8_t cmdend = 0;
static uint8_t cmdnrel = 0;



// mqtt
#ifdef TLS_SUPPORT
#include <WiFiClientSecure.h>
WiFiClientSecure *mqtt_tls_client = nullptr;
WiFiClient mqtt_wifi_client;
bool loadTlsCaFromFS(WiFiClientSecure *client);
static bool last_tls_enabled = false;
static bool new_ca_stored = false;
static std::unique_ptr<char[]> persistent_ca_pem;
PubSubClient mqtt_client;
#else
WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_wifi_client);
#endif


bool firstConnectSinceBoot = true; //if this is true there is no first connection made yet

struct timerqueue_t **timerqueue = NULL;
int timerqueue_size = 0;

#ifdef ESP32
#define ETH_TYPE        ETH_PHY_W5500
#define ETH_ADDR         1
#define ETH_CS          10
#define ETH_IRQ          15
#define ETH_RST          14

// SPI pins
#define ETH_SPI_SCK     12
#define ETH_SPI_MISO    13
#define ETH_SPI_MOSI    11

void setupETH() {
  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  if (ETH.begin(ETH_TYPE, ETH_ADDR, ETH_CS, ETH_IRQ, ETH_RST, SPI)) {
    //sethostname on ESP32 after eth.begin (!! for wifi is most be before...!!)
    ETH.setHostname(heishamonSettings.wifi_hostname);
  } else {
    loggingSerial.println("Could not start ethernet. No ethernet module installed?");
  }
}
#endif



/*
    check_wifi will process wifi reconnecting managing
*/
#if defined(ESP8266)
void check_wifi() {
  int wifistatus = WiFi.status();
  if ((wifistatus != WL_CONNECTED) && (WiFi.localIP())) {
    // special case where it seems that we are not connect but we do have working IP (causing the -1% wifi signal), do a reset.
    log_message(_F("Weird case, WiFi seems disconnected but is not. Resetting WiFi!"));
    setupWifi(&heishamonSettings);
  } else if ((wifistatus != WL_CONNECTED) || (!WiFi.localIP())) {
    /*
        if we are not connected to an AP
        we must be in softAP so respond to DNS
    */
    if (heishamonSettings.hotspot) {
      dnsServer.processNextRequest();
    }

    /* we need to stop reconnecting to a configured wifi network if there is a hotspot user connected
        also, do not disconnect if wifi network scan is active
    */
    if ((heishamonSettings.wifi_ssid[0] != '\0') && (wifistatus != WL_DISCONNECTED) && (WiFi.scanComplete() != -1) && (WiFi.softAPgetStationNum() > 0)) {
      log_message(_F("WiFi lost, but softAP station connecting, so stop trying to connect to configured ssid..."));
      WiFi.disconnect(true);
    }

    /*  only start this routine if timeout on
        reconnecting to AP and SSID is set
    */
    if ((heishamonSettings.wifi_ssid[0] != '\0') && ((unsigned long)(millis() - lastWifiRetryTimer) > WIFIRETRYTIMER)) {
      lastWifiRetryTimer = millis();
      if ((WiFi.softAPSSID() == "") && (heishamonSettings.hotspot)) {
        log_message(_F("WiFi lost, starting setup hotspot..."));
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(_F("HeishaMon-Setup"));
      }
      if ((wifistatus == WL_DISCONNECTED) && (WiFi.softAPgetStationNum() == 0)) {
        log_message(_F("Retrying configured WiFi, ..."));
        if (heishamonSettings.wifi_password[0] == '\0') {
          WiFi.begin(heishamonSettings.wifi_ssid);
        } else {
          WiFi.begin(heishamonSettings.wifi_ssid, heishamonSettings.wifi_password);
        }
      } else {
        log_message(_F("Reconnecting to WiFi failed. Waiting a few seconds before trying again."));
        WiFi.disconnect(true);
      }
    }
  }
  if (WiFi.localIP()) {  //WiFi connected
    if (WiFi.softAPSSID() != "") {
      log_message(_F("WiFi (re)connected, shutting down hotspot..."));
      WiFi.softAPdisconnect(true);
      MDNS.notifyAPChange();
    }

    if (firstConnectSinceBoot) {  // this should start only when softap is down or else it will not work properly so run after the routine to disable softap
      firstConnectSinceBoot = false;
      lastMqttReconnectAttempt = 0;  //initiate mqtt connection asap
      setupOTA();
      MDNS.begin(heishamonSettings.wifi_hostname);
      MDNS.addService("http", "tcp", 80);
      experimental::ESP8266WiFiGratuitous::stationKeepAliveSetIntervalMs(5000);  //necessary for some users with bad wifi routers

      if (heishamonSettings.wifi_ssid[0] == '\0') {
        log_message(_F("WiFi connected without SSID and password in settings. Must come from persistent memory. Storing in settings."));
        WiFi.SSID().toCharArray(heishamonSettings.wifi_ssid, 40);
        WiFi.psk().toCharArray(heishamonSettings.wifi_password, 40);
        JsonDocument jsonDoc;
        settingsToJson(jsonDoc, &heishamonSettings);  //stores current settings in a json document
        saveJsonToFile(jsonDoc, "config.json");     //save to config file
      }

      ntpReload(&heishamonSettings);
      logprintln_P(F("Try to syncing with ntp servers. Checking again in 5 minutes"));
      timerqueue_insert(300, 0, -6);
    }

    /*
       always update if wifi is working so next time on ssid failure
       it only starts the routine above after this timeout
    */
    lastWifiRetryTimer = millis();

    // Allow MDNS processing
    MDNS.update();
  }
  if (doInitialWifiScan && (millis() > 15000)) {  //do a wifi scan a boot after 15 seconds
    doInitialWifiScan = false;
    log_message(_F("Starting initial wifi scan ..."));
    WiFi.scanNetworksAsync(getWifiScanResults);
  }
}
#elif defined(ESP32)
void check_wifi() {
  wl_status_t wifistatus = WiFi.status();
  bool ethUp = ETH.hasIP();
  bool wifiUp = (wifistatus == WL_CONNECTED);

  /* ---------- Fast path: network is up ---------- */
  if (wifiUp || ethUp) {

    neoPixelState = pixels.Color(0, 0, 0); // normal operation
    lastWifiRetryTimer = millis();

    // Shut down hotspot if no longer needed
    if ((WiFi.getMode() & WIFI_MODE_AP) &&
        ((heishamonSettings.wifi_ssid[0] != '\0') || !heishamonSettings.hotspot)) {

      log_message(_F("WiFi or ETH connected, shutting down hotspot"));
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      if (wifistatus != WL_CONNECTED) { //it must be that ETH reconnected, so keep trying WiFi in the background
        WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
        if (heishamonSettings.wifi_password[0] == '\0') {
          WiFi.begin(heishamonSettings.wifi_ssid);
        } else {
          WiFi.begin(heishamonSettings.wifi_ssid, heishamonSettings.wifi_password);
        }
      }
    }

    if (firstConnectSinceBoot) {
      firstConnectSinceBoot = false;

      lastMqttReconnectAttempt = 0;
      setupOTA();

      MDNS.begin(heishamonSettings.wifi_hostname);
      MDNS.addService("http", "tcp", 80);

      if (heishamonSettings.wifi_ssid[0] == '\0') {
        log_message(_F("Storing WiFi credentials from persistent memory"));
        WiFi.SSID().toCharArray(heishamonSettings.wifi_ssid, 40);
        WiFi.psk().toCharArray(heishamonSettings.wifi_password, 40);
        JsonDocument jsonDoc;
        settingsToJson(jsonDoc, &heishamonSettings);
        saveJsonToFile(jsonDoc, "config.json");
      }

      ntpReload(&heishamonSettings);
      logprintln_P(F("NTP sync scheduled"));
      timerqueue_insert(300, 0, -6);
    }

    return;
  }

  /* ---------- Network is DOWN ---------- */

  neoPixelState = pixels.Color(16, 16, 0); // yellow: degraded

  if (heishamonSettings.hotspot) {
    dnsServer.processNextRequest();
  }

  // If AP client is connected, STA must back off
  if (WiFi.softAPgetStationNum() > 0) {
    if (WiFi.getMode() != WIFI_AP) {
      log_message(_F("SoftAP client active, suspending STA reconnect"));
	    WiFi.disconnect(true);
	    WiFi.mode(WIFI_AP);
    }
	  return; //always return if hotspot is used
  }

  // Periodic retry gate
  if ((unsigned long)(millis() - lastWifiRetryTimer) < WIFIRETRYTIMER) {
    return;
  }
  lastWifiRetryTimer = millis();

  // Ensure AP is running if allowed
  if (heishamonSettings.hotspot && !(WiFi.getMode() & WIFI_MODE_AP)) {
    log_message(_F("Starting setup hotspot"));
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(_F("HeishaMon-Setup"));
  }

  // Disable STA so next retry is clean and we wait WIFIRETRYTIMER so hotspot can do its thing
  if (WiFi.getMode() != WIFI_AP) {
    log_message(_F("Disabling WiFi STA for a while..."));	
	  WiFi.disconnect(true);
    WiFi.mode(WIFI_AP); 
    return;
  }

  // Retry STA connection
  if (heishamonSettings.wifi_ssid[0] != '\0') {
    // Repair STA if it is stopped
    if (!(WiFi.getMode() & WIFI_MODE_STA)) {
      log_message(_F("STA stopped, re-enabling STA"));
      WiFi.mode(WIFI_AP_STA);
      delay(50);
    }    
    log_message(_F("Retrying configured WiFi"));
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    if (heishamonSettings.wifi_password[0] == '\0') {
      WiFi.begin(heishamonSettings.wifi_ssid);
    } else {
      WiFi.begin(heishamonSettings.wifi_ssid, heishamonSettings.wifi_password);
    }
  }
}
#endif

#ifdef TLS_SUPPORT
bool loadTlsCaFromFS(WiFiClientSecure *client) {
  if (!LittleFS.exists("/ca.pem")) {
    log_message(_F("[TLS] /ca.pem not found"));
    return false;
  }
  File certFile = LittleFS.open("/ca.pem", "r");
  if (!certFile) {
    log_message(_F("[TLS] open(/ca.pem) failed"));
    return false;
  }
  size_t certSize = certFile.size();
  if (certSize == 0) {
    log_message(_F("[TLS] /ca.pem is empty"));
    certFile.close();
    return false;
  }
  persistent_ca_pem.reset(new char[certSize + 1]);
  size_t n = certFile.readBytes(persistent_ca_pem.get(), certSize);
  persistent_ca_pem[n] = '\0';
  certFile.close();
  client->setCACert(persistent_ca_pem.get());
  log_message(_F("[TLS] CA loaded into client"));
  return true;
}
#endif


void mqtt_reconnect()
{
  unsigned long now = millis();
  if ((lastMqttReconnectAttempt == 0) || ((unsigned long)(now - lastMqttReconnectAttempt) > MQTTRECONNECTTIMER)) { //only try reconnect each MQTTRECONNECTTIMER seconds or on boot when lastMqttReconnectAttempt is still 0
    lastMqttReconnectAttempt = now;
    if (mqttReconnects == 0) {
      log_message(_F("Connecting to mqtt server ..."));
    } else {
      log_message(_F("Reconnecting to mqtt server ..."));
    }
    char topic[256];
    sprintf(topic, "%s/%s", heishamonSettings.mqtt_topic_base, mqtt_willtopic);
#ifdef TLS_SUPPORT
    if (heishamonSettings.mqtt_tls_enabled != last_tls_enabled) {
      mqtt_client.disconnect();
      if (last_tls_enabled) {
        mqtt_tls_client->stop();
      } else {
        mqtt_wifi_client.stop();
        if (!loadTlsCaFromFS(mqtt_tls_client)) {
          log_message(_F("[TLS] Proceeding without valid CA (expect failure)"));
        }
      }
      last_tls_enabled = heishamonSettings.mqtt_tls_enabled;
    }

    if (new_ca_stored) {
      log_message(_F("[TLS] Trying to load new CA ertificate"));
      if (!loadTlsCaFromFS(mqtt_tls_client)) {
        log_message(_F("[TLS] Proceeding without valid CA (expect failure)"));
      }
      new_ca_stored = false;
    }
    if (heishamonSettings.mqtt_tls_enabled) {
      mqtt_client.setClient(*mqtt_tls_client);
    } else {
      mqtt_client.setClient(mqtt_wifi_client);
    }
      mqtt_client.setSocketTimeout(10);
      mqtt_client.setKeepAlive(30);
      mqtt_client.setServer(heishamonSettings.mqtt_server, atoi(heishamonSettings.mqtt_port));
#endif
    if (mqtt_client.connect(heishamonSettings.wifi_hostname, heishamonSettings.mqtt_username, heishamonSettings.mqtt_password, topic, 1, true, "Offline"))
    {
      mqttReconnects++;
      if (heishamonSettings.opentherm) {
        sprintf(topic, "%s/%s/#", heishamonSettings.mqtt_topic_base, mqtt_topic_opentherm_read);
        mqtt_client.subscribe(topic);
      }
      sprintf(topic, "%s/%s/#", heishamonSettings.mqtt_topic_base, mqtt_topic_commands);
      mqtt_client.subscribe(topic);
      sprintf(topic, "%s/%s/#", heishamonSettings.mqtt_topic_base, mqtt_topic_gpio);
      mqtt_client.subscribe(topic);      
      sprintf(topic, "%s/%s", heishamonSettings.mqtt_topic_base, mqtt_send_raw_value_topic);
      mqtt_client.subscribe(topic);
      sprintf(topic, "%s/%s", heishamonSettings.mqtt_topic_base, mqtt_willtopic);
      mqtt_client.publish(topic, "Online");
      sprintf(topic, "%s/%s", heishamonSettings.mqtt_topic_base, mqtt_iptopic);
#ifdef ESP8266
      mqtt_client.publish(topic, WiFi.localIP().toString().c_str(), true);
#else
      if (ETH.hasIP()) {
        mqtt_client.publish(topic, ETH.localIP().toString().c_str(), true);
      } else {
        mqtt_client.publish(topic, WiFi.localIP().toString().c_str(), true);
      }
#endif

      if (heishamonSettings.use_s0) { // connect to s0 topic to retrieve older watttotal from mqtt
        sprintf_P(mqtt_topic, PSTR("%s/%s/WatthourTotal/1"), heishamonSettings.mqtt_topic_base, mqtt_topic_s0);
        mqtt_client.subscribe(mqtt_topic);
        sprintf_P(mqtt_topic, PSTR("%s/%s/WatthourTotal/2"), heishamonSettings.mqtt_topic_base, mqtt_topic_s0);
        mqtt_client.subscribe(mqtt_topic);
      }
      if (heishamonSettings.use_1wire && mqttReconnects == 1) { // only on first connect: retrieve previously known 1wire sensors from their retained mqtt topics
        sprintf_P(mqtt_topic, PSTR("%s/%s/+"), heishamonSettings.mqtt_topic_base, mqtt_topic_1wire);
        mqtt_client.subscribe(mqtt_topic);
        dallasMqttRestorePending = true;
        dallasMqttRestoreStart = millis();
      }
      if (mqttReconnects == 1) { //only resend all data on first connect to mqtt so a data bomb like and bad mqtt server will not cause a reconnect bomb everytime
        if (heishamonSettings.use_1wire) resetlastalldatatime_dallas(); //resend all 1wire values to mqtt
        resetlastalldatatime(); //resend all heatpump values to mqtt
      }
      //use this to receive valid heishamon raw data from other heishamon to debug this OT code
//#define RAWDEBUG
#ifdef RAWDEBUG
      if ( heishamonSettings.listenonly) {
        mqtt_client.subscribe((char*)"panasonic_heat_pump/raw/data"); //subscribe to raw heatpump data over MQTT
      }
#endif
    }
//#ifdef TLS_SUPPORT // error state is useful in any case
    else {
      int8_t err = mqtt_client.state();
      log_message(_F("MQTT connect failed, state:"));
      switch (err) {
        case -1: log_message(_F(" -1 → TLS handshake or network error")); break;
        case -2: log_message(_F(" -2 → Connection timeout – cannot reach broker or CA/time error")); break;
        case -3: log_message(_F(" -3 → Server not found or rejected")); break;
        case -4: log_message(_F(" -4 → Connection lost")); break;
        case -5: log_message(_F(" -5 → Check username/password")); break;
        default: log_message(_F("    → Unknown error")); break;
      }
    }
//#endif
  }
}

#ifdef ESP32
void blinkNeoPixel(bool status) {
  if (status) {
    pixels.setPixelColor(0, 0, 0, 16); //blue
  } else {
    pixels.setPixelColor(0, neoPixelState);
  }
  pixels.show(); 
}
#endif  


void log_message(char* string)
{
#ifdef ESP32
  if (!inSetup) blinkNeoPixel(true);
#endif
  time_t rawtime;
  rawtime = time(NULL);
  struct tm *timeinfo = localtime(&rawtime);
  char timestring[32];
  strftime(timestring, 32, "%c", timeinfo);
  size_t len = strlen(string) + strlen(timestring) + 32; //+32 long enough to contain millis() and the json part later for websocket mesg
  char* log_line = (char *) malloc(len);
  snprintf(log_line, len, "%s (%lu): %s", timestring, millis(), string);

  if (heishamonSettings.logSerial1) {
    loggingSerial.println(log_line);
  }
  if (heishamonSettings.logMqtt && mqtt_client.connected())
  {
    char log_topic[256];
    sprintf(log_topic, "%s/%s", heishamonSettings.mqtt_topic_base, mqtt_logtopic);

    if (!mqtt_client.publish(log_topic, log_line)) {
      if (heishamonSettings.logSerial1) {
        loggingSerial.print(millis());
        loggingSerial.print(F(": "));
        loggingSerial.println(F("MQTT publish log message failed!"));
      }
      mqtt_client.disconnect();
    }
  }
  //send log message to websocket
  snprintf(log_line, len+12, "{\"logMsg\":\"%s (%lu): %s\"}", timestring, millis(), string);
  websocket_write_all(log_line, strlen(log_line));
  free(log_line);
#ifdef ESP32
  if (!inSetup) blinkNeoPixel(false);
#endif  
}

void logHex(char *hex, byte hex_len) {
#define LOGHEXBYTESPERLINE 32  // please be aware of max mqtt message size
  for (int i = 0; i < hex_len; i += LOGHEXBYTESPERLINE) {
    char buffer [(LOGHEXBYTESPERLINE * 3) + 1];
    buffer[LOGHEXBYTESPERLINE * 3] = '\0';
    for (int j = 0; ((j < LOGHEXBYTESPERLINE) && ((i + j) < hex_len)); j++) {
      sprintf(&buffer[3 * j], "%02X ", hex[i + j]);
    }
    sprintf_P(log_msg, PSTR("data: %s"), buffer ); log_message(log_msg);
  }
}

void mqttPublish(char* topic, char* subtopic, char* value) {
  mqttPublish(topic, subtopic, value, MQTT_RETAIN_VALUES);
}

void mqttPublish(char* topic, char* subtopic, char* value, bool retain) {
  char mqtt_topic[256];
  sprintf_P(mqtt_topic, PSTR("%s/%s/%s"), heishamonSettings.mqtt_topic_base, topic, subtopic);
  mqtt_client.publish(mqtt_topic, value, retain);
}



byte calcChecksum(byte* command, int length) {
  byte chk = 0;
  for ( int i = 0; i < length; i++)  {
    chk += command[i];
  }
  chk = (chk ^ 0xFF) + 01;
  return chk;
}

bool isValidReceiveChecksum(char* check_data, byte check_length) {
  byte chk = 0;
  for ( int i = 0; i < check_length; i++)  {
    chk += check_data[i];
  }
  return (chk == 0); //all received bytes + checksum should result in 0
}

#ifdef ESP32
void readProxy()
{
  int proxylen = 0;
  while ((proxySerial.available()) && ((proxydata_length + proxylen) < MAXDATASIZE)) {
    proxydata[proxydata_length + proxylen] = proxySerial.read(); //read available data and place it after the last received data
    proxylen++;
    if ((proxydata[0] != 0x71) and  (proxydata[0] != 0x31) and  (proxydata[0] != 0xF1)) { //wrong header received!
      log_message(_F("PROXY Received bad header. Ignoring this data!"));
      if (heishamonSettings.logHexdump) logHex(proxydata, proxylen);
      proxydata_length = 0;
      return; //return so this while loop does not loop forever if there happens to be a continous invalid data stream
    }
  }
  //if ((proxylen > 0) && (proxydata_length == 0 )) proxy_totalreads++; //this is the start of a new read
  proxydata_length +=  proxylen;
  if (proxydata_length > 1 ) { //should have received length part of header now
    if ((proxydata_length > ( proxydata[1] + 3)) || (proxydata_length >= MAXDATASIZE)) {
      sprintf_P(log_msg, PSTR("PROXY Received %i bytes proxy %i\n"), proxydata_length, proxydata[1]);
      log_message(log_msg);
      log_message(_F("PROXY Received more data than header suggests! Ignoring this as this is bad data."));
      proxydata_length = 0;
      if (heishamonSettings.logHexdump) logHex(proxydata, proxydata_length);
      return;
    }
    if (proxydata_length == (proxydata[1] + 3)) { //we received all data (serial2_data[1] is header length field)
      sprintf_P(log_msg, PSTR("PROXY Received %i bytes"), proxydata_length); log_message(log_msg);
      if (heishamonSettings.logHexdump) logHex(proxydata, proxydata_length);
      if (! isValidReceiveChecksum(proxydata,proxydata_length) ) {
        log_message(_F("PROXY Checksum received false!"));
        proxydata_length = 0; //for next attempt
        return;
      }      
      log_message(_F("PROXY Checksum and header received ok!"));
      if ((proxydata[0]==0x71 or proxydata[0]==0xF1) and proxydata_length == (PANASONICQUERYSIZE+1)) { //this is a query from cztaw on proxy port
        if (proxydata[0]==0xf1) {  //this is a write query, just pass this message forward as new command
          log_message(_F("PROXY received write query, copy message forward to heatpump"));
          send_command((byte*)proxydata,proxydata_length-1); //strip CRC, will be calculated again in send_command
          //then just reply with the current settings, for read and write it is the same as the write is only acknowledged in the next read
          //so we just run to the next if statement
        }
        if (proxydata[3] == 0x10) {
          log_message(_F("PROXY requests basic data"));
          if ((actData[0] == 0x71) && (actData[1] == 0xc8) && (actData[2] == 0x01)) { //don't answer if we don't have data
            proxySerial.write(actData,DATASIZE); //should contain valid checksum also
          }
        } else if (proxydata[3] == 0x21 ) {
          log_message(_F("PROXY requests extra data"));
          if ((actDataExtra[0] == 0x71) && (actDataExtra[1] == 0xc8) && (actDataExtra[2] == 0x01)) { //don't answer if we don't have data
            proxySerial.write(actDataExtra,DATASIZE); //should contain valid checksum also
          }
        } else {
          log_message(_F("PROXY has sent unknown query! Forwarding to heatpump!"));
          send_command((byte *)proxydata, proxydata_length-1); //strip CRC from end as send_command wil recalculate it
        }
        proxydata_length = 0;
        return;
      } else if (proxydata[0]==0x31) {
        log_message(_F("PROXY received startup message, forwarding to heatpump!"));
        send_command((byte *)proxydata, proxydata_length-1); //strip CRC from end as send_command wil recalculate it
        proxydata_length = 0;
        return;
      } else {
        log_message(_F("PROXY received unknown message, forwarding it to heatpump anyway!"));
        send_command((byte *)proxydata, proxydata_length-1); //strip CRC from end as send_command wil recalculate it
        proxydata_length = 0;
        return;
      }
    }
  }
}
#endif

bool readSerial()
{
  int len = 0;
  while ((heatpumpSerial.available()) && ((data_length + len) < MAXDATASIZE)) {
    data[data_length + len] = heatpumpSerial.read(); //read available data and place it after the last received data
    len++;
  }

  if ((len > 0) && (data_length == 0 )) totalreads++; //this is the start of a new read
  data_length += len;

  if (data_length > 3) { //should have received length part of header now

    if (((data[0] != 0x71) && (data[0] != 0x31)) || (data[2] != 0x01))  { //wrong header received!
      if (heishamonSettings.logHexdump) {
        log_message(_F("Received bad header. Ignoring this data!"));
        logHex(data, len);
      }
      badheaderread++;
      data_length = 0;
      return false;
    }

    if ((data_length > (data[1] + 3)) || (data_length >= MAXDATASIZE) ) {
      log_message(_F("Received more data than header suggests! Ignoring this as this is bad data."));
      if (heishamonSettings.logHexdump) logHex(data, data_length);
      data_length = 0;
      toolongread++;
      return false;
    }

    if (data_length == (data[1] + 3)) { //we received all data (data[1] is header length field)
      sprintf_P(log_msg, PSTR("Received %d bytes data"), data_length); log_message(log_msg);
      sending = false; //we received an answer after our last command so from now on we can start a new send request again
      if (heishamonSettings.logHexdump) logHex(data, data_length);
      if (! isValidReceiveChecksum(data, data_length) ) {
        log_message(_F("Checksum received false!"));
        data_length = 0; //for next attempt
        badcrcread++;
        return false;
      }
      log_message(_F("Checksum and header received ok!"));
      goodreads++;

      if (data_length == DATASIZE)  {  //receive a full data block
        if  (data[3] == 0x10) { //decode the normal data block
          decode_heatpump_data(data, actData, mqtt_client, log_message, heishamonSettings.mqtt_topic_base, heishamonSettings.updateAllTime);
          if ( (!extraDataBlockAvailable) && ((actData[0] == 0x71) && (actData[0xc7] >= 3)) ) { //do we have valid header and byte 0xc7 is more or equal 3 then assume K&L and more series
            log_message(_F("Extra data available on this heatpump"));
            extraDataBlockAvailable = true; //request for extra data next run
          }
          #ifdef RAWDEBUG
          {
            char mqtt_topic[256];
            sprintf(mqtt_topic, "%s/raw/data", heishamonSettings.mqtt_topic_base);
            mqtt_client.publish(mqtt_topic, (const uint8_t *)actData, DATASIZE, false); //do not retain this raw data
          }
          #endif
          data_length = 0;
          return true;
        } else if (data[3] == 0x21) { //decode the new model extra data block
          extraDataBlockAvailable = true; //set the flag to true so we know we can request this data always
          decode_heatpump_data_extra(data, actDataExtra, mqtt_client, log_message, heishamonSettings.mqtt_topic_base, heishamonSettings.updateAllTime);
          #ifdef RAWDEBUG
          {
            char mqtt_topic[256];
            sprintf(mqtt_topic, "%s/raw/dataextra", heishamonSettings.mqtt_topic_base);
            mqtt_client.publish(mqtt_topic, (const uint8_t *)actDataExtra, DATASIZE, false); //do not retain this raw data
          }
          #endif
          data_length = 0;
          return true;
        } else {
#ifdef ESP8266
          log_message(_F("Received an unknown full size datagram. Can't decode this yet."));
#else 
          log_message(_F("Received a full size datagram but not for me. Forwarding to proxy port."));
          proxySerial.write(data,data_length);
#endif               
          data_length = 0;
          return false;
        }
      }
      else if (data_length == OPTDATASIZE ) { //optional pcb acknowledge answer
        log_message(_F("Received optional PCB ack answer. Decoding this in OPT topics."));
        decode_optional_heatpump_data(data, actOptData, mqtt_client, log_message, heishamonSettings.mqtt_topic_base, heishamonSettings.updateAllTime);
        data_length = 0;
        return true;
      }
      else {
#ifdef ESP8266
        log_message(_F("Received a shorter datagram. Can't decode this yet."));
#else
        log_message(_F("Received a shorter datagram but not for me. Forwarding to proxy port."));
        proxySerial.write(data,data_length);
#endif           
        data_length = 0;
        return false;
      }
    }
  }
  return false;
}

void popCommandBuffer() {
  // to make sure we can pop a command from the buffer
  if ((!sending) && cmdnrel > 0) {
    send_command(cmdbuffer[cmdstart].data, cmdbuffer[cmdstart].length);
    cmdstart = (cmdstart + 1) % (MAXCOMMANDSINBUFFER);
    cmdnrel--;
  }
}

void pushCommandBuffer(byte* command, int length) {
  if (cmdnrel + 1 > MAXCOMMANDSINBUFFER) {
    log_message(_F("Too much commands already in buffer. Ignoring this commands.\n"));
    return;
  }
  cmdbuffer[cmdend].length = length;
  memcpy(&cmdbuffer[cmdend].data, command, length);
  cmdend = (cmdend + 1) % (MAXCOMMANDSINBUFFER);
  cmdnrel++;
}

#ifdef ESP32
void serialTXTask(void *pvParameters) {
  unsigned long lastPCBSendTime = 0;
  unsigned long lastHPSendTime = 0;
  unsigned long lastHPExtraSendTime = 0;
  unsigned long lastPCBSaveTime = 0;
  char local_log_msg[LOG_MSG_SIZE];

  byte localPCBQuery[OPTIONALPCBQUERYSIZE] = {0xF1, 0x11, 0x01, 0x50, 0x00, 0x00, 0x40, 0xFF, 0xFF, 0xE5, 0xFF, 0xFF, 0x00, 0xFF, 0xEB, 0xFF, 0xFF, 0x00, 0x00};
  
  for (;;) {
    unsigned long now = millis();

    if (sending && ((unsigned long)(millis() - sendCommandReadTime) > (SERIALTIMEOUT + OPTIONALPCBQUERYTIME) )) {
      //clear sending flag if taking too long so the optional pcb can still send regulary
      //normally the flag would already be cleared by the readserial timeout but if that process hangs (wifi, mqtt issue) this check will free it anyways
      sending = false;
    }

    // highest priority: optional PCB query every second
    if ((!sending) && ((unsigned long)(now - lastPCBSendTime) >= OPTIONALPCBQUERYTIME)) {
      lastPCBSendTime = now;
      if (heishamonSettings.optionalPCB && !heishamonSettings.listenonly) {
        sending = true;
        sendCommandReadTime = now;
        xQueuePeek(pcbQueue, localPCBQuery, 0);
        byte chk = calcChecksum(localPCBQuery, OPTIONALPCBQUERYSIZE);
        heatpumpSerial.write(localPCBQuery, OPTIONALPCBQUERYSIZE);
        heatpumpSerial.write(chk);
        sprintf_P(local_log_msg, PSTR("optional PCB datagram sent bytes: %d"), OPTIONALPCBQUERYSIZE + 1);
        xQueueSend(logQueue,local_log_msg,0);
      }
      // save to flash periodically
      if ((unsigned long)(now - lastPCBSaveTime) >= (1000 * OPTIONALPCBSAVETIME)) {
        lastPCBSaveTime = now;
        saveOptionalPCB(localPCBQuery, OPTIONALPCBQUERYSIZE);
      }
    }

    // second priority: static heatpump query every waitTime seconds
    if ((!sending) && (!heishamonSettings.listenonly)) {
      if ((unsigned long)(now - lastHPSendTime) >= (1000 * heishamonSettings.waitTime)) {
        sending = true;
        sendCommandReadTime = now;
        lastHPSendTime = now;
        byte chk = calcChecksum(panasonicQuery, PANASONICQUERYSIZE);
        heatpumpSerial.write(panasonicQuery, PANASONICQUERYSIZE);
        heatpumpSerial.write(chk);
        sprintf_P(local_log_msg, PSTR("heatpump request query sent bytes: %d"), PANASONICQUERYSIZE + 1);
        xQueueSend(logQueue,local_log_msg,0);    
      }
    }

    // third priority: extra data block query every waitTime seconds (offset from basic query)
    if ((!sending) && (!heishamonSettings.listenonly) && extraDataBlockAvailable) {
      if ((unsigned long)(now - lastHPExtraSendTime) >= (1000 * heishamonSettings.waitTime)) {
        lastHPExtraSendTime = now;
        sending = true;
        sendCommandReadTime = now;
        panasonicQuery[3] = 0x21;
        byte chk = calcChecksum(panasonicQuery, PANASONICQUERYSIZE);
        heatpumpSerial.write(panasonicQuery, PANASONICQUERYSIZE);
        heatpumpSerial.write(chk);
        panasonicQuery[3] = 0x10;
        xQueueSend(logQueue, (void*)"heatpump extra query sent", 0);
      }
    }    

    // lowest priority: user commands from queue
    if ((!sending) && (!heishamonSettings.listenonly)) {
      struct cmdbuffer_t cmd;
      if (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE) {
        sending = true;
        sendCommandReadTime = now;
        byte chk = calcChecksum(cmd.data, cmd.length);
        heatpumpSerial.write(cmd.data, cmd.length);
        heatpumpSerial.write(chk);
        sprintf_P(local_log_msg, PSTR("Command datagram sent bytes: %d"), cmd.length + 1);
        xQueueSend(logQueue,local_log_msg,0);      
      }
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}
bool send_command(byte* command, int length) {
  if ( heishamonSettings.listenonly ) {
    log_message(_F("Not sending this command. Heishamon in listen only mode!"));
    return false;
  }
  struct cmdbuffer_t cmd;
  cmd.length = length;
  memcpy(&cmd.data, command, length);
  xQueueSend(cmdQueue, &cmd, 0);
  return true;
}

#else

bool send_command(byte* command, int length) {
  if ( heishamonSettings.listenonly ) {
    log_message(_F("Not sending this command. Heishamon in listen only mode!"));
    return false;
  }
  if ( sending ) {
    log_message(_F("Already sending data. Buffering this send request"));
    pushCommandBuffer(command, length);
    return false;
  }
  sending = true; //simple semaphore to only allow one send command at a time, semaphore ends when answered data is received

  byte chk = calcChecksum(command, length);
  int bytesSent = heatpumpSerial.write(command, length); //first send command
  bytesSent += heatpumpSerial.write(chk); //then calculcated checksum byte afterwards
  sprintf_P(log_msg, PSTR("sent bytes: %d including checksum value: %d "), bytesSent, int(chk));
  log_message(log_msg);

  if (heishamonSettings.logHexdump) logHex((char*)command, length);
  sendCommandReadTime = millis(); //set sendCommandReadTime when to timeout the answer of this command
  return true;
}
#endif

// Callback function that is called when a message has been pushed to one of your topics.
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  if (mqttcallbackinprogress) {
    log_message(_F("Already processing another mqtt callback. Ignoring this one"));
  }
  else {
    mqttcallbackinprogress = true; //simple semaphore to make sure we don't have two callbacks at the same time
    char msg[length + 1];
    for (unsigned int i = 0; i < length; i++) {
      msg[i] = (char)payload[i];
    }
    msg[length] = '\0';

    // copy topic to the heap so a later mqtt publish can't clobber PubSubClient's buffer
    char* topiccopy = (char*) malloc(strlen(topic) + 1);
    if (topiccopy) {
      memcpy(topiccopy, topic, strlen(topic) + 1);
      topic = topiccopy;	
    }
	  
	char* topic_command = topic + strlen(heishamonSettings.mqtt_topic_base) + 1; //strip base plus seperator from topic
    if (strcmp(topic_command, mqtt_send_raw_value_topic) == 0)
    { // send a raw hex string
      byte *rawcommand;
      rawcommand = (byte *) malloc(length);
      memcpy(rawcommand, msg, length);

      sprintf_P(log_msg, PSTR("sending raw value"));
      log_message(log_msg);
      send_command(rawcommand, length);
      free(rawcommand);
    } else if (strncmp(topic_command, mqtt_topic_s0, strlen(mqtt_topic_s0)) == 0)  // this is a s0 topic, check for watthour topic and restore it
    {
      char* topic_s0_watthour_port = topic_command + strlen(mqtt_topic_s0) + 15; //strip the first 17 "s0/WatthourTotal/" from the topic to get the s0 port
      int s0Port = String(topic_s0_watthour_port).toInt();
      float watthour = String(msg).toFloat();
      restore_s0_Watthour(s0Port, watthour);
      //unsubscribe after restoring the watthour values
      char mqtt_topic[256];
      sprintf(mqtt_topic, "%s", topic);
      if (mqtt_client.unsubscribe(mqtt_topic)) {
        log_message(_F("Unsubscribed from S0 watthour restore topic"));
      }
    } else if (strncmp(topic_command, mqtt_topic_commands, strlen(mqtt_topic_commands)) == 0)  // check for commands to heishamon
    {
      char* topic_sendcommand = topic_command + strlen(mqtt_topic_commands) + 1; //strip the first 9 "commands/" from the topic to get what we need
      send_heatpump_command(topic_sendcommand, msg, send_command, log_message, heishamonSettings.optionalPCB);
    //use this to receive valid heishamon raw data from other heishamon to debug this OT code
#ifdef RAWDEBUG
    } else if (strcmp((char*)"panasonic_heat_pump/raw/data", topic) == 0) {  // check for raw heatpump input
      sprintf_P(log_msg, PSTR("Received raw heatpump data from MQTT"));
      log_message(log_msg);
      decode_heatpump_data(msg, actData, mqtt_client, log_message, heishamonSettings.mqtt_topic_base, heishamonSettings.updateAllTime);
      memcpy(actData, msg, DATASIZE);
#endif
    } else if (strncmp(topic_command, mqtt_topic_opentherm_read, strlen(mqtt_topic_opentherm_read)) == 0)  {
      char* topic_otcommand = topic_command + strlen(mqtt_topic_opentherm_read) + 1; //strip the opentherm subtopic from the topic
      mqttOTCallback(topic_otcommand, msg);
    } else if (strncmp(topic_command, mqtt_topic_gpio, strlen(mqtt_topic_gpio)) == 0)  {
      char* topic_gpiocommand = topic_command + strlen(mqtt_topic_gpio) + 1; //strip the gpio subtopic from the topic
      mqttGPIOCallback(topic_gpiocommand, msg);
    } else if (strncmp(topic_command, mqtt_topic_1wire, strlen(mqtt_topic_1wire)) == 0) { //this is a 1wire address topic, restore its retained value at boot
      char* topic_1wire_address = topic_command + strlen(mqtt_topic_1wire) + 1; //strip the "1wire/" from the topic to get the sensor address
      if ((strchr(topic_1wire_address, '/') == NULL) && (length > 0)) { //only handle the address topic itself (not .../alias) and skip empty (cleared) retained messages
        restoreDallasFromMqtt(topic_1wire_address, String(msg).toFloat(), log_message);
      }
    }
    free(topiccopy);
    mqttcallbackinprogress = false;
  }
}

void setupOTA() {
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(heishamonSettings.wifi_hostname);

  // Set authentication
  ArduinoOTA.setPassword(heishamonSettings.ota_password);

  ArduinoOTA.onStart([]() {
  });
  ArduinoOTA.onEnd([]() {
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {

  });
  ArduinoOTA.onError([](ota_error_t error) {

  });
  ArduinoOTA.begin();
}



int8_t webserver_cb(struct webserver_t *client, void *dat) {
  

  switch (client->step) {
    case WEBSERVER_CLIENT_REQUEST_METHOD: {
        if (strcmp_P((char *)dat, PSTR("POST")) == 0) {
          client->route = 110;
        }
        return 0;
      } break;
    case WEBSERVER_CLIENT_REQUEST_URI: {
        if (strcmp_P((char *)dat, PSTR("/")) == 0) {
          client->route = 1;
        } else if (strcmp_P((char *)dat, PSTR("/json")) == 0) {
          client->route = 20;
        } else if (strcmp_P((char *)dat, PSTR("/reboot")) == 0) {
          client->route = 30;
        } else if (strcmp_P((char *)dat, PSTR("/debug")) == 0) {
          client->route = 40;
          log_message(_F("Debug URL requested"));
        } else if (strcmp_P((char *)dat, PSTR("/wifiscan")) == 0) {
          client->route = 50;
        } else if (strcmp((char *)dat, "/dallasalias") == 0) {
          client->route = 60;
        } else if (strcmp((char *)dat, "/removedallas") == 0) {
          client->route = 190;
        } else if (strcmp((char *)dat, "/togglelog") == 0) {
          client->route = 1;
          log_message(_F("Toggled mqtt log flag"));
          heishamonSettings.logMqtt ^= true;
        } else if (strcmp_P((char *)dat, PSTR("/togglehexdump")) == 0) {
          client->route = 1;
          log_message(_F("Toggled hexdump log flag"));
          heishamonSettings.logHexdump ^= true;
        } else if (strcmp_P((char *)dat, PSTR("/connecttest.txt")) == 0 ||
                   strcmp_P((char *)dat, PSTR("/ncsi.txt")) == 0 ||
                   strcmp_P((char *)dat, PSTR("/redirect")) == 0 ||
                   strcmp_P((char *)dat, PSTR("/fwlink")) == 0 ||
                   strcmp_P((char *)dat, PSTR("/generate_204")) == 0 ||
                   strcmp_P((char *)dat, PSTR("/gen_204")) == 0 ||
                   strcmp_P((char *)dat, PSTR("/popup")) == 0) {
          client->route = 80; //for Android/Windows devices
        } else if (strcmp_P((char *)dat, PSTR("/hotspot-detect.html")) == 0 ) {
          client->route = 81;  //for Apple devices
        } else if (strcmp_P((char *)dat, PSTR("/factoryreset")) == 0) {
          client->route = 90;
        } else if (strcmp_P((char *)dat, PSTR("/command")) == 0) {
          if ((client->userdata = malloc(1)) == NULL) {
            loggingSerial.printf(PSTR("Out of memory %s:#%d\n"), __FUNCTION__, __LINE__);
            ESP.restart();
            exit(-1);
          }
          ((char *)client->userdata)[0] = 0;
          client->route = 100;
        } else if (client->route == 110) {
          // Only accept settings POST requests
          if (strcmp_P((char *)dat, PSTR("/savesettings")) == 0) {
            client->route = 110;
          } else if (strcmp_P((char *)dat, PSTR("/saverules")) == 0) {
            client->route = 170;
            if (LittleFS.begin()) {
              LittleFS.remove("/rules.new");
              client->userdata = new File(LittleFS.open("/rules.new", "a+"));
            }
#ifdef TLS_SUPPORT
        } else if (strcmp_P((char *)dat, PSTR("/cacert")) == 0) {
          client->route = 165; 
          if (LittleFS.begin()) {
            LittleFS.remove("/ca.tmp");
            File cf = LittleFS.open("/ca.tmp", "w");
            if (cf) {
              client->userdata = new File(cf);
            }
            new_ca_stored = true;
          }
#endif
          } else if (strcmp_P((char *)dat, PSTR("/firmware")) == 0) {
            if (!Update.isRunning()) {
#ifdef ESP8266
              Update.runAsync(true);
#endif
              if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
                Update.printError(loggingSerial);
                return -1;
              } else {
                client->route = 150;
              }
            } else {
              loggingSerial.println(PSTR("New firmware update client, while previous isn't finished yet! Assume broken connection, abort!"));
              Update.end();
              return -1;
            }
          } else {
            return -1;
          }
        } else if (strcmp_P((char *)dat, PSTR("/settings")) == 0) {
          client->route = 120;
        } else if (strcmp_P((char *)dat, PSTR("/getsettings")) == 0) {
          client->route = 130;
        } else if (strcmp_P((char *)dat, PSTR("/firmware")) == 0) {
          client->route = 140;
        } else if (strcmp_P((char *)dat, PSTR("/rules")) == 0) {
          client->route = 160;
#ifdef TLS_SUPPORT
        } else if (strcmp_P((char *)dat, PSTR("/cacert")) == 0) {
          client->route = 166; 
#endif
        } else if (strcmp_P((char *)dat, PSTR("/scandallas")) == 0) {
          client->route = 180;          
        } else {
          client->route = 0;
        }

        return 0;
      } break;
    case WEBSERVER_CLIENT_ARGS: {
        struct arguments_t *args = (struct arguments_t *)dat;
        switch (client->route) {
          case 60: {
              sprintf_P(log_msg, PSTR("Dallas alias changed address %s to alias %s"), args->name, args->value);
              log_message(log_msg);
              changeDallasAlias((char *)args->name, (char *)args->value);
              return 0;
            } break;
          case 190: {
              removeDallasSensor(mqtt_client, heishamonSettings.mqtt_topic_base, (char *)args->name, log_message);
              return 0;
            } break;
          case 100: {
              unsigned char cmd[256] = { 0 };
              char cpy[args->len + 1];
              char log_msg[256] = { 0 };
              unsigned int len = 0;

              memset(&cpy, 0, args->len + 1);
              snprintf((char *)&cpy, args->len + 1, "%.*s", args->len, args->value);

              for (uint8_t x = 0; x < sizeof(commands) / sizeof(commands[0]); x++) {
                cmdStruct tmp;
                memcpy_P(&tmp, &commands[x], sizeof(tmp));
                if (strcmp((char *)args->name, tmp.name) == 0) {
                  len = tmp.func(cpy, cmd, log_msg);
                  if ((client->userdata = realloc(client->userdata, strlen((char *)client->userdata) + strlen(log_msg) + 2)) == NULL) {
                    loggingSerial.printf(PSTR("Out of memory %s:#%d\n"), __FUNCTION__, __LINE__);
                    ESP.restart();
                    exit(-1);
                  }
                  strcat((char *)client->userdata, log_msg);
                  strcat((char *)client->userdata, "\n");
                  log_message(log_msg);
                  send_command(cmd, len);
                }
              }

              memset(&cmd, 0, 256);
              memset(&log_msg, 0, 256);

              if (heishamonSettings.optionalPCB) {
                //optional commands
                for (uint8_t x = 0; x < sizeof(optionalCommands) / sizeof(optionalCommands[0]); x++) {
                  optCmdStruct tmp;
                  memcpy_P(&tmp, &optionalCommands[x], sizeof(tmp));
                  if (strcmp((char *)args->name, tmp.name) == 0) {
                    len = tmp.func(cpy, log_msg);
                    if ((client->userdata = realloc(client->userdata, strlen((char *)client->userdata) + strlen(log_msg) + 2)) == NULL) {
                      loggingSerial.printf(PSTR("Out of memory %s:#%d\n"), __FUNCTION__, __LINE__);
                      ESP.restart();
                      exit(-1);
                    }
                    strcat((char *)client->userdata, log_msg);
                    strcat((char *)client->userdata, "\n");
                    log_message(log_msg);
#ifdef ESP32
                    xQueueOverwrite(pcbQueue, optionalPCBQuery);
#endif
                  }
                }
              }
            } break;
          case 110: {
              return cacheSettings(client, args);
            } break;
          case 150: {
              if (Update.isRunning() && (!Update.hasError())) {
                if ((strcmp((char *)args->name, "md5") == 0) && (args->len > 0)) {
                  char md5[args->len + 1];
                  memset(&md5, 0, args->len + 1);
                  snprintf((char *)&md5, args->len + 1, "%.*s", args->len, args->value);
                  sprintf_P(log_msg, PSTR("Firmware MD5 expected: %s"), md5);
                  log_message(log_msg);
                  if (!Update.setMD5(md5)) {
                    log_message(_F("Failed to set expected update file MD5!"));
                    Update.end(false);
                  }
                } else if (strcmp((char *)args->name, "firmware") == 0) {
                  if (Update.write((uint8_t *)args->value, args->len) != args->len) {
                    Update.printError(loggingSerial);
                    Update.end(false);
                  } else {
                    if (uploadpercentage != (unsigned int)(((float)client->readlen / (float)client->totallen) * 20)) {
                      uploadpercentage = (unsigned int)(((float)client->readlen / (float)client->totallen) * 20);
                      sprintf_P(log_msg, PSTR("Uploading new firmware: %d%%"), uploadpercentage * 5);
                      log_message(log_msg);
                    }
                  }
                }
              } else {
                log_message((char*)"New firmware POST data but update not running anymore!");
              }
            } break;
          case 170: {
              File *f = (File *)client->userdata;
              if (!f || !*f) {
                client->route = 160;
              } else {
                f->write(args->value, args->len);
              }
            } break;
#ifdef TLS_SUPPORT            
          case 165: {
              File *f = (File *)client->userdata;
              if (f && *f && args->len > 0) {
                  f->write((const uint8_t*)args->value, (size_t)args->len);
              }
              return 0;
            } break;
#endif
        }
      } break;
    case WEBSERVER_CLIENT_HEADER: {
        struct arguments_t *args = (struct arguments_t *)dat;
        return 0;
      } break;
    case WEBSERVER_CLIENT_WRITE: {
        switch (client->route) {
          case 0: {
              if (client->content == 0) {
                webserver_send(client, 404, (char *)"text/plain", 13);
                webserver_send_content_P(client, PSTR("404 Not found"), 13);
              }
              return 0;
            } break;
          case 1: {
              return handleRoot(client, readpercentage, mqttReconnects, &heishamonSettings);
            } break;
          case 20: {
              return handleJsonOutput(client, actData, actDataExtra, actOptData, &heishamonSettings, extraDataBlockAvailable);
            } break;
          case 30: {
              return handleReboot(client);
            } break;
          case 40: {
              if (client->content == 0) {
                webserver_send(client, 200, (char *)"text/plain", 0);
              } else if (client->content == 1) {
                webserver_send_content_P(client, PSTR("-- heatpump data --\n"), 20);
                handleDebug(client, (char *)actData, 203);
              } else if ((client->content == 2) && extraDataBlockAvailable) {
                webserver_send_content_P(client, PSTR("-- extra data --\n"), 17);
                handleDebug(client, (char *)actDataExtra, 203);
              }
              return 0;
            } break;
          case 50: {
              return handleWifiScan(client);
            } break;
          case 60: {
              return 0;
            } break;
          case 190: {
              return 0;
            } break;
          case 80: {
              if (client->content == 0) {
                webserver_send(client, 302, (char *)"text/html", 0);
              }
              return 0;
            } break;
          case 81: {
              if (client->content == 0) {
                static const char body[] PROGMEM =
                  "<HTML><HEAD><TITLE>HeishaMon Setup</TITLE>"
                  "<META name='viewport' content='width=device-width,initial-scale=1'>"
                  "</HEAD><BODY>"
                  "<h2>HeishaMon Setup</h2>"
                  "<p><a href='http://192.168.4.1/settings'>Open Settings</a></p>"
                  "</BODY></HTML>";
                webserver_send(client, 200, (char *)"text/html", strlen(body));
                webserver_send_content_P(client, body, strlen(body));
              }
              return 0;
            } break;            
          case 90: {
              return handleFactoryReset(client);
            } break;
          case 100: {
              if (client->content == 0) {
                webserver_send(client, 200, (char *)"text/plain", 0);
                char *RESTmsg = (char *)client->userdata;
                webserver_send_content(client, (char *)RESTmsg, strlen(RESTmsg));
                free(RESTmsg);
                client->userdata = NULL;
              }
              return 0;
            } break;
          case 110: {
              int ret = saveSettings(client, &heishamonSettings);
              #ifdef ESP8266
              if ((!heishamonSettings.opentherm) && (heishamonSettings.listenonly)) {
                //make sure we disable TX to heatpump-RX using the mosfet so this line is floating and will not disturb cz-taw1
                //does not work for opentherm version currently
                digitalWrite(ENABLEPIN, LOW);
              } else {
                digitalWrite(ENABLEPIN, HIGH);
              }
              #else
              if (heishamonSettings.listenonly) {
                digitalWrite(ENABLEPIN, LOW);
              } else {
                digitalWrite(ENABLEPIN, HIGH);
              }
              if (!heishamonSettings.opentherm) {
                digitalWrite(ENABLEOTPIN, LOW);
              } else {
                digitalWrite(ENABLEOTPIN, HIGH);
              }
              #endif
              switch (client->route) {
                case 111: {
                    return settingsNewPassword(client, &heishamonSettings);
                  } break;
                case 112: {
                    return settingsReconnectWifi(client, &heishamonSettings);
                  } break;
                case 113: {
                    webserver_send(client, 301, (char *)"text/plain", 0);
                  } break;
              }
              return 0;
            } break;
          case 111: {
              return settingsNewPassword(client, &heishamonSettings);
            } break;
          case 112: {
              return settingsReconnectWifi(client, &heishamonSettings);
            } break;
          case 120: {
              return handleSettings(client);
            } break;
          case 130: {
              return getSettings(client, &heishamonSettings);
            } break;
          case 140: {
              return showFirmware(client);
            } break;
          case 150: {
              log_message((char*)"In /firmware client write part");
              if (Update.isRunning()) {
                if (Update.end(true)) {
                  log_message((char*)"Firmware update success");
                  timerqueue_insert(2, 0, -2); // Start reboot sequence
                  return showFirmwareSuccess(client);
                } else {
                  Update.printError(loggingSerial);
                  return showFirmwareFail(client);
                }
              }
              return 0;
            } break;
          case 160: {
              return showRules(client);
            } break;
#ifdef TLS_SUPPORT
        case 165: {
          if (client->userdata) {
            File *pf = (File *)client->userdata;
            pf->close();
            delete pf;
            client->userdata = NULL;
          }
          return handleCACert(client);
        } break;
        case 166: {
          return showCACert(client);
        } break; 
#endif  
          case 170: {
              File *f = (File *)client->userdata;
              if (f) {
                if (*f) {
                  f->close();
                }
                delete f;
              }
              client->userdata = NULL;
              timerqueue_insert(0, 1, -4);
              webserver_send(client, 301, (char *)"text/plain", 0);

            } break;
          case 180: {
              if (heishamonSettings.use_1wire) rescanDallasSensors(log_message, heishamonSettings.dallasResolution);
            } break;
          default: {
              webserver_send(client, 301, (char *)"text/plain", 0);
            } break;
        }
        return -1;
      } break;
    case WEBSERVER_CLIENT_CREATE_HEADER: {
        struct header_t *header = (struct header_t *)dat;
        switch (client->route) {
          case 113: {
              header->ptr += sprintf_P((char *)header->buffer, PSTR("Location: /settings"));
              return -1;
            } break;
          case 60:
          case 70:
          case 190: {
              header->ptr += sprintf_P((char *)header->buffer, PSTR("Location: /"));
              return -1;
            } break;
          case 80: {
              header->ptr += sprintf_P((char *)header->buffer, 
              PSTR("Location: http://192.168.4.1/settings"));
              return -1;
            } break;            
          case 170: {
              header->ptr += sprintf_P((char *)header->buffer, PSTR("Location: /rules"));
              return -1;
            } break;
          default: {
              if (client->route != 0) {
                header->ptr += sprintf_P((char *)header->buffer, PSTR("Access-Control-Allow-Origin: *"));
              }
            } break;
        }
        return 0;
      } break;
    case WEBSERVER_CLIENT_CLOSE: {
        switch (client->route) {
          case 100: {
              if (client->userdata != NULL) {
                free(client->userdata);
              }
            } break;
          case 110: {
              struct websettings_t *tmp = NULL;
              while (client->userdata) {
                tmp = (struct websettings_t *)client->userdata;
                client->userdata = ((struct websettings_t *)(client->userdata))->next;
                free(tmp);
              }
            } break;
          case 160:
#ifdef TLS_SUPPORT
          case 165:
#endif
          case 170: {
              if (client->userdata != NULL) {
                File *f = (File *)client->userdata;
                if (f) {
                  if (*f) {
                    f->close();
                  }
                  delete f;
                }
              }
            } break;
        }
        client->userdata = NULL;
      } break;
      default: {
        return 0;
      } break;
  }

  return 0;
}

void setupHttp() {
  webserver_start(80, &webserver_cb, 0);
}

void factoryReset() {
    loggingSerial.println("Factory reset request detected, clearing config."); 
    LittleFS.format();
    //create first boot file
    File startupFile = LittleFS.open("/heishamon", "w");
    startupFile.close();
    WiFi.persistent(true);
    WiFi.disconnect();
    WiFi.persistent(false);
    loggingSerial.println("Config cleared. Please reset to configure this device...");
    //initiate debug led indication for factory reset
#if defined(ESP8266)
    pinMode(LEDPIN, FUNCTION_0); //set it as gpio
    pinMode(LEDPIN, OUTPUT);
    while (true) {
      digitalWrite(LEDPIN, HIGH);
      delay(100);
      digitalWrite(LEDPIN, LOW);
      delay(100);
      yield();
    }
#else
    while (true) {
     delay(100);
     pixels.setPixelColor(0, 128, 0, 0);
     pixels.show();
     delay(100);
     pixels.setPixelColor(0, 0, 0, 128);
     pixels.show();
    }
#endif
}
void doubleResetDetect() {
  if (LittleFS.exists("/doublereset")) {
    factoryReset();
  }
  File doubleresetFile = LittleFS.open("/doublereset", "w");
  doubleresetFile.close();
}

void setupSerial() {
#if defined(ESP8266)
  //boot issue's first on normal serial
  heatpumpSerial.begin(115200);
  heatpumpSerial.flush();
#endif
  if (heishamonSettings.logSerial1) { //settings are not loaded yet, this is the startup default
    loggingSerial.begin(115200);
    //debug line on serial1 (D4, GPIO2)
#ifdef ESP32
    delay(100); //to let USB CDC to be opened if necessary
#endif    
    loggingSerial.print(F("Starting debugging, version: "));
    loggingSerial.println(heishamon_version);
  }
#if defined(ESP8266)
  else {
    pinMode(LEDPIN, FUNCTION_0); //set it as gpio
  }
#elif defined(ESP32)
  pixels.begin();
  pixels.clear();
  pixels.setPixelColor(0, 16, 0, 0);
  pixels.show(); 
#endif
}

void switchSerial() {
#if defined(ESP8266)
  loggingSerial.println(F("Switching serial to connect to heatpump. Look for debug on serial1 (GPIO2) and mqtt log topic."));
  //serial to cn-cnt
  heatpumpSerial.flush();
  heatpumpSerial.end();
  heatpumpSerial.begin(9600, SERIAL_8E1); //on normal tx/rx esp8266
  heatpumpSerial.flush();
  //swap to gpio13 (D7) and gpio15 (D8)
  heatpumpSerial.swap();
  //turn on GPIO's on tx/rx for opentherm part
  pinMode(1, FUNCTION_3);
  pinMode(3, FUNCTION_3);
#elif defined(ESP32)
  // need to create new serial startup config for ESP32
  heatpumpSerial.flush();
  heatpumpSerial.end();
  heatpumpSerial.begin(9600, SERIAL_8E1,HEATPUMPRX,HEATPUMPTX);
  heatpumpSerial.flush();
  proxySerial.flush();
  proxySerial.end();
  proxySerial.begin(9600, SERIAL_8E1,PROXYRX,PROXYTX);
  proxySerial.flush();  
#endif

  setupGPIO(heishamonSettings.gpioSettings); //switch extra GPIOs to configured mode

  //mosfet output enable
  pinMode(ENABLEPIN, OUTPUT);
  #if defined (ESP32)
  //OT 24v booster disable from boot
  pinMode(ENABLEOTPIN, OUTPUT);
  digitalWrite(ENABLEOTPIN, LOW);
  #endif

  //try to detect if cz-taw1 is connected in parallel
  if (!heishamonSettings.listenonly) {
    if (heatpumpSerial.available() > 0) {
      log_message(_F("There is data on the line without asking for it. Switching to listen only mode."));
      heishamonSettings.listenonly = true;
    }
    else {
      //enable gpio15 after boot using gpio5 (D1) which enables the level shifter for the tx to panasonic
      //do not enable if listen only to keep the line floating
      digitalWrite(ENABLEPIN, HIGH);
    }
  }
}

void setupMqtt() {
  mqtt_client.setBufferSize(1024);
#ifdef TLS_SUPPORT
  mqtt_client.setSocketTimeout(8); mqtt_client.setKeepAlive(30); //fast timeout, any slower than 10s will block the main loop too long (8s might be even safer to avoid reboots on bad wifi); short keepalive may lead to problems with TLS
  if (heishamonSettings.mqtt_tls_enabled) {
    if (mqtt_tls_client == nullptr) {
        mqtt_tls_client = new WiFiClientSecure();
    }
    if (!loadTlsCaFromFS(mqtt_tls_client)) {
      log_message(_F("[TLS] Proceeding without valid CA (expect failure)"));
    }
    mqtt_client.setClient(*mqtt_tls_client );
  } else {
    mqtt_client.setClient(mqtt_wifi_client);
  }
  last_tls_enabled = heishamonSettings.mqtt_tls_enabled;
#else
  mqtt_client.setSocketTimeout(10); mqtt_client.setKeepAlive(5); //fast timeout, any slower will block the main loop too long
#endif
  mqtt_client.setServer(heishamonSettings.mqtt_server, atoi(heishamonSettings.mqtt_port));
  mqtt_client.setCallback(mqtt_callback);
}

void setupConditionals() {

#ifdef ESP32
  pcbQueue = xQueueCreate(1, OPTIONALPCBQUERYSIZE);
  cmdQueue = xQueueCreate(MAXCOMMANDSINBUFFER, sizeof(cmdbuffer_t));
  logQueue = xQueueCreate(4, LOG_MSG_SIZE);
  
  xTaskCreatePinnedToCore(
    serialTXTask,
    "serialTXTask",
    8192,
    NULL,
    1,
    NULL,
    1
  );
#endif

  //send_initial_query(); //maybe necessary but for now disable. CZ-TAW1 sends this query on boot

  //load optional PCB data from flash
  if (heishamonSettings.optionalPCB) {
    if (loadOptionalPCB(optionalPCBQuery, OPTIONALPCBQUERYSIZE)) {
      log_message(_F("Succesfully loaded optional PCB data from saved flash!"));
    }
    else {
      log_message(_F("Failed to load optional PCB data from flash!"));
    }
#ifdef ESP32
    //insert on task queue
    xQueueOverwrite(pcbQueue, optionalPCBQuery);
#else
    delay(1500); //need 1.5 sec delay before sending first datagram
    send_optionalpcb_query(); //send one datagram already at start
    lastOptionalPCBRunTime = millis();
#endif
  }

  //these two after optional pcb because it needs to send a datagram fast after boot
  if (heishamonSettings.use_1wire) initDallasSensors(log_message, heishamonSettings.updataAllDallasTime, heishamonSettings.waitDallasTime, heishamonSettings.dallasResolution);
  if (heishamonSettings.use_s0) initS0Sensors(heishamonSettings.s0Settings);


}

void timer_cb(int nr) {
  if (nr > 0) {
    rules_timer_cb(nr);
  } else {
    switch (nr) {
      case -1: {
          LittleFS.begin();
          LittleFS.format();
          //create first boot file
          File startupFile = LittleFS.open("/heishamon", "w");
          startupFile.close(); 
          WiFi.disconnect(true);
          timerqueue_insert(1, 0, -2);
        } break;
      case -2: {
          ESP.restart();
        } break;
      case -3: {
          setupWifi(&heishamonSettings);
        } break;
      case -4: {
          int ret = rules_parse((char*)"/rules.new");
          if (ret == -2) {
            //we received an empty rules.new file which means delete all rules
            LittleFS.remove("/rules.txt");
            LittleFS.remove("/rules.new");
            rules_deinitialize();
          } else if (ret == -1) {
            log_message(_F("Failed to load new rules, reverting back to older rules!"));
            rules_parse((char*)"/rules.txt");
          } else {
            if (LittleFS.begin()) {
              LittleFS.rename("/rules.new", "/rules.txt");
            }
          }
          rules_boot();
        } break;
      case -5: {
          ntpReload(&heishamonSettings);
          logprintln_P(F("Resynced with NTP servers. Next sync after 24 hours."));
          timerqueue_insert(86400, 0, -5);
        } break;
      case -6: {
          time_t now = time(NULL);
          struct tm *tm_struct = localtime(&now);
          if(tm_struct->tm_year == 70) {
            /*
             * No valid time yet since reboot. Retry every 5 min
             */
            ntpReload(&heishamonSettings);
            logprintln_P(F("Still trying to sync with ntp servers. Checking again in 5 minutes"));
            timerqueue_insert(300, 0, -6);
          } else {
            /*
             * Wait 300 sec less than a full day
             */
            logprintln_P(F("Successfully synced with ntp servers. Next sync after 24 hours."));
            timerqueue_insert(86100, 0, -5);
          }
        } break;
    }
  }

}


void setup() {
  //first get total memory before we do anything
  getFreeMemory();
  //set boottime
  char *up = getUptime();
  free(up);

  inSetup = true;

  setupSerial();

  loggingSerial.println();
  loggingSerial.println(F("--- HEISHAMON ---"));
  loggingSerial.println(F("starting..."));

  //first boot check, to visually confirm good flash
  //this also formats the littlefs if necessary
#if defined(ESP8266)
  if (LittleFS.begin()) {
#else
  loggingSerial.println(F("Starting littlefs..."));
  if (LittleFS.begin(true)) {
    loggingSerial.println(F("Started littlefs..."));
#endif
    loggingSerial.println(F("Checking littlefs for first boot..."));
    if (LittleFS.exists("/heishamon")) {
      //normal boot
      loggingSerial.println(F("Heishamon boot file exists, normal boot..."));
    } else if (LittleFS.exists("/config.json")) {
      loggingSerial.println(F("Heishamon config file exists, create boot file..."));
      //from old firmware, create file and then normal boot
      File startupFile = LittleFS.open("/heishamon", "w");
      startupFile.close();
    } else {
      //first boot
      loggingSerial.println(F("Heishamon boot file missing, first start..."));
      File startupFile = LittleFS.open("/heishamon", "w");
      startupFile.close();    
#if defined(ESP8266)
      pinMode(LEDPIN, FUNCTION_0); //set it as gpio
      pinMode(LEDPIN, OUTPUT);
      while (true) {
        digitalWrite(LEDPIN, HIGH);
        delay(50);
        digitalWrite(LEDPIN, LOW);
        delay(50);
        yield();
      }
#else
      while (true) {
        delay(50);
        pixels.setPixelColor(0, 128, 0, 0);
        pixels.show();
        delay(50);
        pixels.setPixelColor(0, 0, 0, 128);
       pixels.show();
      }
#endif      
    }
  }
  //double reset detect from start - removed, using boot button now
  //loggingSerial.println(F("Check for double reset..."));
  //doubleResetDetect();

  pinMode(BOOTPIN,INPUT_PULLUP); //enable the boot switch to be used as an input after booting

  loggingSerial.println(F("Send current wifi info to serial..."));
  WiFi.printDiag(loggingSerial);

  loggingSerial.println(F("Loading config from flash..."));
  loadSettings(&heishamonSettings);

  loggingSerial.println(F("Setup wifi..."));
  setupWifi(&heishamonSettings);
  lastWifiRetryTimer = millis();

#if defined(ESP32)
  loggingSerial.println(F("Setup ethernet module..."));
  setupETH();
#endif

  loggingSerial.println(F("Setup HTTP..."));
  setupHttp();

  loggingSerial.println(F("Setup SNTP..."));
#if defined(ESP8266)
  sntp_stop();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_init();
#else
  loggingSerial.println(F("SNTP setup deferred until network is up"));
#endif

  loggingSerial.println(F("Setup MQTT..."));
  setupMqtt();

  loggingSerial.println(F("Switch serial..."));
  switchSerial(); //switch serial to gpio13/gpio15

  loggingSerial.println(F("Sending new wifi diag..."));
  WiFi.printDiag(loggingSerial);

  loggingSerial.println(F("Settings conditionals..."));
  setupConditionals(); //setup for routines based on settings

  loggingSerial.println(F("Settings DNS..."));
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  loggingSerial.println(F("Check OT config..."));
  //OT begin must be after serial setup
  if (heishamonSettings.opentherm) {
    #if defined(ESP8266)
    //always enable mosfets if opentherm is used
    digitalWrite(ENABLEPIN, HIGH);
    #else
    //dedicated OT enable pin on ESP32 model
    digitalWrite(ENABLEOTPIN, HIGH);
    #endif
    HeishaOTSetup();
  }

  loggingSerial.println(F("Enabling rules.."));
  if (heishamonSettings.force_rules == false) {
#if defined(ESP8266)
  rst_info *resetInfo = ESP.getResetInfoPtr();
  loggingSerial.printf(PSTR("Reset reason: %d, exception cause: %d\n"), resetInfo->reason, resetInfo->exccause);
    if (resetInfo->reason > 0 && resetInfo->reason < 4) {
#elif defined(ESP32)
      esp_reset_reason_t reset_reason = esp_reset_reason();
      loggingSerial.printf(PSTR("Reset reason: %d\n"), reset_reason);
    if (reset_reason > 3 && reset_reason < 12) {  //is this correct for esp32?
#endif  
        loggingSerial.println("Not loading rules due to crash reboot!");
    } else {
      rules_parse((char *)"/rules.txt");
      rules_boot();
    }
  } else {
    rules_parse((char *)"/rules.txt");
    rules_boot();
  }

  delay(200); //small delay to allow double reset
  #ifdef ESP32
  //turn off neopixel to indicate end of setup
  neoPixelState = pixels.Color(0,0,0);
  pixels.setPixelColor(0, neoPixelState);
  pixels.show(); 
  #endif
  //end of setup, clear double reset flag
  //loggingSerial.println(F("Clearing double reset flag.."));
  //LittleFS.remove("/doublereset");  
  //loggingSerial.println(F("End of setup.."));

  inSetup = false;
}

void send_initial_query() {
  log_message(_F("Requesting initial start query"));
  send_command(initialQuery, INITIALQUERYSIZE);

}

void send_panasonic_query() {
  log_message(_F("Requesting new panasonic data"));
  send_command(panasonicQuery, PANASONICQUERYSIZE);
  // rest is for the new data block on new models
  if (extraDataBlockAvailable) {
    log_message(_F("Requesting new panasonic extra data"));
    panasonicQuery[3] = 0x21; //setting 4th byte to 0x21 is a request for extra block
    send_command(panasonicQuery, PANASONICQUERYSIZE);
    panasonicQuery[3] = 0x10; //setting 4th back to 0x10 for normal data request next time
  }
}

#ifdef ESP8266
void send_optionalpcb_query() {
  log_message(_F("Sending optional PCB data"));
  send_command(optionalPCBQuery, OPTIONALPCBQUERYSIZE);
}
#endif

void readHeatpump() {
  if (sending && ((unsigned long)(millis() - sendCommandReadTime) > SERIALTIMEOUT)) {
    log_message(_F("Previous read data attempt failed due to timeout!"));
    sprintf_P(log_msg, PSTR("Received %d bytes data"), data_length);
    log_message(log_msg);
    if (heishamonSettings.logHexdump) logHex(data, data_length);
    if (data_length == 0) {
      timeoutread++;
      totalreads++; //at at timeout we didn't receive anything but did expect it so need to increase this for the stats
    } else {
      tooshortread++;
    }
    data_length = 0; //clear any data in array
    sending = false; //receiving the answer from the send command timed out, so we are allowed to send a new command
  }
  if ( (heishamonSettings.listenonly || sending) && (heatpumpSerial.available() > 0)) readSerial();
}

void checkBootButton() {
  if (digitalRead(BOOTPIN)) { //true = 1, not pressed
    bootButtonNotPressed = millis();
  } else {
      if ((unsigned long)(millis() - bootButtonNotPressed) > 10000) {
        //initiate factory reset
        factoryReset();
      }
  }
}

void loop() {
  //check boot button state
  checkBootButton();

  //webserver function
  webserver_loop();

  // check wifi
  check_wifi();
  // Handle OTA first.s
  ArduinoOTA.handle();

  mqtt_client.loop();

  if (heishamonSettings.opentherm) {
    HeishaOTLoop(actData, mqtt_client, heishamonSettings.mqtt_topic_base);
  }

  readHeatpump();

#ifdef ESP32
  if (heishamonSettings.proxy) readProxy();

  if (logQueue != NULL) {
    if (xQueueReceive(logQueue, log_msg, 0) == pdTRUE) {
      log_message(log_msg);
    }
  }
#endif

#ifdef ESP8266
  if ((!sending) && (cmdnrel > 0)) { //check if there is a send command in the buffer
    log_message(_F("Sending command from buffer"));
    popCommandBuffer();
  }
#endif

  if (heishamonSettings.use_1wire) dallasLoop(mqtt_client, log_message, heishamonSettings.mqtt_topic_base);

  if (dallasMqttRestorePending && ((unsigned long)(millis() - dallasMqttRestoreStart) > DALLAS_MQTT_RESTORE_TIMEOUT)) {
    sprintf_P(mqtt_topic, PSTR("%s/%s/+"), heishamonSettings.mqtt_topic_base, mqtt_topic_1wire);
    mqtt_client.unsubscribe(mqtt_topic);
    dallasMqttRestorePending = false;
    log_message(_F("Done restoring 1wire sensors from mqtt"));
  }

  if (heishamonSettings.use_s0) s0Loop(mqtt_client, log_message, heishamonSettings.mqtt_topic_base, heishamonSettings.s0Settings);

#ifdef ESP8266
//this only runs on ESP8266, the ESP32 does this in vTask
  if ((!sending) && (!heishamonSettings.listenonly) && (heishamonSettings.optionalPCB) && ((unsigned long)(millis() - lastOptionalPCBRunTime) > OPTIONALPCBQUERYTIME) ) {
    lastOptionalPCBRunTime = millis();
    send_optionalpcb_query();
    if ((unsigned long)(millis() - lastOptionalPCBSave) > (1000 * OPTIONALPCBSAVETIME)) {  // only save each 5 minutes
      lastOptionalPCBSave = millis();
      if (saveOptionalPCB(optionalPCBQuery, OPTIONALPCBQUERYSIZE)) {
        log_message((char*)"Succesfully saved optional PCB data to flash!");
      } else {
        log_message((char*)"Failed to save optional PCB data to flash!");
      }
    }
  }
#endif

  // run the data query only each WAITTIME
  if ((unsigned long)(millis() - lastRunTime) > (1000 * heishamonSettings.waitTime)) {
    lastRunTime = millis();
    //check mqtt
  #ifdef ESP8266
    if ( WiFi.isConnected() && (!mqtt_client.connected()) )
  #else
    if ( (WiFi.isConnected() || ETH.connected()) && (!mqtt_client.connected()) )
  #endif
    {
      if (mqttReconnects > 0 ) log_message(_F("Lost MQTT connection!"));
      if (strlen(heishamonSettings.mqtt_server) > 0) mqtt_reconnect();
    }


    //log stats
    if (totalreads > 0 ) readpercentage = (((float)goodreads / (float)totalreads) * 100);
    String message;
#ifdef ESP8266
    message.reserve(384);
#endif
    message += F("Heishamon stats: Uptime: ");
    char *up = getUptime();
    message += up;
    free(up);
    message += F(" ## Free memory: ");
    message += getFreeMemory();
#if defined(ESP8266)
    message += F("% ## Heap fragmentation: ");
    message += ESP.getHeapFragmentation();
    message += F("% ## Max free block: ");
    message += ESP.getMaxFreeBlockSize();
    message += F(" bytes ## Free heap: ");
#elif defined(ESP32)
    message += F("% ## Free PSRAM: ");
    message += ESP.getFreePsram();
    message += F(" bytes ## Free heap: ");
#endif
    message += ESP.getFreeHeap();
    message += F(" bytes ## Wifi: ");
    message += getWifiQuality();
    message += F("% (RSSI: ");
    message += WiFi.RSSI();
#ifdef ESP32
    message += F(") ## Ethernet: ");
    if (ETH.phyAddr() != 0) {        
      if (ETH.connected()) {
        if (ETH.hasIP()) {
          message += F("connected (");
          message += ETH.localIP().toString();
          message += F(")");
        } else {
          message += F("connected (no IP)");
        }
      } 
      else {
        message += F("not connected");
      }
    } else {
      message += F("not installed");
    }
    message += F(" ## Mqtt reconnects: ");
#else
    message += F(") ## Mqtt reconnects: ");
#endif
    message += mqttReconnects;
    message += F(" ## Correct data: ");
    message += readpercentage;
    message += F("% Rules active: ");
    message += nrrules;
    log_message((char*)message.c_str());

    String stats;
#ifdef ESP8266
    stats.reserve(384);
#endif
    stats += F("{\"uptime\":");
    stats += String(millis());
    stats += F(",\"voltage\":");
#if defined(ESP8266)
    stats += ESP.getVcc() / 1024.0;
#else
    stats += "3.3";
#endif
    stats += F(",\"free memory\":");
    stats += getFreeMemory();
    stats += F(",\"free heap\":");
    stats += ESP.getFreeHeap();
    stats += F(",\"wifi\":");
    stats += getWifiQuality();
    stats += F(",\"mqtt reconnects\":");
    stats += mqttReconnects;
    stats += F(",\"total reads\":");
    stats += totalreads;
    stats += F(",\"good reads\":");
    stats += goodreads;
    stats += F(",\"bad crc reads\":");
    stats += badcrcread;
    stats += F(",\"bad header reads\":");
    stats += badheaderread;
    stats += F(",\"too short reads\":");
    stats += tooshortread;
    stats += F(",\"too long reads\":");
    stats += toolongread;
    stats += F(",\"timeout reads\":");
    stats += timeoutread;
    stats += F(",\"version\":\"");
    stats += heishamon_version;
    stats += F("\",\"board\":\"");
#ifdef ESP8266
    stats += F("ESP8266");
#else
    stats += F("ESP32");
#endif
    stats += F("\",\"rules active\":");
    stats += nrrules;
    stats += F("}");
    sprintf_P(mqtt_topic, PSTR("%s/stats"), heishamonSettings.mqtt_topic_base);
    mqtt_client.publish(mqtt_topic, stats.c_str(), MQTT_RETAIN_VALUES);

    //websocket stats
#ifdef ESP32
    String ethernetStat;
    if (ETH.phyAddr() != 0) {        
      if (ETH.connected()) {
        if (ETH.hasIP()) {
          ethernetStat = F("connected - IP: ");
          ethernetStat += ETH.localIP().toString();
        } else {
          ethernetStat = F("connected - no IP");
        }
      } 
      else {
        ethernetStat = F("not connected");
      }
    } else {
      ethernetStat = F("not installed");
    }
    char *getuptime = getUptime();
    sprintf_P(log_msg, PSTR("{\"data\": {\"stats\": {\"wifi\": %d, \"ethernet\": \"%s\", \"memory\": %d, \"correct\": %.0f,\"mqtt\": %d,\"rules\": %d,\"uptime\": \"%s\"}}}"), getWifiQuality(), ethernetStat.c_str(), getFreeMemory(), readpercentage, mqttReconnects, nrrules, getuptime);
    free(getuptime);    
#else
    char *getuptime = getUptime();
    sprintf_P(log_msg, PSTR("{\"data\": {\"stats\": {\"wifi\": %d, \"memory\": %d, \"correct\": %.0f,\"mqtt\": %d,\"rules\": %d,\"uptime\": \"%s\"}}}"), getWifiQuality(), getFreeMemory(), readpercentage, mqttReconnects, nrrules, getuptime);    
    free(getuptime);    
#endif
    
    websocket_write_all(log_msg, strlen(log_msg));        

#ifdef ESP8266
    //get new data
    if (!heishamonSettings.listenonly) send_panasonic_query();
#endif

    //Make sure the LWT is set to Online, even if the broker have marked it dead.
    sprintf_P(mqtt_topic, PSTR("%s/%s"), heishamonSettings.mqtt_topic_base, mqtt_willtopic);
    mqtt_client.publish(mqtt_topic, "Online");

#ifdef ESP8266
    if (WiFi.isConnected()) {
      MDNS.announce();
    }
#endif
  }

  timerqueue_update();
  #ifdef ESP32
  delay(1); // to keep watchdog happy
  #endif
}
