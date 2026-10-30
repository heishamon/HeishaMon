#pragma once
#include <Arduino.h>

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
  unsigned int ppkwh = 1000; //pulses per kWh of the connected meter
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
  float lastReportWatthour = 0; //energy in the most recent S0 report interval
  unsigned int avgPulseWidth = 0;
};

// A non-destructive view for consumers such as Modbus. Values are sampled once
// per request so both words of a float are generated from the same reading.
struct S0Reading {
  float watt = 0;
  float watthourTotal = 0;
  float watthour = 0;
  float pulseQuality = 0;
  float avgPulseWidth = 0;
  bool enabled = false;
};

void readS0Readings(bool enabled, S0Reading readings[NUM_S0_COUNTERS]);
