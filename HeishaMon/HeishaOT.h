#ifndef _HEISHA_OT_H_
#define _HEISHA_OT_H_

#include <PubSubClient.h>
#include "src/common/webserver.h"

// opentherm
#if defined(ESP8266)
#define inOTPin 3 //RX pin from ESP8266
#define outOTPin 1 //TX pin from ESP8266
#elif defined(ESP32)
#define inOTPin 6
#define outOTPin 7
#endif

// mqtt
extern const char* mqtt_topic_opentherm_read;
extern const char* mqtt_topic_opentherm_write;


#define TBOOL 1
#define TFLOAT 2
#define TINT8 3

typedef struct heishaOTDataStruct_t {
  const char *name;
  uint8_t type;
  union {
    bool b;
    float f;
    int8_t s8;
  } value;
  uint8_t rw;
} heishaOTDataStruct_t;

extern struct heishaOTDataStruct_t heishaOTDataStruct[];

void HeishaOTSetup();
void HeishaOTLoop(char *actDat, PubSubClient &mqtt_client, char* mqtt_topic_base);
void mqttOTCallback(char* topic, char* value);
void openthermJsonOutput(struct webserver_t *client);
void openthermJsonMember(struct webserver_t *client, int i);
int openthermJsonFind(const char *name);

#endif