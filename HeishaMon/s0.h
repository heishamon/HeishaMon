#pragma once
#include <PubSubClient.h>
#include "src/common/webserver.h"

#include "s0data.h"

void initS0Sensors(s0SettingsStruct s0Settings[]);
void restore_s0_Watthour(int s0Port, float watthour);
void s0Loop(PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base, s0SettingsStruct s0Settings[]);
void s0JsonOutput(struct webserver_t *client);
