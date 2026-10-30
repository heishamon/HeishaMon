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

void publishRelayStates(PubSubClient &mqtt, const char* base, unsigned int interval, bool force) {
#ifdef ESP32
  static int reported[2] = {-1, -1};
  static unsigned long lastPublish = 0;
  const unsigned long now = millis();
  const bool periodic = (unsigned long)(now - lastPublish) >= interval * 1000UL;
  const int pins[2] = {relayOnePin, relayTwoPin};
  const char* names[2] = {"one", "two"};
  if (!mqtt.connected()) return;
  bool success = true;
  for (int i = 0; i < 2; i++) {
    const int state = digitalRead(pins[i]) ? 1 : 0;
    if (force || periodic || reported[i] != state) {
      char topic[256];
      snprintf(topic, sizeof(topic), "%s/gpio/relay/%s/state", base, names[i]);
      if (mqtt.publish(topic, state ? "1" : "0", true)) {
        reported[i] = state;
      } else {
        reported[i] = -1; // Retry failed publications on the next loop.
        success = false;
      }
    }
  }
  if ((force || periodic) && success) lastPublish = now;
#endif
}

void mqttGPIOCallback(char* topic, char* value, PubSubClient &mqtt, const char* base) {
  char log_msg[256];
  sprintf_P(log_msg, PSTR("GPIO: MQTT message received on subtopic '%s' value '%s'"), topic, value);
  log_message(log_msg);
#ifdef ESP32
  if (strcmp_P(topic, PSTR("relay/one")) == 0) {
    log_message(_F("GPIO: MQTT message received 'relay/one'"));
    digitalWrite(relayOnePin,((stricmp((char*)"true", value) == 0) || (stricmp((char*)"on", value) == 0)  || (stricmp((char*)"enable", value) == 0)|| (String(value).toInt() == 1 )));
    publishRelayStates(mqtt, base, 0, true);
  } else if (strcmp_P(topic,PSTR("relay/two")) == 0) {
    log_message(_F("GPIO: MQTT message received 'relay/two'"));
    digitalWrite(relayTwoPin,((stricmp((char*)"true", value) == 0) || (stricmp((char*)"on", value) == 0)  || (stricmp((char*)"enable", value) == 0)|| (String(value).toInt() == 1 )));
    publishRelayStates(mqtt, base, 0, true);
  }
#endif
}
