#include "src/opentherm/opentherm.h"
#include "HeishaOT.h"
#include "decode.h"
#include "rules.h"
#include "webfunctions.h"
#include "src/common/stricmp.h"
#include "src/common/progmem.h"

OpenTherm ot(inOTPin, outOTPin, true);

const char* mqtt_topic_opentherm_read PROGMEM = "opentherm/read";
const char* mqtt_topic_opentherm_write PROGMEM = "opentherm/write";


unsigned long otResponse = 0;

struct heishaOTDataStruct_t heishaOTDataStruct[] = {
  //WRITE values
  { "chEnable", TBOOL, { .b = false }, 3 }, //is central heating enabled by thermostat
  { "dhwEnable", TBOOL, { .b = false }, 3 }, //is dhw heating enabled by thermostat
  { "coolingEnable", TBOOL, { .b = false }, 3 }, //is cooling mode enabled by thermostat
  { "roomTemp", TFLOAT, { .f = -99 }, 3 }, //what is measured room temp by thermostat
  { "roomTempSet", TFLOAT, { .f = -99 }, 3 }, //what is request room temp setpoint by thermostat
  { "chSetpoint", TFLOAT, { .f = -99 }, 3 }, //what is calculated Ta setpoint by thermostat
  { "maxRelativeModulation", TFLOAT, { .f = -99 }, 3 }, //what is requested max relative modulation (0-100%)
  { "coolingControl", TFLOAT, { .f = -99 }, 3 }, //what is requested cooling amount (0-100%)
  //READ AND WRITE values
  { "dhwSetpoint", TFLOAT, { .f = 65 }, 2 }, //what is DHW setpoint by thermostat
  { "maxTSet", TFLOAT, { .f = 65 }, 2 }, //max ch setpoint
  //READ values
  { "chPressure", TFLOAT, { .f = -99 }, 1 }, //provides measured water pressure of central heating
  { "relativeModulation", TFLOAT, { .f = -99 }, 1 }, //provides the current level of relative modulation (0-100%)
  { "outsideTemp", TFLOAT, { .f = -99 }, 1 }, //provides measured outside temp to thermostat
  { "inletTemp", TFLOAT, { .f = -99 }, 1 }, //provides measured Treturn temp to thermostat
  { "outletTemp", TFLOAT, { .f = -99 }, 1 }, //provides measured Tout (boiler) temp to thermostat
  { "dhwTemp", TFLOAT, { .f = -99 }, 1 }, //provides measured dhw water temp to to thermostat
  { "flameState", TBOOL, { .b = false }, 1 }, //provides current flame state to thermostat
  { "chState", TBOOL, { .b = false }, 1 }, //provides if heatpump is in centrale heating state
  { "dhwState", TBOOL, { .b = false }, 1 }, //provides if heatpump is in dhw heating state
  { "coolingState", TBOOL, { .b = false }, 1 }, //provides if heatpump is in cooling state
  { "roomSetOverride", TFLOAT, { .f = 0 }, 1 }, //provides a room setpoint override ID9 (not implemented completly in heishamon)
  { "dhwSetUppBound", TINT8, { .s8 = 75 }, 1 }, //provides DHW upper boundary, default to 75 degrees celcius
  { "dhwSetLowBound", TINT8, { .s8 = 40 }, 1 }, //provides DHW lower boundary, default to 40 degrees celcius
  { "chSetUppBound", TINT8, { .s8 = 65 }, 1 }, //provides CH upper boundary, default to 65 degrees celcius
  { "chSetLowBound", TINT8, { .s8 = 20 }, 1 }, //provides CH lower boundary, default to 20 degrees celcius    
  { NULL, 0, 0, 0 }
};

void mqttPublish(char* topic, char* subtopic, char* value, bool retain);
void mqttPublish(char* topic, char* subtopic, char* value);

struct heishaOTDataStruct_t *getOTStructMember(const char *name) {
  int i = 0;
  while(heishaOTDataStruct[i].name != NULL) {
    if(strcmp(heishaOTDataStruct[i].name, name) == 0) {
      return &heishaOTDataStruct[i];
    }
    i++;
  }
  return NULL;
}

