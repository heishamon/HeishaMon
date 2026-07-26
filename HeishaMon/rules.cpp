/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <Arduino.h>
#include <LittleFS.h>

#include "src/common/mem.h"
#include "src/common/stricmp.h"
#include "src/common/strnicmp.h"
#include "src/common/log.h"
#include "src/common/uint32float.h"
#include "src/common/timerqueue.h"
#include "src/common/progmem.h"
#include "src/rules/rules.h"

#include "dallas.h"
#include "webfunctions.h"
#include "decode.h"
#include "HeishaOT.h"
#include "commands.h"

#define MAXCOMMANDSINBUFFER 10
#define OPTDATASIZE 20

bool send_command(byte* command, int length);
#ifdef ESP32
extern QueueHandle_t pcbQueue;
#endif

extern int dallasDevicecount;
extern dallasDataStruct *actDallasData;
extern settingsStruct heishamonSettings;
extern char actData[DATASIZE];
extern char actOptData[OPTDATASIZE];
extern char actDataExtra[DATASIZE];
extern volatile s0DataStruct actS0Data[];
extern volatile s0SettingsStruct actS0Settings[];

extern String openTherm[2];
static uint8_t parsing = 0;

static struct rules_t **rules = NULL;
uint8_t nrrules = 0;

struct rule_options_t rule_options;

typedef struct rule_timer_t {
  uint32_t first;
  uint32_t second;
} __attribute__((aligned(4))) rule_timer_t;

static struct rule_timer_t timestamp;

typedef struct array_t {
  const char *key;
  union {
    int i;
    float f;
    void *n;
    const char *s;
  } val;
  uint8_t type;
} array_t;

typedef struct varstack_t {
  struct array_t *array;
  uint16_t nr;
} varstack_t;

static struct varstack_t global_varstack = { .array = NULL, .nr = 0 };

#if defined(ESP8266)
unsigned char *mempool = (unsigned char *)MMU_SEC_HEAP;
#elif defined(ESP32)
unsigned char *mempool; //malloc in runtime
#endif
unsigned int memptr = 0;

// static int readRuleFromFS(int i) {
  // char fname[24];
  // memset(&fname, 0, sizeof(fname));
  // sprintf_P((char *)&fname, PSTR("/rule%d.bc"), i);
  // File f = LittleFS.open(fname, "r");
  // if(!f) {
    // logprintf_P(F("failed to open file: %s"), fname);
    // return -1;
  // }
  // FREE(rules[i]->ast.buffer);
  // if((rules[i]->ast.buffer = (unsigned char *)MALLOC(rules[i]->ast.bufsize)) == NULL) {
    // OUT_OF_MEMORY
  // }
  // memset(rules[i]->ast.buffer, 0, rules[i]->ast.bufsize);
  // f.readBytes((char *)rules[i]->ast.buffer, rules[i]->ast.bufsize);
  // f.close();
  // return 0;
// }

// static int writeRuleToFS(int i) {
  // char fname[24];
  // memset(&fname, 0, sizeof(fname));
  // sprintf_P((char *)&fname, PSTR("/rule%d.bc"), i);
  // File f = LittleFS.open(fname, "w");
  // if(!f) {
    // logprintf_P(F("failed to open file: %s"), fname);
    // return -1;
  // }
  // f.write((char *)rules[i]->ast.buffer, rules[i]->ast.bufsize);
  // f.close();
  // return 0;
// }

