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

// Every GPIO (the two ESP32 relays and the extra GPIOs) has a command topic and a state topic:
//   <base>/gpio/relay/one, relay/two, extra/N   commands, for output pins only
//   <base>/gpio/<same>/state                    actual pin level, retained, published by HeishaMon
// HeishaMon never acts on its own state topics (except the single restore after boot below), so
// a state publish can never echo back as a command. Rules write the pins directly with digitalWrite,
// so states are found by reading the pins every loop, which also covers rules, REST and Modbus.
#define GPIO_RESTORE_TIMEOUT 5000 // how long to wait for retained states after the first mqtt connect

static int reportedState[NUMGPIO];  // last level published to mqtt, -1 is not (successfully) published
static int wsState[NUMGPIO];        // last level sent to the web page
static int bootLevel[NUMGPIO];      // output level right after setupGPIO, to see if something changed it
static bool commanded[NUMGPIO];     // output was switched by a command (mqtt, REST, web, Modbus) since boot
static bool gpioStatesInitialized = false;
static bool restorePending = false;
static bool restoreDone = false;
static unsigned long restoreStart = 0;
static bool publishAllPending = false;
static unsigned long lastInputResend = 0;

static void initGPIOStates() {
  if (gpioStatesInitialized) return;
  for (int i = 0; i < NUMGPIO; i++) {
    reportedState[i] = -1;
    wsState[i] = -1;
    bootLevel[i] = -1;
    commanded[i] = false;
  }
  gpioStatesInitialized = true;
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
  initGPIOStates();
  for (int i = 0; i < NUMGPIO; i++) {
    if (isGpioUnavailable(gpioSettings.gpioPin[i], i, openthermEnabled, listenOnly)) continue;
    pinMode(gpioSettings.gpioPin[i], gpioSettings.gpioMode[i]);
    if (gpioSettings.gpioMode[i] == OUTPUT) bootLevel[i] = digitalRead(gpioSettings.gpioPin[i]) ? 1 : 0;
  }
}

// topic of a GPIO relative to <base>/gpio, e.g. "relay/one" or "extra/2"
static void gpioTopicName(int i, char* buf, size_t len) {
#ifdef ESP32
  if (i < NUMGPIO_RELAY) {
    snprintf_P(buf, len, i == 0 ? PSTR("relay/one") : PSTR("relay/two"));
    return;
  }
#endif
  snprintf_P(buf, len, PSTR("extra/%d"), i - NUMGPIO_RELAY + 1);
}

// Find the GPIO index for a topic relative to <base>/gpio. isState is set for the "/state" topics.
static int gpioIndexFromTopic(const char* topic, bool &isState) {
  char name[16];
  for (int i = 0; i < NUMGPIO; i++) {
    gpioTopicName(i, name, sizeof(name));
    size_t n = strlen(name);
    if (strncmp(topic, name, n) != 0) continue;
    if (topic[n] == '\0') {
      isState = false;
      return i;
    }
    if (strcmp_P(topic + n, PSTR("/state")) == 0) {
      isState = true;
      return i;
    }
  }
  return -1;
}

static void writeOutput(int i, bool state, gpioSettingsStruct &gpioSettings) {
  initGPIOStates();
  commanded[i] = true;
  digitalWrite(gpioSettings.gpioPin[i], state);
}

void gpioMqttConnected(bool firstConnect) {
  initGPIOStates();
  publishAllPending = true; // publish all states on every (re)connect, the broker may have lost them
  if (firstConnect && !restoreDone) {
    restorePending = true;  // hold the state publishes until the retained states are restored
    restoreStart = millis();
  }
}

void mqttGPIOCallback(char* topic, char* value, gpioSettingsStruct &gpioSettings, bool openthermEnabled, bool listenOnly) {
  initGPIOStates();
  bool isState = false;
  int i = gpioIndexFromTopic(topic, isState);
  if (i < 0) return;
  if (isGpioUnavailable(gpioSettings.gpioPin[i], i, openthermEnabled, listenOnly)) {
    if (!isState) log_message(_F("GPIO: ignoring GPIO write, pin is reserved by Opentherm or unpowered in listen-only mode"));
    return;
  }
  if (gpioSettings.gpioMode[i] != OUTPUT) return; // inputs can't be switched, and their state is not restored
  char log_msg[96];
  if (isState) {
    // Our own state topic. Only used once after boot to restore the outputs, ignored otherwise.
    if (!restorePending) return;
    if (commanded[i] || (digitalRead(gpioSettings.gpioPin[i]) != bootLevel[i])) {
      snprintf_P(log_msg, sizeof(log_msg), PSTR("GPIO: not restoring %s, it was already switched since boot"), topic);
      log_message(log_msg);
      return;
    }
    snprintf_P(log_msg, sizeof(log_msg), PSTR("GPIO: restoring %s = %s"), topic, value);
    log_message(log_msg);
    digitalWrite(gpioSettings.gpioPin[i], toGPIOValue(value));
    return;
  }
  snprintf_P(log_msg, sizeof(log_msg), PSTR("GPIO: set %s (pin %d) = %s"), topic, gpioSettings.gpioPin[i], value);
  log_message(log_msg);
  writeOutput(i, toGPIOValue(value), gpioSettings);
}

