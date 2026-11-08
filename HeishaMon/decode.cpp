#include "decode.h"
#include "commands.h"
#include "rules.h"
#include "src/common/progmem.h"

void websocket_write_all(char *data, uint16_t data_len);

unsigned long lastalldatatime = 0;
unsigned long lastallextradatatime = 0;
unsigned long lastalloptdatatime = 0;

String getRawByte(byte input) {
  return String(input);
}

String getHeaterCapacity(byte input) {
  int encoded = (input >> 2) & 0b11;
  return String(encoded == 0 ? -1 : encoded * 3);
}

String getIntMinus1Times30(byte input) {
  return String(((int)input - 1) * 30);
}

String getZone2Active(byte input) {
  return String((input >> 1) & 1);
}

String getZone1Active(byte input) {
  return String(input & 1);
}

String getBit1(byte input) {
  return String(input  >> 7);
}

String getBit1and2(byte input) {
  return String((input  >> 6) - 1);
}

String getBit3and4(byte input) {
  return String(((input >> 4) & 0b11) - 1);
}

String getBit5and6(byte input) {
  return String(((input >> 2) & 0b11) - 1);
}

String getBit7and8(byte input) {
  return String((input & 0b11) - 1);
}

String getBit3and4and5(byte input) {
  return String(((input >> 3) & 0b111) - 1);
}

String getLeft5bits(byte input) {
  return String((input >> 3) - 1);
}

String getRight3bits(byte input) {
  return String((input & 0b111) - 1);
}

String getIntMinus1(byte input) {
  int value = (int)input - 1;
  return (String)value;
}

String getIntMinus128(byte input) {
  int value = (int)input - 128;
  return (String)value;
}

String getIntMinus1Div5(byte input) {
  return String((((float)input - 1) / 5), 1);
}

String getIntMinus1Div50(byte input) {
  return String((((float)input - 1) / 50), 2);
}

String getIntMinus1Times10(byte input) {
  int value = (int)input - 1;
  return (String)(value * 10);
}

String getIntMinus1Times50(byte input) {
  int value = (int)input - 1;
  return (String)(value * 50);
}


String unknown(byte input) {
  return "-1";
}

String getValvePID(byte input) {
  return String((((float)input - 1) / 2), 1);
}

String getOpMode(byte input) {
  switch ((int)(input & 0b111111)) {
    case 18:
      return "0";
    case 19:
      return "1";
    case 25:
      return "2";
    case 33:
      return "3";
    case 34:
      return "4";
    case 35:
      return "5";
    case 41:
      return "6";
    case 26:
      return "7";
    case 42:
      return "8";
    default:
      return "-1";
  }
}

String getModel(char* data) { // TOP92 //
  byte model[10] = { data[129], data[130], data[131], data[132], data[133], data[134], data[135], data[136], data[137], data[138]};
  char modelResult[31];
  for (size_t i = 0; i < 10; ++i) {
    sprintf(&modelResult[i*3], "%02X ", model[i]);
  }
  modelResult[29] = '\0';
  return String(modelResult);
}

String getPower(byte input) {
  int value = ((int)input - 1) * 200;
  return (String)value;
}

String getUintt16(char* data, byte addr) {
  uint16_t value = static_cast<uint16_t>((data[addr + 1] << 8) | data[addr]);
  return (String)(value - 1);
}

String getPumpFlow(char* data) {  // TOP1 //
  int PumpFlow1 = (int)data[170];
  float PumpFlow2 = (((float)data[169] - 1) / 256);
  float PumpFlow = PumpFlow1 + PumpFlow2;
  return String(PumpFlow, 2);
}

String getErrorInfo(char* data) { // TOP44 //
  int Error_type = (int)(data[113]);
  int Error_number = ((int)(data[114])) - 17;
  char Error_string[10];
  switch (Error_type) {
    case 177:                  //B1=F type error
      sprintf(Error_string, "F%02X", Error_number);
      break;
    case 161:                  //A1=H type error
      sprintf(Error_string, "H%02X", Error_number);
      break;
    default:
      sprintf(Error_string, "No error");
      break;
  }
  return String(Error_string);
}


void resetlastalldatatime() {
  lastalldatatime = 0;
  lastallextradatatime = 0;
  lastalloptdatatime = 0;
}