static int8_t is_variable(char *text, uint16_t size) {
  uint16_t i = 1, x = 0, match = 0;

  if(size == strlen_P(PSTR("ds18b20#2800000000000000")) && strncmp_P(text, PSTR("ds18b20#"), 8) == 0) {
    return 24;
  } else if(strncmp_P(text, PSTR("s0#"), 3) == 0) {
    if((size == strlen_P(PSTR("s0#watt_1")) && strncmp_P(&text[3], PSTR("watt_"), 5) == 0) ||
       (size == strlen_P(PSTR("s0#watt_2")) && strncmp_P(&text[3], PSTR("watt_"), 5) == 0)) {
        return size;
    }
    if((size == strlen_P(PSTR("s0#watthour_1")) && strncmp_P(&text[3], PSTR("watthour_"), 9) == 0) ||
       (size == strlen_P(PSTR("s0#watthour_2")) && strncmp_P(&text[3], PSTR("watthour_"), 9) == 0)) {
        return size;
    }
    if((size == strlen_P(PSTR("s0#watthourtotal_1")) && strncmp_P(&text[3], PSTR("watthourtotal_"), 14) == 0) ||
       (size == strlen_P(PSTR("s0#watthourtotal_2")) && strncmp_P(&text[3], PSTR("watthourtotal_"), 14) == 0)) {
        return size;
    }
  } else if(text[0] == '$' || text[0] == '#' || text[0] == '@' || text[0] == '%' || text[0] == '?') {
    while(isalnum(text[i])) {
      i++;
    }

    if(text[0] == '%') {
      if(size == 5 && strnicmp(&text[1], "hour", 4) == 0) {
        return 5;
      }
      if(size == 7 && strnicmp(&text[1], "minute", 6) == 0) {
        return 7;
      }
      if(size == 6 && strnicmp(&text[1], "month", 5) == 0) {
        return 6;
      }
      if(size == 4 && strnicmp(&text[1], "day", 3) == 0) {
        return 4;
      }
    }

    if(text[0] == '@') {
      int nrcommands = sizeof(commands)/sizeof(commands[0]);
      for(x=0;x<nrcommands;x++) {
        cmdStruct cmd;
        memcpy_P(&cmd, &commands[x], sizeof(cmd));
        size_t len = strlen(cmd.name);
        if(size-1 == len && strnicmp(&text[1], cmd.name, len) == 0) {
          i = len+1;
          match = 1;
          break;
        }
      }

      int nroptcommands = sizeof(optionalCommands)/sizeof(optionalCommands[0]);
      for(x=0;x<nroptcommands;x++) {
        optCmdStruct cmd;
        memcpy_P(&cmd, &optionalCommands[x], sizeof(cmd));
        size_t len = strlen(cmd.name);
        if(size-1 == len && strnicmp(&text[1], cmd.name, len) == 0) {
          i = len+1;
          match = 1;
          break;
        }
      }
      if(match == 0) {
        int nrtopics = sizeof(topics)/sizeof(topics[0]);
        for(x=0;x<nrtopics;x++) {
          char cpy[MAX_TOPIC_LEN];
          memcpy_P(&cpy, topics[x], MAX_TOPIC_LEN);
          size_t len = strlen(cpy);
          if(size-1 == len && strnicmp(&text[1], cpy, len) == 0) {
            i = len+1;
            match = 1;
            break;
          }
        }
      }
      if(match == 0) {
        int nrtopics = sizeof(optTopics)/sizeof(optTopics[0]);
        for(x=0;x<nrtopics;x++) {
          char cpy[MAX_TOPIC_LEN];
          memcpy_P(&cpy, optTopics[x], MAX_TOPIC_LEN);
          size_t len = strlen(cpy);
          if(size-1 == len && strnicmp(&text[1], cpy, len) == 0) {
            i = len+1;
            match = 1;
            break;
          }
        }
      }
      if(match == 0) {
        int nrtopics = sizeof(xtopics)/sizeof(xtopics[0]);
        for(x=0;x<nrtopics;x++) {
          char cpy[MAX_TOPIC_LEN];
          memcpy_P(&cpy, xtopics[x], MAX_TOPIC_LEN);
          size_t len = strlen(cpy);
          if(size-1 == len && strnicmp(&text[1], cpy, len) == 0) {
            i = len+1;
            match = 1;
            break;
          }
        }
      }
      if(match == 0) {
        return -1;
      }
    }
    if(text[0] == '?') {
      int x = 0;
      while(heishaOTDataStruct[x].name != NULL) {
        size_t len = strlen(heishaOTDataStruct[x].name);
        if(size-1 == len && strnicmp(&text[1], heishaOTDataStruct[x].name, len) == 0) {
          i = len+1;
          match = 1;
          break;
        }
        x++;
      }
      if(match == 0) {
        logprintf_P(F("err: %s %d"), __FUNCTION__, __LINE__);
        return -1;
      }
    }

    return i;
  }
  return -1;
}


