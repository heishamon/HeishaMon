#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include "commands.h"
#include "dallas.h"
#include "rules.h"
#include "src/common/progmem.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

#define MQTT_RETAIN_VALUES 1 // do we retain 1wire values?

#define MAXTEMPDIFFPERSEC 0.5 // what is the allowed temp difference per second which is allowed (to filter bad values)

#define DALLASASYNC 1 //async dallas yes or no (default yes)

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

//global array for 1wire data - fixed size so known sensors survive across rescans
dallasDataStruct* actDallasData = 0;
int dallasDevicecount = 0; // kept as the array length (MAX_DALLAS_SENSORS) for external callers; iterate using the "known" flag


unsigned long lastalldatatime_dallas = 0;

unsigned long dallasTimer = 0;
unsigned long dallasTimer1 = 0;
unsigned int updateAllDallasTime = 30000; // will be set using heishmonSettings
unsigned int dallasTimerWait = 30000; // will be set using heishmonSettings
void loadDallasAlias();
void saveDallasAliasFile();
void loadDallasOffset();
void saveDallasOffsetFile();

static int findDallasSlot(const char* address) {
  for (int i = 0; i < MAX_DALLAS_SENSORS; i++) {
    if (actDallasData[i].known && strcmp(actDallasData[i].address, address) == 0) return i;
  }
  return -1;
}

static int findFreeDallasSlot() {
  for (int i = 0; i < MAX_DALLAS_SENSORS; i++) {
    if (!actDallasData[i].known) return i;
  }
  return -1;
}

// parses a 16 hex char address string (as produced/stored in .address) back into a DeviceAddress,
// so sensors restored purely from mqtt can still be addressed directly on the bus without a rescan
static void parseDallasAddress(const char* addrStr, DeviceAddress out) {
  for (int x = 0; x < 8; x++) {
    char byteStr[3] = { addrStr[x * 2], addrStr[x * 2 + 1], '\0' };
    out[x] = (uint8_t) strtoul(byteStr, NULL, 16);
  }
}

void rescanDallasSensors(void (*log_message)(char*), unsigned int dallasResolution) {
  char log_msg[256];
  DS18B20.begin(); // re-search the bus so newly attached sensors are actually detected
  int found = DS18B20.getDeviceCount();
  sprintf_P(log_msg, PSTR("Number of 1wire sensors on bus: %d"), found); log_message(log_msg);

  //assume known sensors are gone until the scan below re-affirms them
  for (int i = 0; i < MAX_DALLAS_SENSORS; i++) {
    if (actDallasData[i].known) actDallasData[i].present = false;
  }

  for (int j = 0; j < found; j++) {
    DeviceAddress addr;
    if (!DS18B20.getAddress(addr, j)) continue;
    char addrStr[17];
    addrStr[16] = '\0';
    for (int x = 0; x < 8; x++) {
      sprintf(&addrStr[x * 2], "%02x", addr[x]);
    }

    int slot = findDallasSlot(addrStr);
    if (slot < 0) {
      slot = findFreeDallasSlot();
      if (slot < 0) {
        sprintf_P(log_msg, PSTR("Reached max 1wire sensor count (%d). Ignoring sensor: %s"), MAX_DALLAS_SENSORS, addrStr);
        log_message(log_msg);
        continue;
      }
      actDallasData[slot].known = true;
      strlcpy(actDallasData[slot].address, addrStr, sizeof(actDallasData[slot].address));
      actDallasData[slot].temperature = -127.0;
      actDallasData[slot].lastgoodtime = 0;
      strlcpy(actDallasData[slot].alias, "NOT SET", sizeof(actDallasData[slot].alias));
      actDallasData[slot].offset = 0.0;
      sprintf_P(log_msg, PSTR("Found new 1wire sensor: %s"), addrStr); log_message(log_msg);
      sprintf_P(log_msg, PSTR("{\"data\": {\"dallasRescan\": true}}")); websocket_write_all(log_msg, strlen(log_msg)); // tell open browser tabs to reload the sensor table
    }
    memcpy(actDallasData[slot].sensor, addr, sizeof(DeviceAddress));
    actDallasData[slot].present = true;
    DS18B20.setResolution(addr, dallasResolution);
  }

  for (int i = 0; i < MAX_DALLAS_SENSORS; i++) {
    if (actDallasData[i].known && !actDallasData[i].present) {
      sprintf_P(log_msg, PSTR("Known 1wire sensor not responding: %s"), actDallasData[i].address); log_message(log_msg);
    }
  }

  if (DALLASASYNC) DS18B20.setWaitForConversion(false); //async 1wire during next loops
  loadDallasAlias();
  loadDallasOffset();
}