String getDataValue(char* data, unsigned int Topic_Number) {
  String Topic_Value;
  byte Input_Byte;
  switch (Topic_Number) { //switch on topic numbers, some have special needs
    case 1:
      Topic_Value = getPumpFlow(data);
      break;
    case 5: {
        byte cpy;
        memcpy_P(&cpy, &topicBytes[Topic_Number], sizeof(byte));
        Input_Byte = data[cpy];
        Topic_Value = topicFunctions[Topic_Number](Input_Byte);
        int fractional = (int)(data[118] & 0b111);
        switch (fractional) {
          case 1: // fractional .00
            break;
          case 2: // fractional .25
            Topic_Value = Topic_Value + ".25";
            break;
          case 3: // fractional .50
            Topic_Value = Topic_Value + ".50";
            break;
          case 4: // fractional .75
            Topic_Value = Topic_Value + ".75";
            break;
          default:
            break;
        }
      }
      break;
    case 6: {
        byte cpy;
        memcpy_P(&cpy, &topicBytes[Topic_Number], sizeof(byte));
        Input_Byte = data[cpy];
        Topic_Value = topicFunctions[Topic_Number](Input_Byte);
        int fractional = (int)((data[118] >> 3) & 0b111) ;
        switch (fractional) {
          case 1: // fractional .00
            break;
          case 2: // fractional .25
            Topic_Value = Topic_Value + ".25";
            break;
          case 3: // fractional .50
            Topic_Value = Topic_Value + ".50";
            break;
          case 4: // fractional .75
            Topic_Value = Topic_Value + ".75";
            break;
          default:
            break;
        }
      }
      break;
    case 11:
      Topic_Value = String(word(data[183], data[182]) - 1);
      break;
    case 12:
      Topic_Value = String(word(data[180], data[179]) - 1);
      break;
    case 90:
      Topic_Value = String(word(data[186], data[185]) - 1);
      break;
    case 91:
      Topic_Value = String(word(data[189], data[188]) - 1);
      break;
    case 44:
      Topic_Value = getErrorInfo(data);
      break;
    case 92:
      Topic_Value = getModel(data);
      break;
    default:
      byte cpy;
      memcpy_P(&cpy, &topicBytes[Topic_Number], sizeof(byte));
      Input_Byte = data[cpy];
      Topic_Value = topicFunctions[Topic_Number](Input_Byte);
      break;
  }
  return Topic_Value;
}

String getDataValueExtra(char* data, unsigned int Topic_Number) {
  String Topic_Value;
  switch (Topic_Number) { //switch on topic numbers, some have special needs
    default:
      byte addr;
      memcpy_P(&addr, &xtopicBytes[Topic_Number], sizeof(byte));
      Topic_Value = xtopicFunctions[Topic_Number](data, addr);
      break;
  }
  return Topic_Value;
}

String getOptDataValue(char* data, unsigned int Topic_Number) {
  String Topic_Value;
  switch (Topic_Number) { //switch on topic numbers, some have special needs
    case 0:
      Topic_Value = String(data[4] >> 7);
      break;
    case 1:
      Topic_Value = String((data[4] >> 5) & 0b11);
      break;
    case 2:
      Topic_Value = String((data[4] >> 4) & 0b1);
      break;
    case 3:
      Topic_Value = String((data[4] >> 2) & 0b11);
      break;
    case 4:
      Topic_Value = String((data[4] >> 1) & 0b1);
      break;
    case 5:
      Topic_Value = String((data[4] >> 0) & 0b1);
      break;
    case 6:
      Topic_Value = String((data[5] >> 0) & 0b1);
      break;
    default:
      break;
  }
  return Topic_Value;
}

String getFirstByte(byte input) {
  return String((input >> 4) - 1);
}

String getSecondByte(byte input) {
  return String((input & 0b1111) - 1);
}



