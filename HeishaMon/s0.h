#include <PubSubClient.h>
#include "src/common/webserver.h"

#define NUM_S0_COUNTERS 2
#if defined(ESP8266)
#define DEFAULT_S0_PIN_1 12 
#define DEFAULT_S0_PIN_2 14 
#elif defined(ESP32)
#define DEFAULT_S0_PIN_1 1
#define DEFAULT_S0_PIN_2 2 
#endif

struct s0SettingsStruct {
  byte gpiopin = 255;
  unsigned int ppkwh = 1000; //pulses per Wh of the connected meter
  unsigned int lowerPowerInterval = 60; //configurabel low power interval
  unsigned int minimalPulseWidth = 25; //configurabel minimal s0 pulse width
  unsigned int maximalPulseWidth = 100; //configurabel maximal s0 pulse width
};

struct s0DataStruct {
  unsigned int pulses = 0; //number of pulses since last report
  unsigned int pulsesTotal = 0; //total pulses measured from begin
  unsigned int watt = 0; //calculated average power
  unsigned long lastPulse = 0; //last pulse in millis
  unsigned long nextReport = 0; //next time we reported the s0 value in millis
  unsigned long goodPulses = 0;
  unsigned long badPulses = 0;
  unsigned int avgPulseWidth = 0;
};

void initS0Sensors(s0SettingsStruct s0Settings[]);
bool s0_restore_pending(int s0Port);
void s0_restore_subscribed(int s0Port);
void restore_s0_Watthour(int s0Port, double watthour);
void s0Loop(PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base, s0SettingsStruct s0Settings[]);
void s0JsonOutput(struct webserver_t *client);
