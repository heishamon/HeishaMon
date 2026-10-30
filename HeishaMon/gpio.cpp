#include "gpio.h"
#include <PubSubClient.h>
#include "HeishaOT.h"
#include "src/common/progmem.h"
#include "src/common/stricmp.h"
#include "src/common/webserver.h"
const char* mqtt_topic_gpio PROGMEM = "gpio";

void log_message(char* string);

static bool toGPIOValue(char* value) {
  return (stricmp((char*)"true", value) == 0) || (stricmp((char*)"on", value) == 0) ||
         (stricmp((char*)"enable", value) == 0) || (String(value).toInt() == 1);
}

// Opentherm hardwires inOTPin/outOTPin for its own TX/RX (on ESP8266 these are pins 1 and 3,
// which otherwise double as extra GPIOs) - never let the extra-GPIO code touch them while it's enabled.
static bool isOpenthermPin(unsigned int pin, bool openthermEnabled) {
  return openthermEnabled && (pin == inOTPin || pin == outOTPin);
}

// On the small (ESP8266) board, listen-only mode skips digitalWrite(ENABLEPIN, HIGH) in switchSerial()
// so the shared level shifter/mosfets that the extra GPIO pins run through are never powered - using
// them would be pointless (or float) in that mode, so block them entirely rather than silently no-op.
bool extraGPIOBlockedByListenOnly(bool listenOnly) {
#if defined(ESP8266)
  return listenOnly;
#else
  return false;
#endif
}

// A pin is fully unavailable (no pinMode/digitalWrite, no digitalRead, no reporting anywhere) when
// either Opentherm has claimed it, or listen-only mode leaves its level shifter unpowered.
static bool isGpioUnavailable(unsigned int pin, int pinIndex, bool openthermEnabled, bool listenOnly) {
  if (isOpenthermPin(pin, openthermEnabled)) return true;
  if (pinIndex >= NUMGPIO_RELAY && extraGPIOBlockedByListenOnly(listenOnly)) return true;
  return false;
}

void setupGPIO(gpioSettingsStruct &gpioSettings, bool openthermEnabled, bool listenOnly) {
  // Apply user-configurable modes to the non-relay GPIO pins (relays occupy the first NUMGPIO_RELAY slots)
  for (int i = 0; i < NUMGPIO_USER; i++) {
    int pinIdx = NUMGPIO_RELAY + i;
    if (isGpioUnavailable(gpioSettings.gpioPin[pinIdx], pinIdx, openthermEnabled, listenOnly)) continue;
    switch (gpioSettings.gpioUserMode[i]) {
      case GPIO_MODE_INPUT:        gpioSettings.gpioMode[pinIdx] = INPUT;        break;
      case GPIO_MODE_OUTPUT:       gpioSettings.gpioMode[pinIdx] = OUTPUT;       break;
      default:                     gpioSettings.gpioMode[pinIdx] = INPUT_PULLUP; break;
    }
  }
  for (int i = 0; i < NUMGPIO; i++) {
    if (isGpioUnavailable(gpioSettings.gpioPin[i], i, openthermEnabled, listenOnly)) continue;
    pinMode(gpioSettings.gpioPin[i], gpioSettings.gpioMode[i]);
  }
}