// Decode ////////////////////////////////////////////////////////////////////////////
void decode_heatpump_data(char* data, char* actData, PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base, unsigned int updateAllTime) {
  bool updateTime = false;
  bool updateTopic[NUMBER_OF_TOPICS] = { false };

  if ((lastalldatatime == 0) || ((unsigned long)(millis() - lastalldatatime) > (1000 * updateAllTime))) {
    updateTime = true;
    lastalldatatime = millis();
  }
  for (unsigned int Topic_Number = 0 ; Topic_Number < NUMBER_OF_TOPICS ; Topic_Number++) {
    String Topic_Value;
    Topic_Value = getDataValue(data, Topic_Number);

    if(getDataValue(actData, Topic_Number) != Topic_Value) {
      updateTopic[Topic_Number] = true;
    }

    if (updateTime || updateTopic[Topic_Number]) {
      char log_msg[256];
      char mqtt_topic[256];
      sprintf_P(log_msg, PSTR("received TOP%d %s: %s"), Topic_Number, topics[Topic_Number], Topic_Value.c_str());
      log_message(log_msg);
      sprintf_P(mqtt_topic, PSTR("%s/%s/%s"), mqtt_topic_base, mqtt_topic_values, topics[Topic_Number]);
      mqtt_client.publish(mqtt_topic, Topic_Value.c_str(), MQTT_RETAIN_VALUES);
    }
  }
  memcpy(actData, data, DATASIZE);
  for (unsigned int Topic_Number = 0 ; Topic_Number < NUMBER_OF_TOPICS ; Topic_Number++) {
    if(updateTopic[Topic_Number]) {
      char log_msg[256];
      int maxvalue = atoi(topicDescription[Topic_Number][0]);
      String dataValue = getDataValue(actData, Topic_Number);
      if (maxvalue == 0) { //this takes the special case where the description is a real value description instead of a mode, so get description index 1
        if ((Topic_Number != 44) && (Topic_Number != 92)) {
          sprintf_P(log_msg, PSTR("{\"data\": {\"heishavalues\": {\"topic\": \"TOP%u\", \"value\": %s, \"description\": \"%s\"}}}"), Topic_Number, dataValue.c_str(),topicDescription[Topic_Number][1]);
        } else {
          sprintf_P(log_msg, PSTR("{\"data\": {\"heishavalues\": {\"topic\": \"TOP%u\", \"value\": \"%s\", \"description\": \"%s\"}}}"), Topic_Number, dataValue.c_str(),topicDescription[Topic_Number][1]);
        }
      } else {
        sprintf_P(log_msg, PSTR("{\"data\": {\"heishavalues\": {\"topic\": \"TOP%u\", \"value\": %s, \"description\": \"%s\"}}}"), Topic_Number, dataValue.c_str(),topicDescription[Topic_Number][dataValue.toInt() + 1]);
      }
      websocket_write_all(log_msg, strlen(log_msg));          
      rules_event_cb(_F("@"), topics[Topic_Number]);
    }
  }
}

void decode_heatpump_data_extra(char* data, char* actDataExtra, PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base, unsigned int updateAllTime) {
  bool updateTime = false;
  bool updateTopic[NUMBER_OF_TOPICS_EXTRA] = { false };

  if ((lastallextradatatime == 0) || ((unsigned long)(millis() - lastallextradatatime) > (1000 * updateAllTime))) {
    updateTime = true;
    lastallextradatatime = millis();
  }
  for (unsigned int Topic_Number = 0 ; Topic_Number < NUMBER_OF_TOPICS_EXTRA ; Topic_Number++) {
    String Topic_Value;
    Topic_Value = getDataValueExtra(data, Topic_Number);

    if(getDataValueExtra(actDataExtra, Topic_Number) != Topic_Value) {
      updateTopic[Topic_Number] = true;
    }

    if (updateTime || updateTopic[Topic_Number]) {
      char log_msg[256];
      char mqtt_topic[256];
      sprintf_P(log_msg, PSTR("received XTOP%d %s: %s"), Topic_Number, xtopics[Topic_Number], Topic_Value.c_str());
      log_message(log_msg);
      sprintf_P(mqtt_topic, PSTR("%s/%s/%s"), mqtt_topic_base, mqtt_topic_xvalues, xtopics[Topic_Number]);
      mqtt_client.publish(mqtt_topic, Topic_Value.c_str(), MQTT_RETAIN_VALUES);
    }
  }
  memcpy(actDataExtra, data, DATASIZE);
  for (unsigned int Topic_Number = 0 ; Topic_Number < NUMBER_OF_TOPICS_EXTRA ; Topic_Number++) {
    if(updateTopic[Topic_Number]) {
      char log_msg[256];
      int maxvalue = atoi(xtopicDescription[Topic_Number][0]);
      String dataValue = getDataValueExtra(actDataExtra, Topic_Number);
      if (maxvalue == 0) { //this takes the special case where the description is a real value description instead of a mode, so get description index 1
        sprintf_P(log_msg, PSTR("{\"data\": {\"heishavalues\": {\"topic\": \"XTOP%u\", \"value\": %s, \"description\": \"%s\"}}}"), Topic_Number, dataValue.c_str(),xtopicDescription[Topic_Number][1]);
      } else {
        sprintf_P(log_msg, PSTR("{\"data\": {\"heishavalues\": {\"topic\": \"XTOP%u\", \"value\": %s, \"description\": \"%s\"}}}"), Topic_Number, dataValue.c_str(),xtopicDescription[Topic_Number][dataValue.toInt() + 1]);
      }
      websocket_write_all(log_msg, strlen(log_msg));         
      rules_event_cb(_F("@"), xtopics[Topic_Number]);
    }
  }
}