static int8_t is_event(char *text, uint16_t size) {
  int i = 1, x = 0, match = 0;
  if(text[0] == '@') {
    int nrcommands = sizeof(commands)/sizeof(commands[0]);
    for(x=0;x<nrcommands;x++) {
      cmdStruct cmd;
      memcpy_P(&cmd, &commands[x], sizeof(cmd));
      size_t len = strlen(cmd.name);
      if(size-1 == len && strnicmp(&text[1], cmd.name, len) == 0) {
        i = len+1;
        match = 1;
        break;
      }
    }

    if(match == 0) {
      int nroptcommands = sizeof(optionalCommands)/sizeof(optionalCommands[0]);
      for(x=0;x<nroptcommands;x++) {
        optCmdStruct cmd;
        memcpy_P(&cmd, &optionalCommands[x], sizeof(cmd));
        size_t len = strlen(cmd.name);
        if(size-1 == len && strnicmp(&text[1], cmd.name, len) == 0) {
          i = len+1;
          match = 1;
          break;
        }
      }
    }
    if(match == 0) {
      int nrtopics = sizeof(topics)/sizeof(topics[0]);
      for(x=0;x<nrtopics;x++) {
        size_t len = strlen_P(topics[x]);
        char cpy[len];
        memcpy_P(&cpy, &topics[x], len);
        if(size-1 == len && strnicmp(&text[1], cpy, len) == 0) {
          i = len+1;
          match = 1;
          break;
        }
      }
    }
    if(match == 0) {
      int nrtopics = sizeof(optTopics)/sizeof(optTopics[0]);
      for(x=0;x<nrtopics;x++) {
        size_t len = strlen_P(optTopics[x]);
        char cpy[len];
        memcpy_P(&cpy, &optTopics[x], len);
        if(size-1 == len && strnicmp(&text[1], cpy, len) == 0) {
          i = len+1;
          match = 1;
          break;
        }
      }
    }
    if(match == 0) {
      int nrtopics = sizeof(xtopics)/sizeof(xtopics[0]);
      for(x=0;x<nrtopics;x++) {
        size_t len = strlen_P(xtopics[x]);
        char cpy[len];
        memcpy_P(&cpy, &xtopics[x], len);
        if(size-1 == len && strnicmp(&text[1], cpy, len) == 0) {
          i = len+1;
          match = 1;
          break;
        }
      }
    }
    if(match == 0) {
      return -1;
    }

    return i;
  }

  if(text[0] == '?') {
    int x = 0;
    while(heishaOTDataStruct[x].name != NULL) {
      size_t len = strlen(heishaOTDataStruct[x].name);
      if(size-1 == len && strnicmp(&text[1], heishaOTDataStruct[x].name, len) == 0) {
        i = len+1;
        match = 1;
        break;
      }
      x++;
    }
    if(match == 0) {
      return -1;
    }
    return i;
  }

  if(size == strlen_P(PSTR("ds18b20#2800000000000000")) && strncmp_P((const char *)text, PSTR("ds18b20#"), 8) == 0) {
    return 24;
  }

if(strncmp(text, "s0#", 3) == 0 && (size == 9 || size == 13 || size == 18)) {
    char port = text[size - 1];  // last char is the port digit
    if(port != '1' && port != '2') return -1;
    if(size == 9  && strnicmp(&text[3], "watt_", 5) == 0)          return size;
    if(size == 13 && strnicmp(&text[3], "watthour_", 9) == 0)      return size;
    if(size == 18 && strnicmp(&text[3], "watthourtotal_", 14) == 0) return size;
}

// Custom event. Make sure it is in a valid format
  if(isalpha(text[0]) || text[0] == '_') {
    uint16_t i = 0;
    while(i < size && (isalnum(text[i]) || text[i] == '_' || text[i] == '#')) {
      i++;
    }
    if(i == size) {
      return size;  // Valid identifier - accept it
    }
  }

  return -1;
}

static void rule_done_cb(struct rules_t *obj) {
  return;
}

static int8_t event_cb(struct rules_t *obj, char *name) {
  int8_t nr = rule_by_name(rules, nrrules, name);
  if(nr == -1) {
    char msg[100];
    sprintf_P((char *)&msg, PSTR("Rule block '%s' not found"), name);
    log_message(msg);
    return -1;
  }

  obj->ctx.go = rules[nr];
  rules[nr]->ctx.ret = obj;

  return 1;
}

static int8_t check_is_number(const char *str) {
  uint16_t pos = 0, nrdot = 0;
  size_t len = strlen(str);

  if (len == 0) {
    return -1;  // Empty string is not a number.
  }

  // Check the first character (optional '-' or a digit).
  if (str[0] != '-' && !isdigit((unsigned char)str[0])) {
    return -1;
  }

  // Traverse the rest of the string.
  for (pos = 1; pos < len; pos++) {
    char current = str[pos];

    if (current == '.') {
      nrdot++;
      if (nrdot > 1) {
        return -1;  // More than one dot.
      }
    } else if (!isdigit((unsigned char)current)) {
      return -1;  // Non-digit, non-dot character.
    }
  }

  // Valid number if all checks pass.
  return 0;
}

