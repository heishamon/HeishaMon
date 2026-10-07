#include <PubSubClient.h>
#include "commands.h"
#include "s0.h"
#include "rules.h"

#define MQTT_RETAIN_VALUES 1 // do we retain 1wire values?

#define MINREPORTEDS0TIME 5000 // how often s0 Watts are reported (not faster than this)

//global array for s0 data
volatile s0DataStruct actS0Data[NUM_S0_COUNTERS];

//global array for s0 Settings
volatile s0SettingsStruct actS0Settings[NUM_S0_COUNTERS];


//These are the interrupt routines. Make them as short as possible so we don't block main code
volatile unsigned long lastEdgeS0[NUM_S0_COUNTERS] = {0, 0};
volatile bool badEdge[NUM_S0_COUNTERS] = {0, 0}; //two edges is a pulse, so store as bool

//for debug
/*
  volatile unsigned long allLastEdgeS0[2][20];
  volatile int allLastEdgeS0Index[2] = {0, 0};
*/

IRAM_ATTR void countPulse(int i) {
  volatile unsigned long newEdgeS0 = millis();
  volatile unsigned long curPulseWidth = newEdgeS0 - lastEdgeS0[i];
  // debug
  /*
    if (allLastEdgeS0Index[i] < 20) {
    allLastEdgeS0[i][allLastEdgeS0Index[i]] = curPulseWidth;
    allLastEdgeS0Index[i]++;
    }
  */
  // end debug
  if ((curPulseWidth >=  actS0Settings[i].minimalPulseWidth) && (curPulseWidth <= actS0Settings[i].maximalPulseWidth) ) {
    if (actS0Data[i].lastPulse > 0) { //Do not calculate watt for the first pulse since reboot because we will always report a too high watt. Better to show 0 watt at first pulse.
      volatile unsigned long pulseInterval = newEdgeS0 - actS0Data[i].lastPulse; //calculate the interval between last valid pulse edge and this edge
      actS0Data[i].watt = (3600000000.0 / pulseInterval) / actS0Settings[i].ppkwh;
      if ((unsigned long)(actS0Data[i].nextReport - newEdgeS0) > MINREPORTEDS0TIME) { //pulse seen in standby interval so report directly
        actS0Data[i].nextReport = 0; // report now
      }
    }
    actS0Data[i].lastPulse = newEdgeS0; // store this edge to compare in next pulses for valid pulse and calculate watt
    actS0Data[i].pulses++;
    actS0Data[i].pulsesTotal++;
    actS0Data[i].goodPulses++;
    actS0Data[i].avgPulseWidth = ((actS0Data[i].avgPulseWidth * (actS0Data[i].goodPulses - 1)) + curPulseWidth ) / actS0Data[i].goodPulses;
    badEdge[i] = false; //set it to false again to allow to count two bad edges as a new bad pulse because we know we had a good pulse now
  } else {
    if (badEdge[i]) actS0Data[i].badPulses++; //there was already an edge before so count this one as a bad pulse
    badEdge[i] = !badEdge[i]; //for now count it as a bad edge (if it is a edge for a good pulse, this will reset to false a few lines above). The bool is for not counting each edge as a bad pulse, but only two bad edges.
  }
  lastEdgeS0[i] = newEdgeS0; //store this edge time for next use
}

IRAM_ATTR void onS0Pulse1Change() {
  countPulse(0); //port 1, index 0 of array
}

IRAM_ATTR void onS0Pulse2Change() {
  countPulse(1); //port 2, index 1 of array
}