void processOTRequest(unsigned long request, OpenThermResponseStatus status) {
 if (status != OpenThermResponseStatus::SUCCESS) {
    log_message(_F("OpenTherm: Request invalid!"));
    char log_msg[512];
    sprintf_P(log_msg, PSTR("OpenTherm: Unknown data ID: %u (%#010lx)"), (unsigned int)ot.getDataID(request), request);
    log_message(log_msg);    
 } else {
  char log_msg[512];
  {
    //only for debugging OT messages
    //char str[200];
    //sprintf_P(str, PSTR("%#010x"), request);
    //mqttPublish((char*)mqtt_topic_opentherm_write, _F("raw"), str, false);
  }
  switch (ot.getDataID(request)) {
    case OpenThermMessageID::Status: { //mandatory
        unsigned long data = ot.getUInt(request);
        unsigned int CHEnable = (data >> 8) & (1 << 0);
        unsigned int DHWEnable = ((data >> 8) & (1 << 1)) >> 1;
        unsigned int Cooling = ((data >> 8) & (1 << 2)) >> 2;
        unsigned int OTCEnable = ((data >> 8) & (1 << 3)) >> 3;
        unsigned int CH2Enable = ((data >> 8) & (1 << 4)) >> 4;
        unsigned int SWMode = ((data >> 8) & (1 << 5)) >> 5;
        unsigned int DHWBlock = ((data >> 8) & (1 << 6)) >> 6;

        if ((bool)CHEnable != getOTStructMember(_F("chEnable"))->value.b) { //only publish if changed
          getOTStructMember(_F("chEnable"))->value.b = (bool)CHEnable;
          CHEnable ? mqttPublish((char*)mqtt_topic_opentherm_write, _F("chEnable"), _F("true")) : mqttPublish((char*)mqtt_topic_opentherm_write, _F("chEnable"), _F("false")) ;
          sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %s}}}"), _F("chEnable"), CHEnable ? _F("true") : _F("false"));
          websocket_write_all(log_msg, strlen(log_msg));
        }
        if ((bool)DHWEnable != getOTStructMember(_F("dhwEnable"))->value.b) { //only publish if changed
          getOTStructMember(_F("dhwEnable"))->value.b = (bool)DHWEnable;
          DHWEnable ? mqttPublish((char*)mqtt_topic_opentherm_write, _F("dhwEnable"), _F("true")) : mqttPublish((char*)mqtt_topic_opentherm_write, _F("dhwEnable"), _F("false")) ;
          sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %s}}}"), _F("dhwEnable"), DHWEnable ? _F("true") : _F("false"));
          websocket_write_all(log_msg, strlen(log_msg));
        }
        if ((bool)Cooling != getOTStructMember(_F("coolingEnable"))->value.b) { //only publish if changed
          getOTStructMember(_F("coolingEnable"))->value.b = (bool)Cooling;
          Cooling ? mqttPublish((char*)mqtt_topic_opentherm_write, _F("coolingEnable"), _F("true")) : mqttPublish((char*)mqtt_topic_opentherm_write, _F("coolingEnable"), _F("false")) ;
          sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %s}}}"), _F("coolingEnable"), Cooling ? _F("true") : _F("false"));
          websocket_write_all(log_msg, strlen(log_msg));
        }

        sprintf_P(log_msg, PSTR(
                "OpenTherm: Received status check: %lu, CH: %u, DHW: %u, Cooling, %u, OTC: %u, CH2: %u, SWMode: %u, DHWBlock: %u"),
                data >> 8, CHEnable, DHWEnable, Cooling, OTCEnable, CH2Enable, SWMode, DHWBlock
               );
        log_message(log_msg);
        //clean slave bits from 2-byte data
        data = ((data >> 8) << 8);

        unsigned int FaultInd = false;
        unsigned int CHMode = (unsigned int)getOTStructMember(_F("chState"))->value.b;
        unsigned int FlameStatus = (unsigned int)getOTStructMember(_F("flameState"))->value.b;
        unsigned int DHWMode = (unsigned int)getOTStructMember(_F("dhwState"))->value.b;
        unsigned int CoolingStatus = (unsigned int)getOTStructMember(_F("coolingState"))->value.b;;
        unsigned int CH2 = false;
        unsigned int DiagInd = false;
        sprintf_P(log_msg,
                PSTR("OpenTherm: Send status: CH: %d, Flame:%d, DHW: %d"),
                CHMode, FlameStatus, DHWMode
               );
        log_message(log_msg);
        unsigned int responsedata = FaultInd | (CHMode << 1) | (DHWMode << 2) | (FlameStatus << 3) | (CoolingStatus << 4) | (CH2 << 5) | (DiagInd << 6);
        otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::Status, (data |= responsedata));
        rules_event_cb(_F("?"), _F("chEnable"));
        rules_event_cb(_F("?"), _F("dhwEnable"));
        rules_event_cb(_F("?"), _F("coolingEnable"));
      } break;
    case OpenThermMessageID::TSet: { //mandatory
        char str[200];
        sprintf_P((char *)&str, PSTR("%.*f"), 4, ot.getFloat(request));
        sprintf_P(log_msg, PSTR("OpenTherm: control setpoint TSet: %s"), str);
        log_message(log_msg);
        if (getOTStructMember(_F("chSetpoint"))->value.f != ot.getFloat(request)) { //only publish if changed
          getOTStructMember(_F("chSetpoint"))->value.f = ot.getFloat(request);
          mqttPublish((char*)mqtt_topic_opentherm_write, _F("chSetpoint"), str);
          sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("chSetpoint"), getOTStructMember(_F("chSetpoint"))->value.f);
          websocket_write_all(log_msg, strlen(log_msg));
        }
        otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, OpenThermMessageID::TSet, request & 0xffff);
        rules_event_cb(_F("?"), _F("chsetpoint"));
      } break;
      case OpenThermMessageID::MConfigMMemberIDcode: {
      unsigned long data = ot.getUInt(request);
      unsigned int SmartPower = (data >> 8) & (1 << 0);
      data &= ~(1 << 8); //disable smartpower for now, we don't support it yet
      sprintf_P(log_msg,
			  PSTR("OpenTherm: Received master config: %u, Smartpower: %u"),
              data >> 8, SmartPower
             );
      log_message(log_msg);
      otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, OpenThermMessageID::MConfigMMemberIDcode, data);
      //ot.setSmartPower((bool)SmartPower); not working correctly yet
      } break;
    case OpenThermMessageID::SConfigSMemberIDcode: { //mandatory
        log_message(_F("OpenTherm: Received read slave config"));
        unsigned int DHW = true;
        unsigned int ModulationOrOnOff = false; //false means modulation according to specification v2.2
        unsigned int Cool = true;
        unsigned int DHWConf = false;
        unsigned int Pump = false;
        unsigned int CH2 = false; // no 2nd zone yet

        unsigned int data = DHW | (ModulationOrOnOff << 1) | (Cool << 2) | (DHWConf << 3) | (Pump << 4) | (CH2 << 5);
        data <<= 8;
        otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::SConfigSMemberIDcode, data);

      } break;
    case OpenThermMessageID::MaxRelModLevelSetting: { //mandatory
        char str[200];
        sprintf_P((char *)&str, PSTR("%.*f"), 4, ot.getFloat(request));
        sprintf_P(log_msg, PSTR("OpenTherm: Max relative modulation  requested: %s"), str);
        log_message(log_msg);
        if (getOTStructMember(_F("maxRelativeModulation"))->value.f != ot.getFloat(request)) {
          getOTStructMember(_F("maxRelativeModulation"))->value.f = ot.getFloat(request);
          if ( getOTStructMember(_F("relativeModulation"))->value.f > getOTStructMember(_F("maxRelativeModulation"))->value.f) { //need to change the relative modulation on the fly to comply with max requested
            getOTStructMember(_F("relativeModulation"))->value.f = getOTStructMember(_F("maxRelativeModulation"))->value.f;
          }
          mqttPublish((char*)mqtt_topic_opentherm_write, _F("maxRelativeModulation"), str);
          sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("maxRelativeModulation"), getOTStructMember(_F("maxRelativeModulation"))->value.f);
          websocket_write_all(log_msg, strlen(log_msg));          
        }
        otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, OpenThermMessageID::MaxRelModLevelSetting, request & 0xffff); //ACK for mandatory fields
      } break;
    case OpenThermMessageID::RelModLevel: { //mandatory
        log_message(_F("OpenTherm: Received read relative modulation level"));
        if ((getOTStructMember(_F("relativeModulation"))->value.f >= 0) && (getOTStructMember(_F("relativeModulation"))->value.f <= 100) ) {
          unsigned long data = ot.temperatureToData(getOTStructMember(_F("relativeModulation"))->value.f);
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::RelModLevel, data);

        } else {
          otResponse = ot.buildResponse(OpenThermMessageType::DATA_INVALID, OpenThermMessageID::RelModLevel, request & 0xffff);
        }        
      } break;
    case OpenThermMessageID::Tboiler: { //mandatory
        log_message(_F("OpenTherm: Received read boiler flow temp (outlet)"));
        if (getOTStructMember(_F("outletTemp"))->value.f > -99) {
          unsigned long data = ot.temperatureToData(getOTStructMember(_F("outletTemp"))->value.f);
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::Tboiler, data);

        } else {
          otResponse = ot.buildResponse(OpenThermMessageType::DATA_INVALID, OpenThermMessageID::Tboiler, request & 0xffff);
        }
      } break;
    // now adding some more useful, not mandatory, types
    case OpenThermMessageID::RBPflags: { //Pre-Defined Remote Boiler Parameters
        log_message(_F("OpenTherm: Received Remote Boiler parameters request"));
        //fixed settings for now
        const unsigned int DHWsetTransfer = true;
        const unsigned int maxCHsetTransfer = true;
        const unsigned int DHWsetReadWrite = true;
        const unsigned int maxCHsetReadWrite = true;
        const unsigned int responsedata = DHWsetReadWrite | (maxCHsetReadWrite << 1) | (DHWsetTransfer << 8) | (maxCHsetTransfer << 9);
        otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::RBPflags, responsedata);
      } break;
    case OpenThermMessageID::CoolingControl: { //mandatory
        char str[200];
        sprintf_P((char *)&str, PSTR("%.*f"), 4, ot.getFloat(request));
        sprintf_P(log_msg, PSTR("OpenTherm: cooling control amount requested: %s"), str);
        log_message(log_msg);
        if (getOTStructMember(_F("coolingControl"))->value.f != ot.getFloat(request)) {
          getOTStructMember(_F("coolingControl"))->value.f = ot.getFloat(request);  
          mqttPublish((char*)mqtt_topic_opentherm_write, _F("coolingControl"), str);
          sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("coolingControl"), getOTStructMember(_F("coolingControl"))->value.f);
          websocket_write_all(log_msg, strlen(log_msg));
        }
        otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, OpenThermMessageID::CoolingControl, request & 0xffff);
        rules_event_cb(_F("?"), _F("coolingControl"));
      } break;
    case OpenThermMessageID::TdhwSetUBTdhwSetLB : { //DHW boundaries
        log_message(_F("OpenTherm: Received DHW set boundaries request"));
        uint16_t result = 0;
        result |= ((getOTStructMember(_F("dhwSetUppBound"))->value.s8 & 0xFF) << 8); 
        result |= (getOTStructMember(_F("dhwSetLowBound"))->value.s8 & 0xFF);
        otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::TdhwSetUBTdhwSetLB, result);
      } break;
    case OpenThermMessageID::MaxTSetUBMaxTSetLB  : { //CHset boundaries
        log_message(_F("OpenTherm: Received CH set boundaries request"));
        uint16_t result = 0;
        result |= ((getOTStructMember(_F("chSetUppBound"))->value.s8 & 0xFF) << 8); 
        result |= (getOTStructMember(_F("chSetLowBound"))->value.s8 & 0xFF);     
        otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::MaxTSetUBMaxTSetLB, result);
      } break;
    case OpenThermMessageID::Tr: {
        char str[200];
        sprintf_P((char *)&str, PSTR("%.*f"), 4, ot.getFloat(request));
        sprintf_P(log_msg, PSTR("OpenTherm: Room temp: %s"), str);
        log_message(log_msg);
        if (getOTStructMember(_F("roomTemp"))->value.f != ot.getFloat(request)) {
          mqttPublish((char*)mqtt_topic_opentherm_write, _F("roomTemp"), str);
          getOTStructMember(_F("roomTemp"))->value.f = ot.getFloat(request);
          sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("roomTemp"), getOTStructMember(_F("roomTemp"))->value.f);
          websocket_write_all(log_msg, strlen(log_msg));
        }
        otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, OpenThermMessageID::Tr, request & 0xffff);
        rules_event_cb(_F("?"), _F("roomtemp"));
      } break;
    case OpenThermMessageID::TrSet: {
        char str[200];
        sprintf_P((char *)&str, PSTR("%.*f"), 4, ot.getFloat(request));
        sprintf_P(log_msg, PSTR("OpenTherm: Room setpoint: %s"), str);
        log_message(log_msg);
        if (getOTStructMember(_F("roomTempSet"))->value.f != ot.getFloat(request)) {
          getOTStructMember(_F("roomTempSet"))->value.f = ot.getFloat(request);
          mqttPublish((char*)mqtt_topic_opentherm_write, _F("roomTempSet"), str);
          sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("roomTempSet"), getOTStructMember(_F("roomTempSet"))->value.f);
          websocket_write_all(log_msg, strlen(log_msg));          
        }
        otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, OpenThermMessageID::TrSet, request & 0xffff);
        rules_event_cb(_F("?"), _F("roomtempset"));
      } break;
    case OpenThermMessageID::TdhwSet: {
        if (ot.getMessageType(request) == OpenThermMessageType::WRITE_DATA) {
          char str[200];
          sprintf_P((char *)&str, PSTR("%.*f"), 4, ot.getFloat(request));
          sprintf_P(log_msg, PSTR("OpenTherm: Write request DHW setpoint: %s"), str);
          log_message(log_msg);
          if (getOTStructMember(_F("dhwSetpoint"))->value.f != ot.getFloat(request)) {
            getOTStructMember(_F("dhwSetpoint"))->value.f = ot.getFloat(request);
            mqttPublish((char*)mqtt_topic_opentherm_write, _F("dhwSetpoint"), str);    
            sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("dhwSetpoint"), getOTStructMember(_F("dhwSetpoint"))->value.f);
            websocket_write_all(log_msg, strlen(log_msg));                      
          }
          otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, OpenThermMessageID::TdhwSet, ot.temperatureToData(getOTStructMember(_F("dhwSetpoint"))->value.f));
        } else { //READ_DATA
          sprintf_P(log_msg, PSTR("OpenTherm: Read request DHW setpoint"));
          log_message(log_msg);
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::TdhwSet, ot.temperatureToData(getOTStructMember(_F("dhwSetpoint"))->value.f));
          rules_event_cb(_F("?"), _F("dhwsetpoint"));
        } 
      } break;
    case OpenThermMessageID::MaxTSet: {
        if (ot.getMessageType(request) == OpenThermMessageType::WRITE_DATA) {
          char str[200];
          sprintf_P((char *)&str, PSTR("%.*f"), 4, ot.getFloat(request));
          sprintf_P(log_msg, PSTR("OpenTherm: Write request Max Ta-set setpoint: %s"), str);
          log_message(log_msg);
          if (getOTStructMember(_F("maxTSet"))->value.f != ot.getFloat(request)) {
            getOTStructMember(_F("maxTSet"))->value.f = ot.getFloat(request);
            mqttPublish((char*)mqtt_topic_opentherm_write, _F("maxTSet"), str);
            sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("maxTSet"), getOTStructMember(_F("maxTSet"))->value.f);
            websocket_write_all(log_msg, strlen(log_msg));                       
          }
          otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, OpenThermMessageID::MaxTSet, ot.temperatureToData(getOTStructMember(_F("maxTSet"))->value.f));
        } else { //READ_DATA
          sprintf_P(log_msg, PSTR("OpenTherm: Read request Max Ta-set setpoint"));
          log_message(log_msg);
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::MaxTSet, ot.temperatureToData(getOTStructMember(_F("maxTSet"))->value.f));
          rules_event_cb(_F("?"), _F("maxtset"));
        }
      } break;
    case OpenThermMessageID::Tret: {
        log_message(_F("OpenTherm: Received read boiler flow temp (inlet)"));
        if (getOTStructMember(_F("inletTemp"))->value.f > -99) {
          unsigned long data = ot.temperatureToData(getOTStructMember(_F("inletTemp"))->value.f);
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::Tret, data);
        } else {
          otResponse = ot.buildResponse(OpenThermMessageType::DATA_INVALID, OpenThermMessageID::Tret, request & 0xffff);
        }
      } break;
    case OpenThermMessageID::Tdhw: {
        log_message(_F("OpenTherm: Received read DHW temp"));
        if (getOTStructMember(_F("dhwTemp"))->value.f > -99) {
          unsigned long data = ot.temperatureToData(getOTStructMember(_F("dhwTemp"))->value.f);
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::Tdhw, data);
        } else {
          otResponse = ot.buildResponse(OpenThermMessageType::DATA_INVALID, OpenThermMessageID::Tdhw, request & 0xffff);
        }
      } break;
    case OpenThermMessageID::CHPressure: {
        log_message(_F("OpenTherm: Received read water pressure"));
        if (getOTStructMember(_F("chPressure"))->value.f > -99) {
          unsigned long data = ot.temperatureToData(getOTStructMember(_F("chPressure"))->value.f);
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::CHPressure, data);

        } else {
          otResponse = ot.buildResponse(OpenThermMessageType::DATA_INVALID, OpenThermMessageID::CHPressure, request & 0xffff);
        }
      } break;      
    case OpenThermMessageID::Toutside: {
        log_message(_F("OpenTherm: Received read outside temp"));
        if (getOTStructMember(_F("outsideTemp"))->value.f > -99) {
          unsigned long data = ot.temperatureToData(getOTStructMember(_F("outsideTemp"))->value.f);
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::Toutside, data);

        } else {
          otResponse = ot.buildResponse(OpenThermMessageType::DATA_INVALID, OpenThermMessageID::Toutside, request & 0xffff);
        }
      } break;
    case OpenThermMessageID::TrOverride: {
        log_message(_F("OpenTherm: Received read room set override temp"));
        if (getOTStructMember(_F("roomSetOverride"))->value.f > -99) {
          unsigned long data = ot.temperatureToData(getOTStructMember(_F("roomSetOverride"))->value.f);
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::TrOverride, data);

        } else {
          otResponse = ot.buildResponse(OpenThermMessageType::DATA_INVALID, OpenThermMessageID::TrOverride, request & 0xffff);
        }
      } break;
    //for date/time requests we don't store anything, just answer the time we know from heishamon  
    //still need to confirm this works correctly. Haven't seen a thermostat which does ask for the time and use it
    case OpenThermMessageID::DayTime: {
        if (ot.getMessageType(request) == OpenThermMessageType::READ_DATA) {
          log_message(_F("OpenTherm: Received time request"));
          time_t rawtime;
          rawtime = time(NULL);
          struct tm *timeinfo = localtime(&rawtime);
          uint16_t result = 0;
          result |= ((((timeinfo->tm_wday + 6) % 7 + 1) & 0x07) << 13);  // Set weekday in the leftmost 3 bits (16 - 3 = 13).  Opentherm counting monday=1, sunday=7
          result |= ((timeinfo->tm_hour & 0x1F) << 8);     // Set hours in the next 5 bits (13 - 5 = 8)
          result |= (timeinfo->tm_min & 0xFF);          // Set minutes in the rightmost 8 bits
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::DayTime, result);
        } else {
          log_message(_F("OpenTherm: Ignore time information set"));
          otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, ot.getDataID(request), 0);
        }
      } break;    
    case OpenThermMessageID::Date: {
        if (ot.getMessageType(request) == OpenThermMessageType::READ_DATA) {
          log_message(_F("OpenTherm: Received date request"));
          time_t rawtime;
          rawtime = time(NULL);
          struct tm *timeinfo = localtime(&rawtime);
          uint16_t result = 0;
          result |= (((timeinfo->tm_mon + 1) & 0xFF) << 8);  // Set month in the leftmost 8 bits. Month + 1 for opentherm counting from 1=january
          result |= (timeinfo->tm_mday & 0xFF);        // Set day of month in the rightmost 8 bits
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::Date, result);
        } else {
          log_message(_F("OpenTherm: Ignore date information set"));
          otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, ot.getDataID(request), 0);
        }
      } break;
    case OpenThermMessageID::Year: {
        if (ot.getMessageType(request) == OpenThermMessageType::READ_DATA) {
          log_message(_F("OpenTherm: Received year request"));
          time_t rawtime;
          rawtime = time(NULL);
          struct tm *timeinfo = localtime(&rawtime);
          uint16_t result = timeinfo->tm_year + 1900; //plus 1900 makes it the real year
          otResponse = ot.buildResponse(OpenThermMessageType::READ_ACK, OpenThermMessageID::Year, result);
        } else {
          log_message(_F("OpenTherm: Ignore year information set"));
          otResponse = ot.buildResponse(OpenThermMessageType::WRITE_ACK, ot.getDataID(request), 0);
        }
      } break;    
    default: {
        sprintf_P(log_msg, PSTR("OpenTherm: Unknown data ID: %u (%#010lx)"), (unsigned int)ot.getDataID(request), request);
        log_message(log_msg);
        otResponse = ot.buildResponse(OpenThermMessageType::UNKNOWN_DATA_ID, ot.getDataID(request), 0);
      } break;
  }
 }
}