static int8_t vm_value_get(struct rules_t *obj) {
  int16_t x = 0;

  if(rules_gettop() < 1) {
    return -1;
  }
  if(rules_type(-1) != VCHAR) {
    return -1;
  }

  const char *key = rules_tostring(-1);

  if(key[0] == '?') {
    int x = 0;
    while(heishaOTDataStruct[x].name != NULL) {
      if(heishaOTDataStruct[x].rw >= 2 &&
         stricmp((char *)&key[1], heishaOTDataStruct[x].name) == 0) {
        if(heishaOTDataStruct[x].type == TBOOL) {
          rules_pushinteger((int)heishaOTDataStruct[x].value.b);
          return 0;
        }
        if(heishaOTDataStruct[x].type == TFLOAT) {
          rules_pushfloat(heishaOTDataStruct[x].value.f);
          return 0;
        }
        break;
      }
      x++;
    }
    logprintf_P(F("err: %s %d"), __FUNCTION__, __LINE__);
  } else if(key[0] == '%') {
    time_t now = time(NULL);
    struct tm *tm_struct = localtime(&now);
    if(stricmp((char *)&key[1], "hour") == 0) {
      rules_pushinteger((int)tm_struct->tm_hour);
      return 0;
    } else if(stricmp((char *)&key[1], "minute") == 0) {
      rules_pushinteger((int)tm_struct->tm_min);
      return 0;
    } else if(stricmp((char *)&key[1], "month") == 0) {
      rules_pushinteger((int)tm_struct->tm_mon);
      return 0;
    } else if(stricmp((char *)&key[1], "day") == 0) {
      rules_pushinteger((int)tm_struct->tm_wday+1);
      return 0;
    }
  } else if(strnicmp((const char *)key, _F("ds18b20#"), 8) == 0) {
    uint8_t i = 0;
    for(i=0;i<dallasDevicecount;i++) {
      if(strncmp(actDallasData[i].address, (const char *)&key[8], 16) == 0) {
        rules_pushfloat(actDallasData[i].temperature);
        return 0;
      }
    }
    rules_pushnil();
    return 0;
} else if(strnicmp((const char *)key, "s0#", 3) == 0) {
    // Port digit is last character; valid sizes are 9, 13, 18
    size_t klen = strlen(key);
    if(klen != 9 && klen != 13 && klen != 18) { rules_pushnil(); return 0; }

    char portChar = key[klen - 1];
    if(portChar != '1' && portChar != '2') { rules_pushnil(); return 0; }
    uint8_t idx = portChar - '1';  // '1'->0, '2'->1

    if(klen == 9 && strnicmp(&key[3], "watt_", 5) == 0) {

        rules_pushfloat((float)actS0Data[idx].watt);
        return 0;
    }
    if(klen == 13 && strnicmp(&key[3], "watthour_", 9) == 0) {
        rules_pushfloat(actS0Data[idx].pulses * (1000.0 / actS0Settings[idx].ppkwh));
        return 0;
    }
    if(klen == 18 && strnicmp(&key[3], "watthourtotal_", 14) == 0) {
        rules_pushfloat(actS0Data[idx].pulsesTotal * (1000.0 / actS0Settings[idx].ppkwh));
        return 0;
    }
    rules_pushnil();
    return 0;
  } else if(key[0] == '@') {
    uint16_t i = 0;
    for(i=0;i<NUMBER_OF_TOPICS;i++) {
      char cpy[MAX_TOPIC_LEN];
      memcpy_P(&cpy, topics[i], MAX_TOPIC_LEN);
      if(stricmp(cpy, (char *)&key[1]) == 0) {
        String dataValue = actData[0] == '\0' ? "" : getDataValue(actData, i);
        char *str = (char *)dataValue.c_str();
        if(strlen(str) == 0) {
          rules_pushnil();
          return 0;
        } else if(check_is_number(str) == 0) {
          float var = atof(str);
          float nr = 0;

          if(modff(var, &nr) == 0) {
            rules_pushinteger((int)var);
            return 0;
          } else {
            rules_pushfloat(var);
            return 0;
          }
        } else {
          rules_pushstring(str);
          return 0;
        }
      }
    }
    for(i=0;i<NUMBER_OF_OPT_TOPICS;i++) {
      char cpy[MAX_TOPIC_LEN];
      memcpy_P(&cpy, topics[i], MAX_TOPIC_LEN);
      if(stricmp(cpy, (char *)&key[1]) == 0) {
        String dataValue = actOptData[0] == '\0' ? "" : getOptDataValue(actOptData, i);
        char *str = (char *)dataValue.c_str();
        if(strlen(str) == 0) {
          rules_pushnil();
          return 0;
        } else if(check_is_number(str) == 0) {
          float var = atof(str);
          float nr = 0;

          if(modff(var, &nr) == 0) {
            rules_pushinteger((int)var);
            return 0;
          } else {
            rules_pushfloat(var);
            return 0;
          }
        } else {
          rules_pushstring(str);
          return 0;
        }
      }
    }
    for(i=0;i<NUMBER_OF_TOPICS_EXTRA;i++) {
      char cpy[MAX_TOPIC_LEN];
      memcpy_P(&cpy, xtopics[i], MAX_TOPIC_LEN);
      if(stricmp(cpy, (char *)&key[1]) == 0) {
        String dataValue = actDataExtra[0] == '\0' ? "" : getDataValueExtra(actDataExtra, i);
        char *str = (char *)dataValue.c_str();
        if(strlen(str) == 0) {
          rules_pushnil();
          return 0;
        } else if(check_is_number(str) == 0) {
          float var = atof(str);
          float nr = 0;

          if(modff(var, &nr) == 0) {
            rules_pushinteger((int)var);
            return 0;
          } else {
            rules_pushfloat(var);
            return 0;
          }
        } else {
          rules_pushstring(str);
          return 0;
        }
      }
    }
  } else {
    struct varstack_t *table = NULL;
    struct array_t *array = NULL;
    if(key[0] == '$') {
      table = (struct varstack_t *)obj->userdata;
    } else if(key[0] == '#') {
      table = &global_varstack;
    }
    if(table == NULL) {
      rules_pushnil();
      return 0;
    } else {
      for(x=0;x<table->nr;x++) {
        if(strcmp(table->array[x].key, key) == 0) {
          array = &table->array[x];
          break;
        }
      }
      if(array == NULL) {
        rules_pushnil();
        return 0;
      } else {
        switch(array->type) {
          case VINTEGER: {
            rules_pushinteger(array->val.i);
            return 0;
          } break;
          case VFLOAT: {
            rules_pushfloat(array->val.f);
            return 0;
          } break;
          case VCHAR: {
            rules_pushstring((char *)array->val.s);
            return 0;
          } break;
          case VNULL: {
            rules_pushnil();
            return 0;
          } break;
        }
      }
    }
  }

  return 0;
}