void decode_optional_heatpump_data(char* data, char* actOptData, PubSubClient & mqtt_client, void (*log_message)(char*), char* mqtt_topic_base, unsigned int updateAllTime) {
  bool updateTime = false;
  bool updateTopic[NUMBER_OF_OPT_TOPICS] = { false };

  if ((lastalloptdatatime == 0) || ((unsigned long)(millis() - lastalloptdatatime) > (1000 * updateAllTime))) {
    updateTime = true;
    lastalloptdatatime = millis();
  }
  for (unsigned int Topic_Number = 0 ; Topic_Number < NUMBER_OF_OPT_TOPICS ; Topic_Number++) {
    String Topic_Value;
    Topic_Value = getOptDataValue(data, Topic_Number);

    if(getOptDataValue(actOptData, Topic_Number) != Topic_Value) {
      updateTopic[Topic_Number] = true;
    }

    if (updateTime || updateTopic[Topic_Number]) {
      char log_msg[256];
      char mqtt_topic[256];
      sprintf_P(log_msg, PSTR("received OPT%d %s: %s"), Topic_Number, optTopics[Topic_Number], Topic_Value.c_str());
      log_message(log_msg);
      sprintf_P(mqtt_topic, PSTR("%s/%s/%s"), mqtt_topic_base, mqtt_topic_pcbvalues, optTopics[Topic_Number]);
      mqtt_client.publish(mqtt_topic, Topic_Value.c_str(), MQTT_RETAIN_VALUES);

    }
  }
  //response to heatpump should contain the data from heatpump on byte 4 and 5
  byte valueByte4 = data[4];
  optionalPCBQuery[4] = valueByte4;
  byte valueByte5 = data[5];
  optionalPCBQuery[5] = valueByte5;

  memcpy(actOptData, data, OPTDATASIZE);
  for (unsigned int Topic_Number = 0 ; Topic_Number < NUMBER_OF_OPT_TOPICS ; Topic_Number++) {
    if(updateTopic[Topic_Number]) {
      char log_msg[256];
      int maxvalue = atoi(opttopicDescription[Topic_Number][0]);
      String dataValue = getOptDataValue(actOptData, Topic_Number);
      if (maxvalue == 0) { //this takes the special case where the description is a real value description instead of a mode, so get description index 1
        sprintf_P(log_msg, PSTR("{\"data\": {\"heishavalues\": {\"topic\": \"OPT%u\", \"value\": %s, \"description\": \"%s\"}}}"), Topic_Number, dataValue.c_str(),opttopicDescription[Topic_Number][1]);
      } else {
        sprintf_P(log_msg, PSTR("{\"data\": {\"heishavalues\": {\"topic\": \"OPT%u\", \"value\": %s, \"description\": \"%s\"}}}"), Topic_Number, dataValue.c_str(),opttopicDescription[Topic_Number][dataValue.toInt() + 1]);
      }      
      websocket_write_all(log_msg, strlen(log_msg));
      rules_event_cb(_F("@"), optTopics[Topic_Number]);
    }
  }

}
