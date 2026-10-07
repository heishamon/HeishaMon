#if defined(ESP8266)
#define NUMGPIO 3
#include <ESP8266WiFi.h>
#include <ESP8266WiFiGratuitous.h>
#elif defined(ESP32)
#define NUMGPIO 7
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#define relayOnePin 21
#define relayTwoPin 47
#endif

extern const char* mqtt_topic_gpio;

struct gpioSettingsStruct {
#if defined(ESP8266)
  unsigned int gpioPin[NUMGPIO] = {1, 3, 16};
  unsigned int gpioMode[NUMGPIO] = {INPUT_PULLUP, INPUT_PULLUP, INPUT_PULLUP};
#elif defined(ESP32)
  unsigned int gpioPin[NUMGPIO] = {33, 34, 35, 36, 37, 21, 47};
  unsigned int gpioMode[NUMGPIO] = {INPUT_PULLUP, INPUT_PULLUP, INPUT_PULLUP, INPUT_PULLUP, INPUT_PULLUP, OUTPUT, OUTPUT};
#endif
};

void setupGPIO(gpioSettingsStruct gpioSettings);
void mqttGPIOCallback(char* topic, char* value);
void setRelay1(bool state);
void setRelay2(bool state);
bool getRelay1();
bool getRelay2();