static int8_t vm_value_set(struct rules_t *obj) {
  struct varstack_t *table = NULL;
  uint16_t x = 0;
  uint8_t type = 0;

  if(rules_gettop() < 2) {
    return -1;
  }
  type = rules_type(-1);

  if(rules_type(-2) != VCHAR
    || (type != VINTEGER && type != VFLOAT && type != VNULL && type != VCHAR)) {
    return -1;
  }

  const char *key = rules_tostring(-2);

  if(key[0] == '@') {
    char *payload = NULL;
    unsigned int len = 0;

    switch(type) {
      case VCHAR: {
        len = snprintf_P(NULL, 0, PSTR("%s"), rules_tostring(-1));
        if((payload = (char *)MALLOC(len+1)) == NULL) {
          OUT_OF_MEMORY
        }
        snprintf_P(payload, len+1, PSTR("%s"), rules_tostring(-1));
      } break;
      case VINTEGER: {
        int val = rules_tointeger(-1);
        len = snprintf_P(NULL, 0, PSTR("%d"), val);
        if((payload = (char *)MALLOC(len+1)) == NULL) {
          OUT_OF_MEMORY
        }
        snprintf_P(payload, len+1, PSTR("%d"), val);
      } break;
      case VFLOAT: {
        float val = rules_tofloat(-1);
        len = snprintf_P(NULL, 0, PSTR("%g"), val);
        if((payload = (char *)MALLOC(len+1)) == NULL) {
          OUT_OF_MEMORY
        }
        snprintf_P(payload, len+1, PSTR("%g"), val);
      } break;
    }

    if(parsing == 0 && !heishamonSettings.listenonly) {
      unsigned char cmd[256] = { 0 };
      char log_msg[256] = { 0 };

      for(uint8_t x = 0; x < sizeof(commands) / sizeof(commands[0]); x++) {
        cmdStruct tmp;
        memcpy_P(&tmp, &commands[x], sizeof(tmp));
        if(stricmp((char *)&key[1], tmp.name) == 0) {
          uint16_t len = tmp.func(payload, cmd, log_msg);
          log_message(log_msg);
          send_command(cmd, len);
          break;
        }
      }

      memset(&cmd, 0, sizeof(cmd));
      memset(&log_msg, 0, sizeof(log_msg));

      if(heishamonSettings.optionalPCB) {
        //optional commands
        for(uint8_t x = 0; x < sizeof(optionalCommands) / sizeof(optionalCommands[0]); x++) {
          optCmdStruct tmp;
          memcpy_P(&tmp, &optionalCommands[x], sizeof(tmp));
          if(stricmp((char *)&key[1], tmp.name) == 0) {
            uint16_t len = tmp.func(payload, log_msg);
            log_message(log_msg);
#ifdef ESP32
            xQueueOverwrite(pcbQueue, optionalPCBQuery);
#endif
            break;
          }
        }
      }
    }
    FREE(payload);
  } else if(key[0] == '?') {
    int x = 0;
    while(heishaOTDataStruct[x].name != NULL) {
      if(heishaOTDataStruct[x].rw <= 2 && stricmp((char *)&key[1], heishaOTDataStruct[x].name) == 0) {
        if(heishaOTDataStruct[x].type == TBOOL) {
          switch(type) {
            case VINTEGER: {
              heishaOTDataStruct[x].value.b = (bool)rules_tointeger(-1);
            } break;
            case VFLOAT: {
              heishaOTDataStruct[x].value.b = (bool)rules_tofloat(-1);
            } break;
          }
        } else if(heishaOTDataStruct[x].type == TFLOAT) {
          switch(type) {
            case VINTEGER: {
              heishaOTDataStruct[x].value.f = (float)rules_tointeger(-1);
            } break;
            case VFLOAT: {
              heishaOTDataStruct[x].value.f = rules_tofloat(-1);
            } break;
          }
        }
        break;
      }
      x++;
    }
  } else {
    if(key[0] == '$') {
      table = (struct varstack_t *)obj->userdata;
      if(table == NULL) {
        if((table = (struct varstack_t *)MALLOC(sizeof(struct varstack_t))) == NULL) {
          OUT_OF_MEMORY
        }
        memset(table, 0, sizeof(struct varstack_t));
        obj->userdata = table;
      }
    } else if(key[0] == '#') {
      table = (struct varstack_t *)&global_varstack;
    }

    struct array_t *array = NULL;
    for(x=0;x<table->nr;x++) {
      if(strcmp(table->array[x].key, key) == 0) {
        array = &table->array[x];
        break;
      }
    }

    if(array == NULL) {
      if((table->array = (struct array_t *)REALLOC(table->array, sizeof(struct array_t)*(table->nr+1))) == NULL) {
        OUT_OF_MEMORY
      }
      array = &table->array[table->nr];
      memset(array, 0, sizeof(struct array_t));
      table->nr++;
      rules_ref(key);
    }

    array->key = key;

    switch(type) {
      case VINTEGER: {
        if(array->type == VCHAR && array->val.s != NULL) {
          rules_unref(array->val.s);
        }
        array->val.i = rules_tointeger(-1);
        array->type = VINTEGER;
      } break;
      case VFLOAT: {
        if(array->type == VCHAR && array->val.s != NULL) {
          rules_unref(array->val.s);
        }
        array->val.f = rules_tofloat(-1);
        array->type = VFLOAT;
      } break;
      case VCHAR: {
        uint8_t doref = 1;
        if(array->type == VCHAR && array->val.s != NULL) {
          if(strcmp(rules_tostring(-1), array->val.s) != 0) {
            rules_unref(array->val.s);
          } else {
            doref = 0;
          }
        }
        array->val.s = rules_tostring(-1);
        array->type = VCHAR;
        if(doref == 1) {
          rules_ref(array->val.s);
        }
      } break;
      case VNULL: {
        if(array->type == VCHAR && array->val.s != NULL) {
          rules_unref(array->val.s);
        }
        array->val.n = NULL;
        array->type = VNULL;
      } break;
    }
  }
  return 0;
}