void IRAM_ATTR handleOTInterrupt() {
  ot.handleInterrupt();
}

void HeishaOTSetup() {
  ot.begin(handleOTInterrupt, processOTRequest);
}

void HeishaOTLoop(char * actData, PubSubClient &mqtt_client, char* mqtt_topic_base) {
  // opentherm loop
  if (otResponse && ot.isReady()) {
    ot.sendResponse(otResponse);
    otResponse = 0;
  }
  ot.process();
}

void mqttOTCallback(char* topic, char* value) {
  char log_msg[512];
  if (strcmp_P(topic,PSTR("chPressure")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'chPressure'"));
    getOTStructMember(_F("chPressure"))->value.f = String(value).toFloat();
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("chPressure"), getOTStructMember(_F("chPressure"))->value.f); websocket_write_all(log_msg, strlen(log_msg));    
  }
  else if (strcmp_P(topic,PSTR("outsideTemp")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'outsideTemp'"));
    getOTStructMember(_F("outsideTemp"))->value.f = String(value).toFloat();
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("outsideTemp"), getOTStructMember(_F("outsideTemp"))->value.f); websocket_write_all(log_msg, strlen(log_msg));
  }
  else if (strcmp_P(topic,PSTR("inletTemp")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'inletTemp'"));
    getOTStructMember(_F("inletTemp"))->value.f = String(value).toFloat();
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("inletTemp"), getOTStructMember(_F("inletTemp"))->value.f); websocket_write_all(log_msg, strlen(log_msg));
  }
  else if (strcmp_P(topic, PSTR("outletTemp")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'outletTemp'"));
    getOTStructMember(_F("outletTemp"))->value.f = String(value).toFloat();
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("outletTemp"), getOTStructMember(_F("outletTemp"))->value.f); websocket_write_all(log_msg, strlen(log_msg));
  }
  else if (strcmp_P(topic, PSTR("dhwTemp")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'dhwTemp'"));
    getOTStructMember(_F("dhwTemp"))->value.f = String(value).toFloat();
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("dhwTemp"), getOTStructMember(_F("dhwTemp"))->value.f); websocket_write_all(log_msg, strlen(log_msg));
  }
  else if (strcmp_P(topic, PSTR("dhwSetpoint")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'dhwSetpoint'"));
    getOTStructMember(_F("dhwSetpoint"))->value.f = String(value).toFloat();
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("dhwSetpoint"), getOTStructMember(_F("dhwSetpoint"))->value.f); websocket_write_all(log_msg, strlen(log_msg));
  }
  else if (strcmp_P(topic, PSTR("relativeModulation")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'relativeModulation'"));
    getOTStructMember(_F("relativeModulation"))->value.f = String(value).toFloat();
    if ((getOTStructMember(_F("relativeModulation"))->value.f > getOTStructMember(_F("maxRelativeModulation"))->value.f) && ( getOTStructMember(_F("maxRelativeModulation"))->value.f > -99)) { //need to change the relative modulation on the fly to comply with max requested
        getOTStructMember(_F("relativeModulation"))->value.f = getOTStructMember(_F("maxRelativeModulation"))->value.f;
    }
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("relativeModulation"), getOTStructMember(_F("relativeModulation"))->value.f); websocket_write_all(log_msg, strlen(log_msg));
    rules_event_cb(_F("?"), topic);
  }  
  else if (strcmp_P(topic, PSTR("maxTSet")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'maxTSet'"));
    getOTStructMember(_F("maxTSet"))->value.f = String(value).toFloat();
    if ((getOTStructMember(_F("maxTSet"))->value.f > getOTStructMember(_F("chSetUppBound"))->value.s8)) { 
        getOTStructMember(_F("maxTSet"))->value.f = getOTStructMember(_F("chSetUppBound"))->value.s8;
    } else if ((getOTStructMember(_F("maxTSet"))->value.f < getOTStructMember(_F("chSetLowBound"))->value.s8)) { 
        getOTStructMember(_F("maxTSet"))->value.f = getOTStructMember(_F("chSetLowBound"))->value.s8;
    }           
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %.2f}}}"), _F("maxTSet"), getOTStructMember(_F("maxTSet"))->value.f); websocket_write_all(log_msg, strlen(log_msg));
    rules_event_cb(_F("?"), topic);
  }
  else if (strcmp_P(topic, PSTR("flameState")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'flameState'"));
    getOTStructMember(_F("flameState"))->value.b = ((stricmp((char*)"true", value) == 0) || (stricmp((char*)"on", value) == 0) || (String(value).toInt() == 1 ));
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %s}}}"), _F("flameState"), getOTStructMember(_F("flameState"))->value.b ? _F("true") : _F("false")); websocket_write_all(log_msg, strlen(log_msg));
  }
  else if (strcmp_P(topic, PSTR("chState")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'chState'"));
    getOTStructMember(_F("chState"))->value.b = ((stricmp((char*)"true", value) == 0) || (stricmp((char*)"on", value) == 0) || (String(value).toInt() == 1 ));
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %s}}}"), _F("chState"), getOTStructMember(_F("chState"))->value.b ? _F("true") : _F("false")); websocket_write_all(log_msg, strlen(log_msg));
  }
  else if (strcmp_P(topic, PSTR("dhwState")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'dhwState'"));
    getOTStructMember(_F("dhwState"))->value.b = ((stricmp((char*)"true", value) == 0) || (stricmp((char*)"on", value) == 0) || (String(value).toInt() == 1 ));
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %s}}}"), _F("dhwState"), getOTStructMember(_F("dhwState"))->value.b ? _F("true") : _F("false")); websocket_write_all(log_msg, strlen(log_msg));
  }
  else if (strcmp_P(topic, PSTR("coolingState")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'coolingState'"));
    getOTStructMember(_F("coolingState"))->value.b = ((stricmp((char*)"true", value) == 0) || (stricmp((char*)"on", value) == 0) || (String(value).toInt() == 1 ));
    rules_event_cb(_F("?"), topic);
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %s}}}"), _F("coolingState"), getOTStructMember(_F("coolingState"))->value.b ? _F("true") : _F("false")); websocket_write_all(log_msg, strlen(log_msg));
  }  
  
  else if (strcmp_P(topic, PSTR("dhwSetUppBound")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'dhwSetUppBound'"));
    getOTStructMember(_F("dhwSetUppBound"))->value.s8 = String(value).toInt();
    if ((getOTStructMember(_F("dhwSetUppBound"))->value.s8 < getOTStructMember(_F("dhwSetLowBound"))->value.s8)) { 
        getOTStructMember(_F("dhwSetUppBound"))->value.s8 = getOTStructMember(_F("dhwSetLowBound"))->value.s8;
    }
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %d}}}"), _F("dhwSetUppBound"), getOTStructMember(_F("dhwSetUppBound"))->value.s8); websocket_write_all(log_msg, strlen(log_msg));        
    rules_event_cb(_F("?"), topic);
  }
  else if (strcmp_P(topic, PSTR("dhwSetLowBound")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'dhwSetLowBound'"));
    getOTStructMember(_F("dhwSetLowBound"))->value.s8 = String(value).toInt();
    if ((getOTStructMember(_F("dhwSetLowBound"))->value.s8 > getOTStructMember(_F("dhwSetUppBound"))->value.s8)) { 
        getOTStructMember(_F("dhwSetLowBound"))->value.s8 = getOTStructMember(_F("dhwSetUppBound"))->value.s8;
    }
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %d}}}"), _F("dhwSetLowBound"), getOTStructMember(_F("dhwSetLowBound"))->value.s8); websocket_write_all(log_msg, strlen(log_msg));      
    rules_event_cb(_F("?"), topic);
  }   
 
  else if (strcmp_P(topic, PSTR("chSetUppBound")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'chSetUppBound'"));
    getOTStructMember(_F("chSetUppBound"))->value.s8 = String(value).toInt();
    if ((getOTStructMember(_F("chSetUppBound"))->value.s8 < getOTStructMember(_F("chSetLowBound"))->value.s8)) { 
        getOTStructMember(_F("chSetUppBound"))->value.s8 = getOTStructMember(_F("chSetLowBound"))->value.s8;
    }    
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %d}}}"), _F("chSetUppBound"), getOTStructMember(_F("chSetUppBound"))->value.s8); websocket_write_all(log_msg, strlen(log_msg));
    rules_event_cb(_F("?"), topic);
  }
  else if (strcmp_P(topic, PSTR("chSetLowBound")) == 0) {
    log_message(_F("OpenTherm: MQTT message received 'chSetLowBound'"));
    getOTStructMember(_F("chSetLowBound"))->value.s8 = String(value).toInt();
    if ((getOTStructMember(_F("chSetLowBound"))->value.s8 > getOTStructMember(_F("chSetUppBound"))->value.s8)) { 
        getOTStructMember(_F("chSetLowBound"))->value.s8 = getOTStructMember(_F("chSetUppBound"))->value.s8;
    }
    sprintf_P(log_msg, PSTR("{\"data\": {\"opentherm\": {\"name\": \"%s\", \"value\": %d}}}"), _F("chSetLowBound"), getOTStructMember(_F("chSetLowBound"))->value.s8); websocket_write_all(log_msg, strlen(log_msg));        
    rules_event_cb(_F("?"), topic);
  }
}