void mqttGPIOCallback(char* topic, char* value, gpioSettingsStruct &gpioSettings, bool openthermEnabled, bool listenOnly) {
  char log_msg[256];
  sprintf_P(log_msg, PSTR("GPIO: MQTT message received on subtopic '%s' value '%s'"), topic, value);
  log_message(log_msg);
#ifdef ESP32
  if (strcmp_P(PSTR("relay/one"), topic) == 0) {
    log_message(_F("GPIO: MQTT message received 'relay/one'"));
    digitalWrite(relayOnePin, toGPIOValue(value));
    return;
  } else if (strcmp_P(PSTR("relay/two"), topic) == 0) {
    log_message(_F("GPIO: MQTT message received 'relay/two'"));
    digitalWrite(relayTwoPin, toGPIOValue(value));
    return;
  }
#endif
  // Handle user-configurable output GPIOs: topic "extra/N" (1-indexed)
  if (strncmp_P(topic, PSTR("extra/"), 6) == 0) {
    int idx = atoi(topic + 6) - 1;  // convert 1-indexed to 0-indexed
    if (idx >= 0 && idx < NUMGPIO_USER) {
      int pinIdx = NUMGPIO_RELAY + idx;
      // Only pins actually in OUTPUT mode are ever candidates for a real write. Pins left in
      // INPUT/INPUT_PULLUP (which includes every opentherm/listen-only-blocked pin, since setupGPIO()
      // never promotes them to OUTPUT) commonly receive this callback too - not as a write command,
      // but as the self-echo of publishGPIOStates() reporting their own read state back over MQTT.
      // Checking mode first (before the opentherm/listen-only reasoning) avoids logging a misleading
      // "ignoring write" message for what is actually just a harmless state echo, not a blocked switch.
      if (gpioSettings.gpioMode[pinIdx] == OUTPUT) {
        if (isOpenthermPin(gpioSettings.gpioPin[pinIdx], openthermEnabled)) {
          log_message(_F("GPIO: ignoring extra GPIO write, pin reserved by Opentherm"));
          return;
        }
        if (extraGPIOBlockedByListenOnly(listenOnly)) {
          log_message(_F("GPIO: ignoring extra GPIO write, listen-only mode leaves the level shifter unpowered"));
          return;
        }
        char log_msg[64];
        snprintf_P(log_msg, sizeof(log_msg), PSTR("GPIO: set extra/%d (pin %d) = %s"), idx + 1, gpioSettings.gpioPin[pinIdx], value);
        log_message(log_msg);
        digitalWrite(gpioSettings.gpioPin[pinIdx], toGPIOValue(value));
      }
    }
  }
}

void publishGPIOStates(PubSubClient &mqtt_client, gpioSettingsStruct &gpioSettings, char* mqtt_topic_base, bool publishAll, bool openthermEnabled, bool listenOnly) {
  static int lastState[NUMGPIO];
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < NUMGPIO; i++) lastState[i] = -1;
    initialized = true;
  }
  char topic[256];
  char wsmsg[64];
  bool mqttConnected = mqtt_client.connected();
  for (int i = 0; i < NUMGPIO; i++) {
    // Unavailable pins (Opentherm-claimed, or listen-only leaving the level shifter unpowered) are not
    // read at all - no digitalRead, no MQTT publish, no websocket push. They're not meaningful telemetry
    // (Opentherm's own bit-banging would just get echoed back over MQTT to mqttGPIOCallback) and the
    // frontend GPIO tab hides/removes them entirely, so there's nothing to report them to.
    if (isGpioUnavailable(gpioSettings.gpioPin[i], i, openthermEnabled, listenOnly)) continue;
    int state = digitalRead(gpioSettings.gpioPin[i]);
    if (publishAll || state != lastState[i]) {
      lastState[i] = state;
      if (i >= NUMGPIO_RELAY && mqttConnected) {
        snprintf_P(topic, sizeof(topic), PSTR("%s/gpio/extra/%d"), mqtt_topic_base, i - NUMGPIO_RELAY + 1);
        mqtt_client.publish(topic, state ? "1" : "0", true);
      }
      int len = snprintf_P(wsmsg, sizeof(wsmsg), PSTR("{\"data\": {\"gpio\": {\"pin\": %d, \"state\": %d}}}"),
                           gpioSettings.gpioPin[i], state);
      websocket_write_all(wsmsg, len);
    }
  }
}

void gpioJsonOutput(struct webserver_t *client, gpioSettingsStruct &gpioSettings, bool openthermEnabled, bool listenOnly) {
  char buf[64];
  webserver_send_content_P(client, PSTR("["), 1);
  bool first = true;
  for (int i = 0; i < NUMGPIO; i++) {
    if (isGpioUnavailable(gpioSettings.gpioPin[i], i, openthermEnabled, listenOnly)) continue;
    int state = digitalRead(gpioSettings.gpioPin[i]);
    int mode = (i < NUMGPIO_RELAY) ? GPIO_MODE_OUTPUT : gpioSettings.gpioUserMode[i - NUMGPIO_RELAY];
    int len = snprintf_P(buf, sizeof(buf), PSTR("%s{\"pin\":%d,\"mode\":%d,\"state\":%d}"),
                         first ? "" : ",", gpioSettings.gpioPin[i], mode, state);
    webserver_send_content(client, buf, len);
    first = false;
  }
  webserver_send_content_P(client, PSTR("]"), 1);
}