static void rules_free_stack(void) {
  int x = 0;
  for(x=0;x<nrrules;x++) {
    struct varstack_t *node = (struct varstack_t *)rules[x]->userdata;
    if(node != NULL) {
      FREE(node->array);
      FREE(node);
    }
    rules[x]->userdata = NULL;
  }
}

static void rules_print_stack(struct varstack_t *table) {
  struct array_t *array = NULL;
  if(table == NULL) {
    return;
  } else {
    uint16_t x = 0;
    for(x=0;x<table->nr;x++) {
      array = &table->array[x];
      switch(array->type) {
        case VINTEGER: {
#if defined(ESP8266) || defined(ESP32)
          logprintf_P(F("%2d %s = %d"), x, array->key, array->val.i);
#else
          printf("%2d %s = %d\n", x, array->key, array->val.i);
#endif
        } break;
        case VFLOAT: {
#if defined(ESP8266) || defined(ESP32)
          logprintf_P(F("%2d %s = %g"), x, array->key, array->val.f);
#else
          printf("%2d %s = %g\n", x, array->key, array->val.f);
#endif
        } break;
        case VCHAR: {
#if defined(ESP8266) || defined(ESP32)
          logprintf_P(F("%2d %s = %s"), x, array->key, array->val.s);
#else
          printf("%2d %s = %s\n", x, array->key, array->val.s);
#endif
        } break;
        case VNULL: {
#if defined(ESP8266) || defined(ESP32)
          logprintf_P(F("%d %s = NULL"), x, array->key);
#else
          printf("%2d %s = NULL\n", x, array->key);
#endif
        } break;
      }
    }
  }
}