// find an OpenTherm value by name (case-insensitive), -1 if none
int openthermJsonFind(const char *name) {
  for (int i = 0; heishaOTDataStruct[i].name != NULL; i++) {
    if (strcasecmp(heishaOTDataStruct[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

// one "<name>":{"type": "W","value":...} pair; type is W (written by the thermostat),
// RW (both) or R (provided to the thermostat)
static void openthermJsonPair(struct webserver_t *client, int i) {
  struct heishaOTDataStruct_t *member = &heishaOTDataStruct[i];
  char value[48]; // dtostrf of a large float (from an mqtt value) can be up to 42 chars
  char buf[96];

  switch (member->type) {
    case TBOOL:
      strcpy_P(value, member->value.b ? PSTR("true") : PSTR("false"));
      break;
    case TFLOAT:
      dtostrf(member->value.f, 0, 2, value);
      break;
    case TINT8:
      itoa(member->value.s8, value, 10);
      break;
    default:
      strcpy_P(value, PSTR("null"));
      break;
  }
  // built in one buffer so each value is a single send (the webserver queues every send separately)
  int len = snprintf_P(buf, sizeof(buf), PSTR("\"%s\":{\"type\": \"%s\",\"value\":%s}"), member->name,
                       member->rw == 3 ? "W" : (member->rw == 2 ? "RW" : "R"), value);
  if (len >= (int)sizeof(buf)) {
    len = sizeof(buf) - 1;
  }
  webserver_send_content(client, buf, len);
}

// all values, in the order of heishaOTDataStruct[]
void openthermJsonOutput(struct webserver_t *client) {
  webserver_send_content_P(client, PSTR("{"), 1);
  for (int i = 0; heishaOTDataStruct[i].name != NULL; i++) {
    if (i > 0) {
      webserver_send_content_P(client, PSTR(","), 1);
    }
    openthermJsonPair(client, i);
  }
  webserver_send_content_P(client, PSTR("}"), 1);
}

// one value for /json?opentherm=<name>: {"<name>":{"type": "W","value":...}}
void openthermJsonMember(struct webserver_t *client, int i) {
  webserver_send_content_P(client, PSTR("{"), 1);
  openthermJsonPair(client, i);
  webserver_send_content_P(client, PSTR("}"), 1);
}