void initS0Sensors(s0SettingsStruct s0Settings[]) {
  //setup s0 port 1

  //TODO: check if this is still necessary
  //actS0Settings[0].gpiopin = s0Settings[0].gpiopin;
  actS0Settings[0].gpiopin = DEFAULT_S0_PIN_1;
  actS0Settings[0].ppkwh = s0Settings[0].ppkwh;
  actS0Settings[0].lowerPowerInterval = s0Settings[0].lowerPowerInterval;
  actS0Settings[0].minimalPulseWidth = s0Settings[0].minimalPulseWidth;
  actS0Settings[0].maximalPulseWidth = s0Settings[0].maximalPulseWidth;

  pinMode(actS0Settings[0].gpiopin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(actS0Settings[0].gpiopin), onS0Pulse1Change, CHANGE);
  actS0Data[0].nextReport = millis() + MINREPORTEDS0TIME; //initial report after interval, not directly at boot

  //setup s0 port 2

  //TODO: check if this is still necessary
  //actS0Settings[1].gpiopin = s0Settings[1].gpiopin; 
  actS0Settings[1].gpiopin = DEFAULT_S0_PIN_2;
  actS0Settings[1].ppkwh = s0Settings[1].ppkwh;
  actS0Settings[1].lowerPowerInterval = s0Settings[1].lowerPowerInterval;
  actS0Settings[1].minimalPulseWidth = s0Settings[1].minimalPulseWidth;
  actS0Settings[1].maximalPulseWidth = s0Settings[1].maximalPulseWidth;

  pinMode(actS0Settings[1].gpiopin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(actS0Settings[1].gpiopin), onS0Pulse2Change, CHANGE);
  actS0Data[1].nextReport = millis() + MINREPORTEDS0TIME; //initial report after interval, not directly at boot
}

void restore_s0_Watthour(int s0Port, float watthour) {
  if ((s0Port == 1) || (s0Port == 2)) {
    unsigned long newTotal = (unsigned long)(watthour * (actS0Settings[s0Port - 1].ppkwh / 1000.0));
    if (newTotal > actS0Data[s0Port - 1].pulsesTotal) {
      noInterrupts();
      actS0Data[s0Port - 1].pulsesTotal = newTotal;
      interrupts();
    }
  }
}



void s0Loop(PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base, s0SettingsStruct s0Settings[]) {

  unsigned long millisThisLoop = millis();

  for (int i = 0 ; i < NUM_S0_COUNTERS ; i++) {
    char tmp_log_msg[256];

    //report after nextReport
    if (millisThisLoop > actS0Data[i].nextReport) {

      unsigned long lastPulseInterval = millisThisLoop - actS0Data[i].lastPulse;
      unsigned long calcMaxWatt = (3600000000.0 / lastPulseInterval) / actS0Settings[i].ppkwh; //calculate the maximum watt was is possible without receiving pulses during the report interval

      if (actS0Data[i].watt < ((3600000.0 / actS0Settings[i].ppkwh) / actS0Settings[i].lowerPowerInterval) ) { //watt is lower than possible in lower power interval time, so we are in low power counting mode
        actS0Data[i].nextReport = millisThisLoop + 1000 * actS0Settings[i].lowerPowerInterval;
        if ((actS0Data[i].watt) / 2 > calcMaxWatt) { //last known watt is higher than possible for the interval, so we need to bring it down fast
          actS0Data[i].watt = calcMaxWatt / 2;
        }
      }
      else { // we are in normal counting mode, report each MINREPORTEDS0TIME
        actS0Data[i].nextReport = millisThisLoop + MINREPORTEDS0TIME;
        if (actS0Data[i].watt > calcMaxWatt) { //last known watt is higher than possible since last report, so bring it down to wat is possible
          actS0Data[i].watt = calcMaxWatt;
        }
      }

      float Watthour = (actS0Data[i].pulses * ( 1000.0 / actS0Settings[i].ppkwh));
      float WatthourTotal = (actS0Data[i].pulsesTotal * ( 1000.0 / actS0Settings[i].ppkwh));
      actS0Data[i].lastReportWatthour = Watthour;

      noInterrupts();
      actS0Data[i].pulses = 0; //per message we report new wattHour, so pulses should be zero at start new message
      interrupts();

      //report using mqtt
      char log_msg[256];
      char mqtt_topic[256];
      char valueStr[20];

      //debug
      /*
        noInterrupts();
        int j = 0;
        while (allLastEdgeS0Index[i] > 0) {
        allLastEdgeS0Index[i]--;
        sprintf_P(log_msg, PSTR("Pulse widths seen on S0 port %d: Width: %lu"), (i + 1),  allLastEdgeS0[i][j] );
        //log_message(log_msg);
        Serial1.println(log_msg);
        j++;
        }
        interrupts();
      */
      //end debug

      sprintf_P(log_msg, PSTR("Pulses seen on S0 port %d: Good: %lu Bad: %lu Average good pulse width: %i"), (i + 1),  actS0Data[i].goodPulses, actS0Data[i].badPulses, actS0Data[i].avgPulseWidth);
      log_message(log_msg);

      sprintf_P(log_msg, PSTR("Measured Watthour on S0 port %d: %.2f"), (i + 1),  Watthour );
      log_message(log_msg);
      sprintf(valueStr, "%.2f", Watthour);
      sprintf_P(mqtt_topic, PSTR("%s/%s/Watthour/%d"), mqtt_topic_base, mqtt_topic_s0, (i + 1));
      mqtt_client.publish(mqtt_topic, valueStr, MQTT_RETAIN_VALUES);

      sprintf(log_msg, PSTR("Measured total Watthour on S0 port %d: %.2f"), (i + 1),  WatthourTotal );
      log_message(log_msg);
      sprintf(valueStr, "%.2f", WatthourTotal);
      sprintf(mqtt_topic, PSTR("%s/%s/WatthourTotal/%d"), mqtt_topic_base, mqtt_topic_s0, (i + 1));
      mqtt_client.publish(mqtt_topic, valueStr, MQTT_RETAIN_VALUES);
      sprintf(log_msg, PSTR("Calculated Watt on S0 port %d: %u"), (i + 1), actS0Data[i].watt);
      log_message(log_msg);
      sprintf(valueStr, "%u",  actS0Data[i].watt);
      sprintf(mqtt_topic, PSTR("%s/%s/Watt/%d"), mqtt_topic_base, mqtt_topic_s0, (i + 1));
      mqtt_client.publish(mqtt_topic, valueStr, MQTT_RETAIN_VALUES);
      //update GUI over websocket
      sprintf_P(log_msg, PSTR("{\"data\": {\"s0values\": {\"s0port\": %d, \"Watt\": %u, \"Watthour\": %.2f, \"WatthourTotal\": %.2f}}}"), i+1, actS0Data[i].watt,Watthour,WatthourTotal);
      websocket_write_all(log_msg, strlen(log_msg));         
      //send rules events
      char s0_event[32];
      snprintf_P(s0_event, sizeof(s0_event), PSTR("s0#watt_%d"), i + 1);
      rules_event_cb("", s0_event);
      snprintf_P(s0_event, sizeof(s0_event), PSTR("s0#watthour_%d"), i + 1);
      rules_event_cb("", s0_event);
      snprintf_P(s0_event, sizeof(s0_event), PSTR("s0#watthourtotal_%d"), i + 1);
      rules_event_cb("", s0_event);
    }
  }
}