void initDallasSensors(void (*log_message)(char*), unsigned int updateAllDallasTimeSettings, unsigned int dallasTimerWaitSettings, unsigned int dallasResolution) {
  updateAllDallasTime = updateAllDallasTimeSettings;
  dallasTimerWait = dallasTimerWaitSettings;

  if (!actDallasData) {
    actDallasData = new dallasDataStruct [MAX_DALLAS_SENSORS];
    dallasDevicecount = MAX_DALLAS_SENSORS;
  }

  rescanDallasSensors(log_message, dallasResolution);
}

void resetlastalldatatime_dallas() {
  lastalldatatime_dallas = 0;
}

void readNewDallasTemp(PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base) {
  char log_msg[256];
  char mqtt_topic[256];
  char valueStr[80];
  bool updatenow = false;

  if ((lastalldatatime_dallas == 0) || ((unsigned long)(millis() - lastalldatatime_dallas) >  (1000 * updateAllDallasTime))) {
    updatenow = true;
    lastalldatatime_dallas = millis();
  }
  if (!(DALLASASYNC)) DS18B20.requestTemperatures();
  for (int i = 0; i < dallasDevicecount; i++) {
    if (!actDallasData[i].known) continue; // query every known sensor, even ones currently marked offline, so they can be marked online again once they respond
    float temp = DS18B20.getTempC(actDallasData[i].sensor);
    bool wasPresent = actDallasData[i].present;
    if (temp < -120.0) {
      actDallasData[i].present = false;
      if (wasPresent) {
        sprintf_P(log_msg, PSTR("1wire sensor went offline: %s"), actDallasData[i].address); log_message(log_msg);
        sprintf_P(log_msg, PSTR("{\"data\": {\"dallasvalues\": {\"sensorID\": \"%s\", \"present\": false}}}"), actDallasData[i].address);
        websocket_write_all(log_msg, strlen(log_msg));
      }
    } else {
      actDallasData[i].present = true;
      if (!wasPresent) {
        sprintf_P(log_msg, PSTR("1wire sensor back online: %s"), actDallasData[i].address); log_message(log_msg);
        sprintf_P(log_msg, PSTR("{\"data\": {\"dallasvalues\": {\"sensorID\": \"%s\", \"present\": true}}}"), actDallasData[i].address);
        websocket_write_all(log_msg, strlen(log_msg));
      }
      float allowedtempdiff = (((millis() - actDallasData[i].lastgoodtime)) / 1000.0) * MAXTEMPDIFFPERSEC;
      bool powerOnReset = (fabs(temp - 85.0) < 0.0001); // check the raw value, before the calibration offset is applied
      temp += actDallasData[i].offset; // apply the per-sensor calibration offset; everything below (filter, mqtt, json, rules) uses the corrected value
      if (powerOnReset) { // 85.0C is the DS18B20 power-on reset default, not a real reading; sensor is online, just not converted yet
        sprintf_P(log_msg, PSTR("Ignoring 1wire sensor power-on-reset value (85.00): %s"), actDallasData[i].address);
        log_message(log_msg);
      } else if ((actDallasData[i].temperature != -127.0) and ((temp > (actDallasData[i].temperature + allowedtempdiff)) or (temp < (actDallasData[i].temperature - allowedtempdiff)))) {
        sprintf_P(log_msg, PSTR("Filtering 1wire sensor temperature (%s). Delta to high. Current: %.2f Last: %.2f"), actDallasData[i].address, temp, actDallasData[i].temperature);
        log_message(log_msg);
      } else {
        actDallasData[i].lastgoodtime = millis();
        if ((updatenow) || (actDallasData[i].temperature != temp )) {  //only update mqtt topic if temp changed or after each update timer
          actDallasData[i].temperature = temp;
          sprintf(log_msg, PSTR("Received 1wire sensor temperature (%s): %.2f"), actDallasData[i].address, actDallasData[i].temperature);
          log_message(log_msg);
          if (true) {
            sprintf_P(valueStr, PSTR("%.2f"), actDallasData[i].temperature);
            sprintf_P(mqtt_topic, PSTR("%s/%s/%s"), mqtt_topic_base, mqtt_topic_1wire, actDallasData[i].address); mqtt_client.publish(mqtt_topic, valueStr, MQTT_RETAIN_VALUES);
            sprintf_P(valueStr, PSTR("%s"), actDallasData[i].alias);
            sprintf_P(mqtt_topic, PSTR("%s/%s/%s/alias"), mqtt_topic_base, mqtt_topic_1wire, actDallasData[i].address); mqtt_client.publish(mqtt_topic, valueStr, MQTT_RETAIN_VALUES);
            sprintf_P(valueStr, PSTR("%.1f"), actDallasData[i].offset);
            sprintf_P(mqtt_topic, PSTR("%s/%s/%s/offset"), mqtt_topic_base, mqtt_topic_1wire, actDallasData[i].address); mqtt_client.publish(mqtt_topic, valueStr, MQTT_RETAIN_VALUES);
          } else {
            sprintf_P(valueStr, PSTR("{\"Temperature\":%.2f,\"Alias\":\"%s\"}"), actDallasData[i].temperature, actDallasData[i].alias);
            sprintf_P(mqtt_topic, PSTR("%s/%s/%s"), mqtt_topic_base, mqtt_topic_1wire, actDallasData[i].address); mqtt_client.publish(mqtt_topic, valueStr, MQTT_RETAIN_VALUES);
          }
          sprintf_P(log_msg, PSTR("{\"data\": {\"dallasvalues\": {\"sensorID\": \"%s\", \"value\": %.2f}}}"), actDallasData[i].address, actDallasData[i].temperature);
          websocket_write_all(log_msg, strlen(log_msg));          
          rules_event_cb(_F("ds18b20#"), actDallasData[i].address);
        }
      }
    }
  }
}