void publishGPIOStates(PubSubClient &mqtt_client, gpioSettingsStruct &gpioSettings, char* mqtt_topic_base, bool openthermEnabled, bool listenOnly, unsigned int updateAllTime) {
  initGPIOStates();
  if (restorePending && ((unsigned long)(millis() - restoreStart) > GPIO_RESTORE_TIMEOUT)) {
    restorePending = false;
    restoreDone = true;
    log_message(_F("GPIO: done restoring output states from mqtt"));
  }
  bool mqttConnected = mqtt_client.connected() && !restorePending;
  bool publishAll = mqttConnected && publishAllPending;
  bool resendInputs = mqttConnected && (updateAllTime > 0) && ((unsigned long)(millis() - lastInputResend) > (1000UL * updateAllTime));
  bool success = true;
  char name[16];
  char topic[256];
  char wsmsg[64];
  for (int i = 0; i < NUMGPIO; i++) {
    // Unavailable pins (Opentherm-claimed, or listen-only leaving the level shifter unpowered) are not
    // read at all - no digitalRead, no MQTT publish, no websocket push.
    if (isGpioUnavailable(gpioSettings.gpioPin[i], i, openthermEnabled, listenOnly)) continue;
    int state = digitalRead(gpioSettings.gpioPin[i]) ? 1 : 0;
    if (state != wsState[i]) {
      wsState[i] = state;
      int len = snprintf_P(wsmsg, sizeof(wsmsg), PSTR("{\"data\": {\"gpio\": {\"pin\": %d, \"state\": %d}}}"),
                           gpioSettings.gpioPin[i], state);
      websocket_write_all(wsmsg, len);
    }
    if (!mqttConnected) continue;
    bool isInput = gpioSettings.gpioMode[i] != OUTPUT;
    if (publishAll || (state != reportedState[i]) || (isInput && resendInputs)) {
      gpioTopicName(i, name, sizeof(name));
      snprintf_P(topic, sizeof(topic), PSTR("%s/gpio/%s/state"), mqtt_topic_base, name);
      if (mqtt_client.publish(topic, state ? "1" : "0", true)) {
        reportedState[i] = state;
      } else {
        reportedState[i] = -1; // retry on the next loop
        success = false;
      }
    }
  }
  if (publishAll && success) publishAllPending = false;
  if (resendInputs && success) lastInputResend = millis();
}

// one pin object, used for the full list and for /json?gpio=<name>
void gpioJsonPin(struct webserver_t *client, int i, gpioSettingsStruct &gpioSettings) {
  char buf[64];
  int state = digitalRead(gpioSettings.gpioPin[i]);
  int mode = (i < NUMGPIO_RELAY) ? GPIO_MODE_OUTPUT : gpioSettings.gpioUserMode[i - NUMGPIO_RELAY];
  int len = snprintf_P(buf, sizeof(buf), PSTR("{\"pin\":%d,\"mode\":%d,\"state\":%d}"), gpioSettings.gpioPin[i], mode, state);
  webserver_send_content(client, buf, len);
}

void gpioJsonOutput(struct webserver_t *client, gpioSettingsStruct &gpioSettings, bool openthermEnabled, bool listenOnly) {
  webserver_send_content_P(client, PSTR("["), 1);
  bool first = true;
  for (int i = 0; i < NUMGPIO; i++) {
    if (isGpioUnavailable(gpioSettings.gpioPin[i], i, openthermEnabled, listenOnly)) continue;
    if (!first) webserver_send_content_P(client, PSTR(","), 1);
    first = false;
    gpioJsonPin(client, i, gpioSettings);
  }
  webserver_send_content_P(client, PSTR("]"), 1);
}

// find an available pin by its MQTT name ("extra/N", "relay/one", "relay/two") or by the
// extra GPIO number alone ("N"), -1 if none
int gpioJsonFind(const char *name, gpioSettingsStruct &gpioSettings, bool openthermEnabled, bool listenOnly) {
  int i = -1;
  if (name[0] >= '1' && name[0] <= '9' && name[1] == '\0') {
    i = NUMGPIO_RELAY + (name[0] - '1');
    if (i >= NUMGPIO) i = -1;
  } else {
    bool isState = false;
    i = gpioIndexFromTopic(name, isState);
    if (isState) i = -1;
  }
  if (i < 0 || isGpioUnavailable(gpioSettings.gpioPin[i], i, openthermEnabled, listenOnly)) return -1;
  return i;
}

#ifdef ESP32
void setRelay1(bool state)
{
  initGPIOStates();
  commanded[0] = true;
  digitalWrite(relayOnePin, state);
}

void setRelay2(bool state)
{
  initGPIOStates();
  commanded[1] = true;
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
