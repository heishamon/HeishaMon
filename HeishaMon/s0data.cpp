#include "s0data.h"

extern volatile s0DataStruct actS0Data[NUM_S0_COUNTERS];
extern volatile s0SettingsStruct actS0Settings[NUM_S0_COUNTERS];

void readS0Readings(bool enabled, S0Reading readings[NUM_S0_COUNTERS]) {
  for (unsigned i = 0; i < NUM_S0_COUNTERS; ++i) {
    readings[i] = S0Reading{};
    const unsigned pulsesPerKwh = actS0Settings[i].ppkwh;
    if (!enabled || actS0Settings[i].gpiopin == 255 || pulsesPerKwh == 0) continue;

    // Read each interrupt-updated scalar once. Do not reset any pulse counters.
    const unsigned total = actS0Data[i].pulsesTotal;
    const unsigned long good = actS0Data[i].goodPulses;
    const unsigned long bad = actS0Data[i].badPulses;
    readings[i].watt = actS0Data[i].watt;
    readings[i].watthourTotal = total * (1000.0 / pulsesPerKwh);
    readings[i].watthour = actS0Data[i].lastReportWatthour;
    // Same startup convention as the S0 UI: no pulses yet means 100% quality.
    readings[i].pulseQuality = 100.0 * (double(good) + 1) / (double(good) + bad + 1);
    readings[i].avgPulseWidth = actS0Data[i].avgPulseWidth;
    readings[i].enabled = true;
  }
}