void dallasLoop(PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base) {
  if ((unsigned long)(millis() - dallasTimer) > (1000 * dallasTimerWait)) {
    log_message((char*)"Requesting new 1wire temperatures");
    dallasTimer = millis();
    if (DALLASASYNC){
      DS18B20.requestTemperatures();
      dallasTimer1=millis();
    }else{
      readNewDallasTemp(mqtt_client, log_message, mqtt_topic_base);
    }
  }
  if ((dallasTimer1!=0) && ((millis() - dallasTimer1)>750)){
    dallasTimer1=0;
    readNewDallasTemp(mqtt_client, log_message, mqtt_topic_base);
  }   
}

void dallasJsonOutput(struct webserver_t *client) {
  webserver_send_content_P(client, PSTR("["), 1);

  bool first = true;
  for (int i = 0; i < dallasDevicecount; i++) {
    if (!actDallasData[i].known) continue;
    if (!first) webserver_send_content_P(client, PSTR(","), 1);
    first = false;
    webserver_send_content_P(client, PSTR("{\"Sensor\":\""), 11);
    webserver_send_content(client, actDallasData[i].address, strlen(actDallasData[i].address));
    webserver_send_content_P(client, PSTR("\",\"Temperature\":"), 16);
    char str[64];
    dtostrf(actDallasData[i].temperature, 0, 2, str);
    webserver_send_content(client, str, strlen(str));
    webserver_send_content_P(client, PSTR(",\"Alias\":\""), 10);
    webserver_send_content(client, actDallasData[i].alias, strlen(actDallasData[i].alias));
    webserver_send_content_P(client, PSTR("\",\"Offset\":"), 11);
    dtostrf(actDallasData[i].offset, 0, 1, str);
    webserver_send_content(client, str, strlen(str));
    webserver_send_content_P(client, PSTR(",\"Present\":"), 11);
    if (actDallasData[i].present) {
      webserver_send_content_P(client, PSTR("true"), 4);
    } else {
      webserver_send_content_P(client, PSTR("false"), 5);
    }
    webserver_send_content_P(client, PSTR(",\"LastSeenSeconds\":"), 19);
    long lastSeenSeconds = (actDallasData[i].lastgoodtime == 0) ? -1 : (long)((millis() - actDallasData[i].lastgoodtime) / 1000);
    sprintf(str, "%ld", lastSeenSeconds);
    webserver_send_content(client, str, strlen(str));
    webserver_send_content_P(client, PSTR("}"), 1);
  }
  webserver_send_content_P(client, PSTR("]"), 1);
}

