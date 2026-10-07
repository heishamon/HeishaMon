#include "gpio.h"
#include "src/common/progmem.h"
#include "src/common/stricmp.h"
const char* mqtt_topic_gpio PROGMEM = "gpio";

void log_message(char* string);

void setupGPIO(gpioSettingsStruct gpioSettings) {
  for (int i = 0 ;  i < NUMGPIO ; i++) {
    pinMode(gpioSettings.gpioPin[i], gpioSettings.gpioMode[i]);
  }
}

void mqttGPIOCallback(char* topic, char* value) {
  char log_msg[256];
  sprintf_P(log_msg, PSTR("GPIO: MQTT message received on subtopic '%s' value '%s'"), topic, value);
  log_message(log_msg);
#ifdef ESP32
  if (strcmp_P(topic, PSTR("relay/one")) == 0) {
    log_message(_F("GPIO: MQTT message received 'relay/one'"));
    setRelay1((stricmp((char*)"true", value) == 0) || (stricmp((char*)"on", value) == 0)  || (stricmp((char*)"enable", value) == 0)|| (String(value).toInt() == 1 ));
  } else if (strcmp_P(topic, PSTR("relay/two")) == 0) {
    log_message(_F("GPIO: MQTT message received 'relay/two'"));
    setRelay2((stricmp((char*)"true", value) == 0) || (stricmp((char*)"on", value) == 0)  || (stricmp((char*)"enable", value) == 0)|| (String(value).toInt() == 1 ));
  }
#endif
}

#ifdef ESP32
void setRelay1(bool state)
{
  digitalWrite(relayOnePin, state);
}

void setRelay2(bool state)
{
  digitalWrite(relayTwoPin, state);
}

bool getRelay1()
{
  return digitalRead(relayOnePin);
}

bool getRelay2()
{
  return digitalRead(relayTwoPin);
}

#endif