unsigned long jsonPulses[NUM_S0_COUNTERS];

void s0JsonOutput(struct webserver_t *client) {
  webserver_send_content_P(client, PSTR("["), 1);
  for (int i = 0; i < NUM_S0_COUNTERS; i++) {
    webserver_send_content_P(client, PSTR("{\"S0 port\":"), 11);

    char str[12];
    itoa(i + 1, str, 10);
    webserver_send_content(client, str, strlen(str));

    webserver_send_content_P(client, PSTR(",\"Watt\":"), 8);

    itoa(actS0Data[i].watt, str, 10);
    webserver_send_content(client, str, strlen(str));

    webserver_send_content_P(client, PSTR(",\"Watthour\":"), 12);

    itoa(((actS0Data[i].pulsesTotal - jsonPulses[i]) * (1000.0 / actS0Settings[i].ppkwh)), str, 10);
    webserver_send_content(client, str, strlen(str));

    jsonPulses[i] = actS0Data[i].pulsesTotal;

    webserver_send_content_P(client, PSTR(",\"WatthourTotal\":"), 17);

    itoa((actS0Data[i].pulsesTotal * (1000.0 / actS0Settings[i].ppkwh)), str, 10);
    webserver_send_content(client, str, strlen(str));

    webserver_send_content_P(client, PSTR(",\"PulseQuality\":"), 16);

    itoa((100 * (actS0Data[i].goodPulses + 1) / (actS0Data[i].goodPulses + actS0Data[i].badPulses + 1)), str, 10);
    webserver_send_content(client, str, strlen(str));

    webserver_send_content_P(client, PSTR(",\"AvgPulseWidth\":"), 17);

    itoa(actS0Data[i].avgPulseWidth, str, 10);
    webserver_send_content(client, str, strlen(str));

    if (i < NUM_S0_COUNTERS - 1) {
      webserver_send_content_P(client, PSTR("},"), 2);
    } else {
      webserver_send_content_P(client, PSTR("}"), 1);
    }
  }
  webserver_send_content_P(client, PSTR("]"), 1);
}