void saveDallasAliasFile() {
  JsonDocument jsonDoc;
  for (int i = 0 ; i < dallasDevicecount; i++) {
    if (!actDallasData[i].known) continue;
    jsonDoc[actDallasData[i].address] = actDallasData[i].alias;
  }
  if (LittleFS.begin()) {
    File configFile = LittleFS.open("/dallas.json", "w");
    if (configFile) {
      serializeJson(jsonDoc, configFile);
      configFile.close();
    }
  }
}

void changeDallasAlias(char* address, char* alias) {
  int slot = findDallasSlot(address);
  if (slot < 0) return;
  strlcpy(actDallasData[slot].alias, alias, sizeof(actDallasData[slot].alias));
  saveDallasAliasFile();
}

void removeDallasSensor(PubSubClient &mqtt_client, char* mqtt_topic_base, char* address, void (*log_message)(char*)) {
  char log_msg[256];
  int slot = findDallasSlot(address);
  if (slot < 0) return;

  char mqtt_topic[256];
  // publishing an empty retained payload clears the previously retained message on the broker
  sprintf_P(mqtt_topic, PSTR("%s/%s/%s"), mqtt_topic_base, mqtt_topic_1wire, address); mqtt_client.publish(mqtt_topic, "", true);
  sprintf_P(mqtt_topic, PSTR("%s/%s/%s/alias"), mqtt_topic_base, mqtt_topic_1wire, address); mqtt_client.publish(mqtt_topic, "", true);
  sprintf_P(mqtt_topic, PSTR("%s/%s/%s/offset"), mqtt_topic_base, mqtt_topic_1wire, address); mqtt_client.publish(mqtt_topic, "", true);

  sprintf_P(log_msg, PSTR("Removed 1wire sensor: %s"), address); log_message(log_msg);

  actDallasData[slot] = dallasDataStruct();
  saveDallasAliasFile();
  saveDallasOffsetFile();
  sprintf_P(log_msg, PSTR("{\"data\": {\"dallasRescan\": true}}")); websocket_write_all(log_msg, strlen(log_msg)); // tell open browser tabs to reload the sensor table
}

void restoreDallasFromMqtt(char* address, float temperature, void (*log_message)(char*)) {
  char log_msg[256];
  int slot = findDallasSlot(address);
  if (slot < 0) {
    slot = findFreeDallasSlot();
    if (slot < 0) return; //no room left, ignore
    actDallasData[slot].known = true;
    actDallasData[slot].present = false;
    strlcpy(actDallasData[slot].address, address, sizeof(actDallasData[slot].address));
    if (strlen(address) == 16) parseDallasAddress(address, actDallasData[slot].sensor); // allows this sensor to be polled directly without waiting for a bus rescan
    strlcpy(actDallasData[slot].alias, "NOT SET", sizeof(actDallasData[slot].alias));
    sprintf_P(log_msg, PSTR("Restored previously known 1wire sensor from mqtt: %s"), address); log_message(log_msg);
    loadDallasAlias();
    loadDallasOffset();
    sprintf_P(log_msg, PSTR("{\"data\": {\"dallasRescan\": true}}")); websocket_write_all(log_msg, strlen(log_msg)); // tell open browser tabs to reload the sensor table
  }
  if (actDallasData[slot].lastgoodtime == 0) { //only backfill if we haven't taken a real reading yet this boot
    actDallasData[slot].temperature = temperature;
  }
}

void loadDallasAlias() {
  if (LittleFS.begin()) {
    if (LittleFS.exists("/dallas.json")) {
      File configFile = LittleFS.open("/dallas.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        JsonDocument jsonDoc;
        DeserializationError error = deserializeJson(jsonDoc, buf.get());
        if (!error) {
          for (int i = 0 ; i < dallasDevicecount; i++) {
            if (!actDallasData[i].known) continue;
            if ( jsonDoc[actDallasData[i].address] ) strncpy(actDallasData[i].alias, jsonDoc[actDallasData[i].address], sizeof(actDallasData[i].alias));
          }
        }
      }
    }
  }
}

// Calibration offsets are stored in their own file instead of being added to dallas.json,
// so a downgrade to an older firmware (which expects plain alias strings in dallas.json) keeps working.
void saveDallasOffsetFile() {
  JsonDocument jsonDoc;
  for (int i = 0 ; i < dallasDevicecount; i++) {
    if (!actDallasData[i].known) continue;
    if (actDallasData[i].offset == 0.0) continue; // only store sensors that actually have an offset
    jsonDoc[actDallasData[i].address] = actDallasData[i].offset;
  }
  if (LittleFS.begin()) {
    File configFile = LittleFS.open("/dallasoffset.json", "w");
    if (configFile) {
      serializeJson(jsonDoc, configFile);
      configFile.close();
    }
  }
}