static void rules_print_local_stacks(void) {
  uint8_t x = 0;
  for(x = 0; x < nrrules; x++) {
    struct varstack_t *node = (struct varstack_t *)rules[x]->userdata;
    if(node != NULL && node->nr > 0) {
      logprintf_P(F("\n>>> local variables (%s)\n"), rules[x]->name);
      rules_print_stack(node);
    }
  }
}

void rules_timer_cb(int nr) {
  char *name = NULL;
  int x = 0, i = 0;

  i = snprintf_P(NULL, 0, PSTR("timer=%d"), nr);
  if((name = (char *)MALLOC(i+2)) == NULL) {
    OUT_OF_MEMORY
  }
  memset(name, 0, i+2);
  snprintf_P(name, i+1, PSTR("timer=%d"), nr);

  // logprintf_P(F("_______ %s %s"), __FUNCTION__, name);

  nr = rule_by_name(rules, nrrules, name);
  if(nr > -1) {
    logprintf_P(F("%s %s %s"), F("===="), name, F("===="));

    timestamp.first = micros();

    int ret = rule_run(rules[nr], 0);

    timestamp.second = micros();

    if(ret == 0) {
      logprintf_P(F("%s%d %s %d %s"), F("rule #"), rules[nr]->nr, F("was executed in"), timestamp.second - timestamp.first, F("microseconds"));

      rules_print_local_stacks();
      logprintf_P(F("\n>>> global variables\n"));
      rules_print_stack(&global_varstack);
      rules_free_stack();
    }
  }
  FREE(name);
}

void rules_setup(void) {
  if(!LittleFS.begin()) {
    return;
  }
    if (rule_options.event_cb == NULL) { //check if not initialized before
#ifdef ESP32
      if (mempool == NULL) { //make sure we only malloc if not done before
        mempool = (unsigned char *)ps_malloc(MEMPOOL_SIZE);
        if (mempool == NULL) {
          logprintln_P(F("Mempool OOM, rules disabled"));
          return;
        }
      }
#endif
    if (mempool == NULL) {
      logprintln_P(F("Mempool unavailable, rules disabled"));
      return;
    }
    memset(mempool, 0, MEMPOOL_SIZE);

    logprintf_P(F("rules mempool size: %d"), MEMPOOL_SIZE);

    logprintln_P(F("reading rules"));

    memset(&rule_options, 0, sizeof(struct rule_options_t));
    rule_options.is_variable_cb = is_variable;
    rule_options.is_event_cb = is_event;
    rule_options.done_cb = rule_done_cb;
    rule_options.vm_value_set = vm_value_set;
    rule_options.vm_value_get = vm_value_get;
    rule_options.event_cb = event_cb;

  }
}

bool existsRulesFile(char *file) {
  if (LittleFS.begin() && (LittleFS.exists(file))) {
    File f = LittleFS.open(file, "r");
    if ((f) && (f.size() > 0)) {
      f.close();
      return true; //only return true if file exists and isn't empty
    }
  }
  return false;
}

