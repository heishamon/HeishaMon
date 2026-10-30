#ifndef _DALLAS_H_
#define _DALLAS_H_

#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "src/common/webserver.h"

#define MAX_DALLAS_SENSORS 15
#define DALLAS_OFFSET_MAX 10.0 // max absolute calibration offset in degrees Celsius (set in steps of 0.1)
#if defined(ESP8266)
#define ONE_WIRE_BUS 4
#elif defined(ESP32)
#define ONE_WIRE_BUS 3
#endif

struct dallasDataStruct {
  float temperature = -127.0;
  unsigned long lastgoodtime = 0;
  bool known = false;   // slot is in use (sensor has been seen on the bus and/or restored from mqtt)
  bool present = false; // sensor was found on the 1-wire bus during the last scan
  DeviceAddress sensor;
  char address[17] = "";
  char alias[32] = "NOT SET";
  float offset = 0.0; // calibration offset (degrees Celsius) added to every raw reading of this sensor
};

void resetlastalldatatime_dallas();
void dallasLoop(PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base);
void initDallasSensors(void (*log_message)(char*), unsigned int updataAllDallasTimeSettings, unsigned int dallasTimerWaitSettings, unsigned int dallasResolution);
void rescanDallasSensors(void (*log_message)(char*), unsigned int dallasResolution);
void dallasJsonOutput(struct webserver_t *client);
void changeDallasAlias(char* address, char* alias);
bool changeDallasOffset(PubSubClient &mqtt_client, char* mqtt_topic_base, char* address, const char* value, void (*log_message)(char*));
void removeDallasSensor(PubSubClient &mqtt_client, char* mqtt_topic_base, char* address, void (*log_message)(char*));
void restoreDallasFromMqtt(char* address, float temperature, void (*log_message)(char*));

#endif