// rounds to 0.1 degree steps and clamps to +/- DALLAS_OFFSET_MAX
static float sanitizeDallasOffset(float offset) {
  offset = roundf(offset * 10.0) / 10.0;
  if (offset > DALLAS_OFFSET_MAX) offset = DALLAS_OFFSET_MAX;
  if (offset < -DALLAS_OFFSET_MAX) offset = -DALLAS_OFFSET_MAX;
  if (offset == 0.0) offset = 0.0; // normalize -0.0
  return offset;
}

void loadDallasOffset() {
  if (LittleFS.begin()) {
    if (LittleFS.exists("/dallasoffset.json")) {
      File configFile = LittleFS.open("/dallasoffset.json", "r");
      if (configFile) {
        JsonDocument jsonDoc;
        DeserializationError error = deserializeJson(jsonDoc, configFile);
        configFile.close();
        if (!error) {
          for (int i = 0 ; i < dallasDevicecount; i++) {
            if (!actDallasData[i].known) continue;
            if (jsonDoc[actDallasData[i].address].is<float>()) {
              actDallasData[i].offset = sanitizeDallasOffset(jsonDoc[actDallasData[i].address].as<float>());
            }
          }
        }
      }
    }
  }
}

bool changeDallasOffset(PubSubClient &mqtt_client, char* mqtt_topic_base, char* address, const char* value, void (*log_message)(char*)) {
  char log_msg[256];
  char mqtt_topic[256];
  char valueStr[16];

  int slot = findDallasSlot(address);
  if (slot < 0) {
    sprintf_P(log_msg, PSTR("Cannot set offset, unknown 1wire sensor: %s"), address); log_message(log_msg);
    return false;
  }
  // from here on use the stored address: the caller's buffer may be reused by an mqtt publish

  // accept both "0.3" and "0,3"
  char buf[16];
  strlcpy(buf, value, sizeof(buf));
  for (char* p = buf; *p; p++) {
    if (*p == ',') *p = '.';
  }
  char* end = NULL;
  float newOffset = strtof(buf, &end);
  while (end && (*end == ' ')) end++;
  if ((end == buf) || (end && *end != '\0') || isnan(newOffset) || isinf(newOffset)) {
    sprintf_P(log_msg, PSTR("Invalid 1wire offset value for %s: %s"), actDallasData[slot].address, value); log_message(log_msg);
    return false;
  }
  newOffset = sanitizeDallasOffset(newOffset);

  float diff = newOffset - actDallasData[slot].offset;
  actDallasData[slot].offset = newOffset;
  saveDallasOffsetFile();
  sprintf_P(log_msg, PSTR("1wire sensor %s offset set to %.1f"), actDallasData[slot].address, newOffset); log_message(log_msg);

  sprintf_P(valueStr, PSTR("%.1f"), newOffset);
  sprintf_P(mqtt_topic, PSTR("%s/%s/%s/offset"), mqtt_topic_base, mqtt_topic_1wire, actDallasData[slot].address); mqtt_client.publish(mqtt_topic, valueStr, MQTT_RETAIN_VALUES);

  if (actDallasData[slot].temperature != -127.0) {
    // shift the last known value right away, so mqtt, web and rules don't have to wait for the next reading
    actDallasData[slot].temperature += diff;
    sprintf_P(valueStr, PSTR("%.2f"), actDallasData[slot].temperature);
    sprintf_P(mqtt_topic, PSTR("%s/%s/%s"), mqtt_topic_base, mqtt_topic_1wire, actDallasData[slot].address); mqtt_client.publish(mqtt_topic, valueStr, MQTT_RETAIN_VALUES);
    sprintf_P(log_msg, PSTR("{\"data\": {\"dallasvalues\": {\"sensorID\": \"%s\", \"value\": %.2f, \"offset\": %.1f}}}"), actDallasData[slot].address, actDallasData[slot].temperature, newOffset);
    websocket_write_all(log_msg, strlen(log_msg));
    rules_event_cb(_F("ds18b20#"), actDallasData[slot].address);
  } else {
    sprintf_P(log_msg, PSTR("{\"data\": {\"dallasvalues\": {\"sensorID\": \"%s\", \"offset\": %.1f}}}"), actDallasData[slot].address, newOffset);
    websocket_write_all(log_msg, strlen(log_msg));
  }
  return true;
}