int rules_parse(char *file) {
  if (existsRulesFile(file)) { //only parse an existing and not empty, file
    rules_setup(); //check there if done already
    if (mempool == NULL) {
      logprintln_P(F("Rules parser unavailable: mempool not initialized"));
      return -1;
    }

    File frules = LittleFS.open(file, "r");
    parsing = 1;

    if(nrrules > 0) {
      rules_free_stack();
      rules_gc(&rules, &nrrules);

      struct varstack_t *table = (struct varstack_t *)&global_varstack;
      if(table->array != NULL) {
        FREE(table->array);
      }
      table->nr = 0;
    }
    memset(mempool, 0, MEMPOOL_SIZE);

#define BUFFER_SIZE 128
    char content[BUFFER_SIZE];
    memset(content, 0, BUFFER_SIZE);
    int len = frules.size();
    int chunk = 0, len1 = 0;

    unsigned int txtoffset = alignedbuffer(MEMPOOL_SIZE-len-5);
	
    while(chunk*BUFFER_SIZE < len) {
      memset(content, 0, BUFFER_SIZE);
      frules.seek(chunk*BUFFER_SIZE, SeekSet);
      len1 = frules.readBytes(content, BUFFER_SIZE);
      memcpy(&mempool[txtoffset+(chunk*BUFFER_SIZE)], &content, alignedbuffer(len1));
      chunk++;
    }
    frules.close();

    struct pbuf mem;
    struct pbuf input;
    memset(&mem, 0, sizeof(struct pbuf));
    memset(&input, 0, sizeof(struct pbuf));

    mem.payload = mempool;
    mem.len = 0;
    mem.tot_len = MEMPOOL_SIZE;

    input.payload = &mempool[txtoffset];
    input.len = txtoffset;
    input.tot_len = len;

    int ret = 0;
    while((ret = rule_initialize(&input, &rules, &nrrules, &mem, NULL)) == 0) {
      input.payload = &mempool[input.len];
    }

    logprintf_P(F("rules memory used: %d / %d"), mem.len, mem.tot_len);

    /*
     * Clear all timers
     */
    struct timerqueue_t *node = NULL;
    while((node = timerqueue_pop()) != NULL) {
      FREE(node);
    }

    struct varstack_t *table = (struct varstack_t *)&global_varstack;
    if(table->array != NULL) {
      FREE(table->array);
    }
    table->nr = 0;

    if(ret == -1) {
      if(nrrules > 0) {
        rules_free_stack();
        rules_gc(&rules, &nrrules);
      }
      return -1;
    }

    parsing = 0;
    return 0;
  } else {
	  return -2; //empty file or not existing
  }
}

void rules_event_cb(const char *prefix, const char *name) {
  uint8_t len = strlen(name), len1 = strlen(prefix), tlen = 0;
  char buf[100] = { '\0' };
  snprintf_P((char *)&buf, sizeof(buf), PSTR("%s%s"), prefix, name);
  int8_t nr = rule_by_name(rules, nrrules, (char *)buf);
  if(nr > -1) {
    logprintf_P(F("%s %s %s"), F("===="), name, F("===="));

    timestamp.first = micros();

    int ret = rule_run(rules[nr], 0);

    timestamp.second = micros();

    if(ret == 0) {
      logprintf_P(F("%s%d %s %d %s"), F("rule #"), rules[nr]->nr, F("was executed in"), timestamp.second - timestamp.first, F("microseconds"));

      rules_print_local_stacks();
      logprintf_P(F("\n>>> global variables\n"));
      rules_print_stack(&global_varstack);
      rules_free_stack();
    }

    return;
  }
}

void rules_boot(void) {
  int8_t nr = rule_by_name(rules, nrrules, (char *)"System#Boot");
  if(nr > -1) {
    logprintf_P(F("%s %s %s"), F("===="), F("System#Boot"), F("===="));

    timestamp.first = micros();

    int ret = rule_run(rules[nr], 0);

    timestamp.second = micros();

    if(ret == 0) {
      logprintf_P(F("%s%d %s %d %s"), F("rule #"), rules[nr]->nr, F("was executed in"), timestamp.second - timestamp.first, F("microseconds"));

      rules_print_local_stacks();
      logprintf_P(F("\n>>> global variables\n"));
      rules_print_stack(&global_varstack);
      rules_free_stack();
    }
  }
}

void rules_deinitialize() {
  if (rule_options.event_cb != NULL) { 
    logprintln_P(F("Deinitialize rules engine..."));
#ifdef ESP32
    FREE(mempool);
#endif 
    if(nrrules > 0) {
      for(int i=0;i<nrrules;i++) {
        if(rules[i]->userdata != NULL) {
          FREE(rules[i]->userdata);
        }
      }
      rules_gc(&rules, &nrrules);
	  rules_free_stack();
    }


    // set this to NULL so a new initialize can start if necessary. 
    rule_options.event_cb = NULL;
  }
}

