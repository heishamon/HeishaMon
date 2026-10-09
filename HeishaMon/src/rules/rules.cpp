/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#if defined(ESP8266) || defined(ESP32)
  #pragma GCC diagnostic warning "-fpermissive"
#endif

#if !defined(ESP8266) && !defined(ESP32)
  #include <stdio.h>
  #include <stdlib.h>
  #include <stdarg.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <limits.h>
  #include <errno.h>
  #include <time.h>
  #include <sys/time.h>
  #include <math.h>
  #include <string.h>
  #include <ctype.h>
  #include <stdint.h>
#else
  #include <Arduino.h>
#endif
#undef NDEBUG
#include <assert.h>

#include "../common/mem.h"
#include "../common/log.h"
#include "../common/uint32float.h"
#include "../common/strnicmp.h"
#include "rules.h"
#include "operator.h"
#include "function.h"

#define EPSILON 0.000001f
#define JMPSIZE 37

#if (!defined(NON32XFER_HANDLER) && defined(MMU_SEC_HEAP)) || defined(COVERALLS)
  #define getval(a) \
    (((void *)(&a) >= (void *)MMU_SEC_HEAP) ? \
      (sizeof(a) == 1) ? mmu_get_uint8((uint8_t *)&a) : \
        (sizeof(a) == 2) ? mmu_get_uint16((uint16_t *)&a) : a : a)

  #define setval(a, b) \
    (((void *)&a >= (void *)MMU_SEC_HEAP) ? \
      (sizeof(a) == 1) ? mmu_set_uint8((uint8_t *)&a, b) : \
        (sizeof(a) == 2) ? mmu_set_uint16((uint16_t *)&a, b) : a = b : a = b)
#else
  #define getval(a) a
  #define setval(a, b) a = b
#endif

#define is_math(a) (a >= 9 && a <= 14)
#define is_op_and_math(a) (a >= 1 && a <= 14)
#define rule_max_var_bytes() 4
#define gettype(a) (getval(a) & 0x1F)
#define get_group(a) ((getval(a) & 0xE0) >> 5)
#define set_group(a, b) (setval(a, gettype(a) | (b << 5)))

typedef struct vm_top_t {
  uint8_t type;
  int8_t a;
  int8_t b;
  int8_t c;
} __attribute__((aligned(4))) vm_top_t;

typedef struct vm_vchar_t {
  uint8_t type;
  uint8_t fixed;
  uint8_t len;
  uint8_t ref;
  char *value;
#if defined(ESP8266) || defined(ESP32)
} __attribute__((packed, aligned(4))) vm_vchar_t;
#else
} __attribute__((aligned(4))) vm_vchar_t;
#endif

typedef struct vm_vptr_t {
  uint8_t type;
  uint16_t value;
} __attribute__((aligned(4))) vm_vptr_t;

typedef struct vm_vnull_t {
  uint8_t type;
} __attribute__((aligned(4))) vm_vnull_t;

typedef struct vm_vinteger_t {
  uint8_t type;
  uint8_t value[3];
} __attribute__((aligned(4))) vm_vinteger_t;

typedef struct vm_vfloat_t {
  uint8_t type;
  uint8_t value[3];
} __attribute__((aligned(4))) vm_vfloat_t;

static void *jmptbl[JMPSIZE] = { NULL };

/*
 * This additional category is needed to seperate
 * the event that is used to define a new function
 * block and when calling an event.
 */
#define TCEVENT 30

#if defined(DEBUG) || defined(COVERALLS)
uint16_t memused = 0;
#endif

#ifdef DEBUG
struct {
  const char *name;
} token_names[] = {
  "",
  "TOPERATOR",
  "TFUNCTION",
  "TSTRING",
  "TNUMBER",
  "TNUMBER1",
  "TNUMBER2",
  "TNUMBER3",
  "TEOF",
  "LPAREN",
  "RPAREN",
  "TCOMMA",
  "TIF",
  "TELSE",
  "TELSEIF",
  "TTHEN",
  "TEVENT",
  "TEND",
  "TVAR",
  "TASSIGN",
  "TSEMICOLON",
  "TTRUE",
  "TFALSE",
  "TSTART",
  "TVALUE",
  "VCHAR",
  "VPTR",
  "VINTEGER",
  "VFLOAT",
  "VNULL",
  "TCEVENT"
};

struct {
  const char *name;
} op_names[] = {
  "",
  "OP_EQ",
  "OP_NE",
  "OP_LT",
  "OP_LE",
  "OP_GT",
  "OP_GE",
  "OP_AND",
  "OP_OR",
  "OP_SUB",
  "OP_ADD",
  "OP_DIV",
  "OP_MUL",
  "OP_POW",
  "OP_MOD",
  "OP_TEST",
  "OP_JMP",
  "OP_SETVAL",
  "OP_GETVAL",
  "OP_PUSH",
  "OP_CALL",
  "OP_CLEAR",
  "OP_RET"
};
#endif

/*LCOV_EXCL_START*/
#ifdef DEBUG
static void print_heap(struct rules_t *obj);
static void print_stack(struct rules_t *obj);
static void print_varstack(void);
static void print_bytecode(struct rules_t *obj);
#endif
/*LCOV_EXCL_STOP*/

#if !defined(ESP8266)
/*LCOV_EXCL_START*/
uint8_t mmu_set_uint8(void *ptr, uint8_t src) { *(uint8_t *)ptr = src; return src; }
uint8_t mmu_get_uint8(void *ptr) { return *(uint8_t *)ptr; }
uint16_t mmu_set_uint16(void *ptr, uint16_t src) { *(uint16_t *)ptr = src; return src; }
uint16_t mmu_get_uint16(void *ptr) { return (*(uint16_t *)ptr); }
/*LCOV_EXCL_STOP*/
#endif

typedef struct rule_timer_t {
#if defined(ESP8266) || defined(ESP32)
  uint32_t first;
  uint32_t second;
#else
  struct timespec first;
  struct timespec second;
#endif
} __attribute__((aligned(4))) rule_timer_t;

static struct rule_stack_t *varstack = NULL;
static struct rule_stack_t *stack = NULL;
static struct rule_timer_t timestamp;

static uint8_t group = 1;

// static uint32_t align(uint32_t p, uint8_t b) {
  // return (p + b) - ((p + b) % b);
// }

// Veltkamp-Dekker algorithm
static float float32to27(float f) {
  uint8_t bits = 5; // remove 8 bits
  float factor = (float)((1u << bits) + 1u);
  float c = factor * f;
  return (c-(c-f));
}

const char *rule_by_nr(struct rules_t **rules, uint8_t nrrules, uint8_t nr) {
  if(nr >= nrrules) {
    return NULL;
  }
  struct rules_t *obj = rules[nr];

  return obj->name;
}

int8_t rule_by_name(struct rules_t **rules, uint8_t nrrules, char *name) {
  uint8_t a = 0;

  for(a=0;a<nrrules;a++) {
    struct rules_t *obj = rules[a];
    if(obj->name != NULL &&
       strlen(name) == strlen(obj->name) &&
       strnicmp(name, obj->name, strlen(obj->name)) == 0) {
      return a;
    }
  }

  return -1;
}

static int32_t is_function(char *text, uint16_t *pos, uint16_t size) {
  uint16_t i = 0, len = 0;
  for(i=0;i<nr_rule_functions;i++) {
    len = strlen(rule_functions[i].name);
    if(size == len) {
      uint16_t x = 0;
      for(x=0;x<len;x++) {
        char cpy = getval(text[*pos+x]);
        if(tolower((unsigned char)cpy) != tolower((unsigned char)rule_functions[i].name[x])) {
          break;
        }
      }
      if(x == len) {
        return i;
      }
    }
  }

  return -1;
}

static int32_t is_operator(char *text, uint16_t *pos, uint16_t size) {
  uint16_t i = 0, len = 0;
  for(i=0;i<nr_rule_operators;i++) {
    len = strlen(rule_operators[i].name);
    if(size == len) {
      uint16_t x = 0;
      for(x=0;x<len;x++) {
        char cpy = getval(text[*pos+x]);
        if(tolower((unsigned char)cpy) != tolower((unsigned char)rule_operators[i].name[x])) {
          break;
        }
      }
      if(x == len) {
        return i;
      }
    }
  }

  return -1;
}

static int8_t lexer_parse_number(char *text, uint16_t len, uint16_t *pos) {
  uint16_t i = 0, nrdot = 0;

  if(*pos >= len) {
    return -1;
  }

  char current = getval(text[*pos]);

  if(isdigit((unsigned char)current) || current == '-') {
    /*
     * The dot cannot be the first character
     * and we cannot have more than 1 dot
     */
    while(*pos < len &&
        (
          isdigit((unsigned char)current) ||
          (i == 0 && current == '-') ||
          (i > 0 && nrdot == 0 && current == '.')
        )
      ) {
      if(current == '.') {
        nrdot++;
      }
      (*pos)++;
      current = getval(text[*pos]);
      i++;
    }

    return 0;
  } else {
    return -1;
  }
}

static uint16_t lexer_parse_string(char *text, uint16_t len, uint16_t *pos) {
  if(*pos >= len) {
    return 0;
  }

  char current = getval(text[*pos]);

  while(*pos < len &&
      (current != ' ' &&
      current != ',' &&
      current != ';' &&
      current != '(' &&
      current != ')')) {
    (*pos)++;
    if(*pos >= len) {
      break;
    }
    current = getval(text[*pos]);
  }

  return 0;
}

static int8_t lexer_parse_quoted_string(char *text, uint16_t len, uint16_t *pos) {
  uint8_t start = 0;
  unsigned char current = 0;

  if(*pos >= len) {
    return -1;
  }

  current = getval(text[*pos]);

  while(*pos < len) {
    if((current < 9 || current > 10) && (current < 32 || current > 127)) {
      return -2;
    } else if(current == '\\') {
      if((*pos + 1) >= len) {
        return -1;
      }
      if(getval(text[(*pos+1)]) == start) {
        (*pos)+=2;
        if(*pos >= len) {
          break;
        }
        current = getval(text[*pos]);
        continue;
      }
    } else if(start == current) {
      (*pos)--;
      break;
    }
    if(current == '"' || current == '\'') {
      start = current;
    }
    ++(*pos);
    current = getval(text[(*pos)]);
  }
  if(*pos == len) {
    return -1;
  }

  return 0;
}

static int8_t lexer_parse_skip_characters(char *text, uint16_t len, uint16_t *pos) {
  if(*pos >= len) {
    return 0;
  }

  char current = getval(text[*pos]);

  while(*pos < len &&
      (current == ' ' ||
      current == '\n' ||
      current == '\t' ||
      current == '\r')) {

    (*pos)++;

    if(*pos >= len) {
      break;
    }
    current = getval(text[*pos]);
  }

  return 0;
}

static int16_t lexer_peek(char **text, uint16_t skip, uint8_t *type, uint16_t *start, uint16_t *len) {
  uint16_t i = 0, nr = 0;
  uint8_t loop = 1;

  while(loop) {
#ifdef ESP8266
    ESP.wdtFeed();;
#endif
    *type = getval((*text)[i]);
    *start = i;
    *len = 0;
    switch(*type) {
      case TELSEIF:
      case TIF:{
        i += 2;
        *len = 2;
      } break;
      case VNULL:
      case TSEMICOLON:
      case TEND:
      case TASSIGN:
      case RPAREN:
      case TCOMMA:
      case LPAREN:
      case TELSE:
      case TTHEN: {
        i += 1;
        *len = 1;
      } break;
      case TNUMBER1: {
        *len = 1;
        i += 2;
      } break;
      case TNUMBER2: {
        *len = 2;
        i += 3;
      } break;
      case TNUMBER3: {
        *len = 3;
        i += 4;
      } break;
      case VINTEGER: {
        /*
         * Sizeof should reflect
         * sizeof of vm_vinteger_t
         * value
         */
        *len = sizeof(uint32_t);
        i += 1+sizeof(uint32_t);
      } break;
      case VFLOAT: {
        /*
         * Sizeof should reflect
         * sizeof of vm_vfloat_t
         * value
         */
        *len = sizeof(float);
        i += 1+sizeof(float);
      } break;
      case VPTR: {
        *len = sizeof(struct vm_vptr_t);
        i += sizeof(struct vm_vptr_t);
      } break;
      case TFUNCTION:
      case TOPERATOR: {
        *len = 2;
        i += 2;
      } break;
      case TEOF: {
        *len = 1;
        i += 1;
        loop = 0;
      } break;
      case TSTRING:
      case TEVENT:
      case TVAR: {
        i++;
        uint8_t current = getval((*text)[i]);

        /*
         * Consider tokens above 31 as regular characters
         */
        while(current >= 32 && current <= 128) {
          ++i;
          current = getval((*text)[i]);
        }
        *len = i - *start - 1;
      } break;
      /* LCOV_EXCL_START*/
      default: {
        logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
        return -1;
      } break;
      /* LCOV_EXCL_STOP*/
    }
    if(skip == nr++) {
      return i;
    }
  }
  return -1;
}

static int32_t varstack_find(char **text, uint16_t start, uint16_t len) {
  uint32_t a = varstack->nrbytes;
  uint32_t i = 0, x = 0;

  for(i=0;i<a;i++) {
    if(gettype(varstack->buffer[i]) == VCHAR) {
      struct vm_vchar_t *old = (struct vm_vchar_t *)&varstack->buffer[i];
      if(len == old->len) {
        for(x=0;x<len;x++) {
          if(getval(old->value[x]) != getval((*text)[start+x])) {
            break;
          }
        }
        if(x == old->len) {
          return i;
        }
      }
      i += sizeof(struct vm_vchar_t)-1;
    }
  }
  return -1;
}

static uint16_t varstack_add(char **text, uint16_t start, uint16_t len, uint8_t fixed) {
  uint16_t a = varstack->nrbytes;
  int32_t i = -1;

  struct vm_vchar_t *value = (struct vm_vchar_t *)&varstack->buffer[a];

#ifdef DEBUG
  assert(a/sizeof(struct vm_vchar_t *)/2 <= 127);
#endif

  i = varstack_find(text, start, len);

  if(i > -1) {
    return i;
  }

  if(fixed == 0) {
    for(i=0;i<a;i++) {
      if(gettype(varstack->buffer[i]) == VCHAR) {
        struct vm_vchar_t *old = (struct vm_vchar_t *)&varstack->buffer[i];
        if(getval(old->fixed) == 0 && getval(old->ref) == 0) {
          break;
        }
        i += sizeof(struct vm_vchar_t)-1;
      }
    }
    if(i == a) {
      i = -1;
    }
    if(i > -1) {
      a = i;
      value = (struct vm_vchar_t *)&varstack->buffer[a];
    }
  }
  if(i == -1) {
    if(a+sizeof(struct vm_vchar_t) > varstack->bufsize) {
      void *oldptr = (void *)varstack->buffer;

      if((varstack->buffer = (unsigned char *)REALLOC(varstack->buffer, varstack->bufsize+sizeof(struct vm_vchar_t))) == NULL) {
        OUT_OF_MEMORY
      }
      memset(&varstack->buffer[varstack->bufsize], 0, sizeof(struct vm_vchar_t));
      varstack->bufsize += sizeof(struct vm_vchar_t);

#if defined(DEBUG) || defined(COVERALLS)
      memused += sizeof(struct vm_vchar_t);
#endif

      if(varstack->buffer != oldptr) {
        value = (struct vm_vchar_t *)&varstack->buffer[a];
      }
    }
  }

  if((value->value = (char *)MALLOC(len+1)) == NULL) {
    OUT_OF_MEMORY;
  }
  memset(value->value, 0, len+1);
  for(uint16_t x=0;x<len;x++) {
    if(((uint8_t)getval((*text)[start+x])) == 127) {
      setval(value->value[x], 9);
    } else if(((uint8_t)getval((*text)[start+x])) == 128) {
      setval(value->value[x], 10);
    } else {
      setval(value->value[x], getval((*text)[start+x]));
    }
  }
  setval(value->value[len], 0);
  setval(value->type, VCHAR);
  setval(value->len, len);
  setval(value->ref, 0);
  setval(value->fixed, fixed);
  if(i == -1) {
    setval(varstack->nrbytes, a+sizeof(struct vm_vchar_t));
  }

#if defined(DEBUG) || defined(COVERALLS)
  memused += len+1;
#endif

  return a;
}

static int8_t rule_prepare(char **text,
  uint16_t *bcsize, uint16_t *heapsize, uint16_t *stacksize,
  uint16_t *memsize, uint16_t *len) {

  uint16_t pos = 0, nrblocks = 0, tpos = 0;
  uint16_t nrtokens = 0, l_func_pos = 0, l_lparen_pos = 0;
  uint8_t ctx = 0, do_clear = 1;
  int8_t nrhooks = 0, do_test = 1;
  char current = 0, next = 0;

  while(pos < *len) {
    lexer_parse_skip_characters(*text, *len, &pos);

    next = 0;

    current = getval((*text)[pos]);
    if(pos < *len) {
      next = getval((*text)[pos+1]);
    }

    if(current == '\'' || current == '"') {
      uint16_t s = pos;
      int8_t ret = 0;
      ret = lexer_parse_quoted_string((*text), *len, &pos);
      if(ret < 0) {
        if(ret == -1) {
          if((*len - pos) > 5) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("ERROR: no ending quotes found for '%.5s...'"), &(*text)[s]);
            /* LCOV_EXCL_STOP*/
          } else {
            logprintf_P(F("ERROR: no ending quotes found for '%.5s'"), &(*text)[s]);
          }
        } else if(ret == -2) {
          if((*len - pos) > 5) {
            logprintf_P(F("ERROR: found invalid ASCII at '%.5s...'"), &(*text)[s]);
          } else {
            /* LCOV_EXCL_START*/
            logprintf_P(F("ERROR: found invalid ASCII at '%.5s'"), &(*text)[s]);
            /* LCOV_EXCL_STOP*/
          }
        }
        return -1;
      } else {
        uint16_t len = pos - s;
        nrtokens++;

        if(varstack_find(text, s+1, len) == -1) {
          *stacksize += sizeof(struct vm_vchar_t);
          *memsize += len+1;
        }

#ifdef DEBUG
        printf("[STACK] VCHAR: %lu\n", sizeof(struct vm_vchar_t));
#endif
        /*
         * Skip the start quote
         */
        s++;
        uint16_t x = 0;
        uint8_t current = 0, next = 0, next2 = 0;
        setval((*text)[tpos], TSTRING); tpos++;
        for(x=0;x<len;x++) {
          current = getval((*text)[s+x]);
          if((x+1) < len) {
            next = getval((*text)[s+x+1]);
          }
          if((x+2) < len) {
            next2 = getval((*text)[s+x+2]);
          }
          /*
           * Remove escape characters
           */
          if(current == '\\' && (next == '"' || next == '\'')) {
            s++;
            len--;
          /*
           * Keep the literal '\n'
           */
          } else if(current == '\\' && next == '\\' && next2 == 'n') {
            s++;
            len--;
            setval((*text)[tpos+x], '\\');
            current = 'n';
          /*
           * Keep the literal '\t'
           */
          } else if(current == '\\' && next == '\\' && next2 == 't') {
            s++;
            len--;
            setval((*text)[tpos+x], '\\');
            current = 't';
          } else if(current == '\\' && next == 'n') {
            s+=2;
            len-=2;
            current = 10;
          } else if(current == '\\' && next == 't') {
            s++;
            len--;
            current = 9;
          }
          /*
           * Put ASCII TAB in ASCII DEL position
           * Put ASCII newline in ASCII EURO position
           */
          if(current == 9) {
            setval((*text)[tpos+x], 127);
          } else if(current == 10) {
            setval((*text)[tpos+x], 128);
          } else {
            setval((*text)[tpos+x], getval((*text)[s+x]));
          }
        }
        tpos += len;
        /*
         * Skip the end quote
         */
        pos+=2;
      }
    } else if(isdigit(current) || (current == '-' && pos < *len && isdigit(next))) {
      if(ctx == TCEVENT) {
        /* LCOV_EXCL_START*/
        /* FIXME */
        if((*len - pos) > 5) {
          logprintf_P(F("ERROR: event arguments can only contain variables at '%.5s...'"), &(*text)[pos]);
        } else {
          logprintf_P(F("ERROR: event arguments can only contain variables at '%.5s'"), &(*text)[pos]);
        }
        /* LCOV_EXCL_STOP*/
        return -1;
      }
      uint16_t newlen = 0;
      lexer_parse_number(&(*text)[pos], *len-pos, &newlen);

#if defined(DEBUG)
      assert(pos + newlen <= *len);
#endif

      char tmp = 0;
      float var = 0;
      tmp = getval((*text)[pos+newlen]);
      setval((*text)[pos+newlen], 0);
      char *cpy = (char *)MALLOC(newlen+1);
      if(cpy == NULL) {
        OUT_OF_MEMORY;
      }
      memset(cpy, 0, newlen+1);
      for(uint16_t x=0;x<newlen;x++) {
        cpy[x] = getval((*text)[pos+x]);
      }
      var = atof(cpy);
      FREE(cpy);

      float nr = 0;
      {
        uint16_t y = 0, start = 0, len = 0;
        uint8_t type = 0, match = 0;
        /*
         * Check if space for this variable
         * was already accounted for. However,
         * don't look past the tokens not
         * already parsed, because that is where
         * the actual unparsed rule lives. This
         * is exactly why we count the nr of
         * tokens while parsing.
         *
         */

        while(lexer_peek(text, y, &type, &start, &len) >= 0 && ++y < nrtokens && match == 0) {
          if(type == TNUMBER1 || type == TNUMBER2 || type == TNUMBER3 ||
             type == TNUMBER || type == VFLOAT || type == VINTEGER) {
            switch(type) {
              case VFLOAT:
              case VINTEGER: {
                uint32_t val = 0;
                val |= (getval((*text)[start+1]) & 0xFF) << 24;
                val |= (getval((*text)[start+2]) & 0xFF) << 16;
                val |= (getval((*text)[start+3]) & 0xFF) << 8;
                val |= (getval((*text)[start+4]) & 0xFF);
                if(type == VINTEGER && val == (uint32_t)var) {
                  match = 1;
                } else if(type == VFLOAT) {
                  float var1 = 0;
                  uint322float(val, &var1);
                  if(fabsf(var-var1) < EPSILON) {
                    match = 1;
                  }
                }
              } break;
              case TNUMBER:
              case TNUMBER1:
              case TNUMBER2:
              case TNUMBER3: {
                char tmp = 0;
                float var1 = 0;
                tmp = getval((*text)[start+1+len]);
                setval((*text)[start+1+len], 0);
                char *cpy = (char *)MALLOC(len + 1);
                if(cpy == NULL) {
                  OUT_OF_MEMORY;
                }
                memset(cpy, 0, len+1);
                for(uint16_t x=0;x<len;x++) {
                  cpy[x] = getval((*text)[start+1+x]);
                }
                var1 = atof(cpy);
                FREE(cpy);
                setval((*text)[start+1+len], tmp);

                if(modff(var1, &nr) == 0) {
                  if(var == (int32_t)var1) {
                    match = 1;
                  }
                } else {
                  if(fabsf(var-var1) < EPSILON) {
                    match = 1;
                  }
                }
              } break;
              /* LCOV_EXCL_START*/
              default: {
                logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                return -1;
              } break;
              /* LCOV_EXCL_STOP*/
            }
          }
        }
        if(match == 0) {
          if(modff(var, &nr) == 0) {
            /*
             * This range of integers
             * take less bytes when stored
             * as ascii characters.
             */
            if(var < 100 && var > -9) {
#ifdef DEBUG
              printf("[HEAP] VINTEGER: %lu\n", sizeof(struct vm_vinteger_t));
#endif
              *heapsize += sizeof(struct vm_vinteger_t);
            } else {
#ifdef DEBUG
              printf("[HEAP] VINTEGER: %lu\n", sizeof(struct vm_vinteger_t));
#endif
              *heapsize += sizeof(struct vm_vinteger_t);
            }
          } else {
#ifdef DEBUG
            printf("[HEAP] VFLOAT: %lu\n", sizeof(struct vm_vfloat_t));
#endif
            *heapsize += sizeof(struct vm_vfloat_t);
          }
        }
      }
      nrtokens++;
      if(newlen < 4) {
        switch(newlen) {
          case 1: {
            setval((*text)[tpos], TNUMBER1); tpos++;
          } break;
          case 2: {
            setval((*text)[tpos], TNUMBER2); tpos++;
          } break;
          case 3: {
            setval((*text)[tpos], TNUMBER3); tpos++;
          } break;
        }

        for(uint16_t x=0;x<newlen;x++) {
          setval((*text)[tpos+x], getval((*text)[pos+x]));
        }
        tpos += newlen;
      } else {
        uint32_t x = 0;
        if(modff(var, &nr) == 0) {
          /*
           * This range of integers
           * take less bytes when stored
           * as ascii characters.
           */
          setval((*text)[tpos], VINTEGER); tpos++;
          x = (uint32_t)var;
          if((var < 0 && var < -8388608) || (var > 0 && var > 16777215)) {
            logprintf_P(F("FATAL: Integer %g is out of range"), __FUNCTION__, __LINE__, (double)var);
            return -1;
          }
        } else {
          float2uint32(var, &x);
          setval((*text)[tpos], VFLOAT); tpos++;
          /*
           * FIXME: Check if float is out of range
           */
        }
        setval((*text)[tpos], (x >> 24) & 0xFF); tpos++;
        setval((*text)[tpos], (x >> 16) & 0xFF); tpos++;
        setval((*text)[tpos], (x >> 8) & 0xFF); tpos++;
        setval((*text)[tpos], x & 0xFF); tpos++;
      }

      setval((*text)[pos+newlen], tmp);
      pos += newlen;
/*    } else if(tolower(current) == 's' && tolower(next) == 'y' &&
              pos+5 < *len &&
              (
                tolower(getval((*text)[pos+2])) == 'n' &&
                tolower(getval((*text)[pos+3])) == 'c' &&
                tolower(getval((*text)[pos+4])) == ' ' &&
                tolower(getval((*text)[pos+5])) == 'i' &&
                tolower(getval((*text)[pos+6])) == 'f'
              )
            ) {
      nrtokens++;

      setval((*text)[tpos], TIF); tpos++;
      setval((*text)[tpos], 1); tpos++;

      ctx = TIF;

      pos += 7;
      nrblocks++;*/
    } else if(tolower(current) == 'i' && tolower(next) == 'f') {
      nrtokens++;
      *bcsize += sizeof(struct vm_top_t);

#ifdef DEBUG
      printf("[BC] OP_JMP: %lu\n", sizeof(struct vm_top_t));
#endif
      setval((*text)[tpos], TIF); tpos++;
      setval((*text)[tpos], 0); tpos++;

      do_test = 1;
      ctx = TIF;
      pos += 2;
      nrblocks++;
    } else if(tolower(current) == 'e' && tolower(next) == 'l' &&
              pos+4 < *len &&
              (
                tolower(getval((*text)[pos+2])) == 's' &&
                tolower(getval((*text)[pos+3])) == 'e' &&
                tolower(getval((*text)[pos+4])) == 'i' &&
                tolower(getval((*text)[pos+5])) == 'f'
              )
            ) {
      *bcsize += sizeof(struct vm_top_t);
      *bcsize += sizeof(struct vm_top_t);

#ifdef DEBUG
      printf("[BC] OP_JMP: %lu\n", sizeof(struct vm_top_t));
      printf("[BC] OP_JMP: %lu\n", sizeof(struct vm_top_t));
#endif

      setval((*text)[tpos], TELSEIF); tpos++;
      setval((*text)[tpos], 0); tpos++;

      do_test = 1;

      pos+=6;
    } else if(tolower(current) == 'o' && tolower(next) == 'n') {
      do_test = 0;
      nrtokens++;
      pos+=2;
      if(pos < *len) {
        lexer_parse_skip_characters((*text), *len, &pos);
      }
      uint16_t s = pos;
      lexer_parse_string((*text), *len, &pos);

      if(ctx == TCEVENT || ctx == TTHEN) {
        logprintf_P(F("ERROR: nested 'on' block"));
        return -1;
      }

      ctx = TCEVENT;

      {
        uint16_t len = pos - s;
        uint16_t x = 0;

        setval((*text)[tpos], TEVENT); tpos++;
        for(x=0;x<len;x++) {
          setval((*text)[tpos+x], getval((*text)[s+x]));
        }
        if(varstack_find(text, tpos, len) == -1) {
          *stacksize += sizeof(struct vm_vchar_t);
          *memsize += len+1;
        }
        tpos += len;
      }

#ifdef DEBUG
      printf("[STACK] VCHAR: %lu\n", sizeof(struct vm_vchar_t));
#endif
      nrblocks++;
    } else if(tolower(current) == 'e' && tolower(next) == 'l' &&
              pos+2 < *len &&
              (
                tolower(getval((*text)[pos+2])) == 's' &&
                tolower(getval((*text)[pos+3])) == 'e'
              )
            ) {
      nrtokens++;
      *bcsize += sizeof(struct vm_top_t);
#ifdef DEBUG
      printf("[BC] OP_JMP: %lu\n", sizeof(struct vm_top_t));
#endif
      pos+=4;
      setval((*text)[tpos], TELSE); tpos++;
    } else if(tolower(current) == 't' && tolower(next) == 'h' &&
              pos+2 < *len &&
              (
                tolower(getval((*text)[pos+2])) == 'e' &&
                tolower(getval((*text)[pos+3])) == 'n'
              )
            ) {
      if(do_test == 1) {
        do_test = 0;
        *bcsize += sizeof(struct vm_top_t);
#ifdef DEBUG
        printf("[BC] OP_TEST: %lu\n", sizeof(struct vm_top_t));
#endif
      }
      nrtokens++;
      pos+=4;
      setval((*text)[tpos], TTHEN); tpos++;
      ctx = TTHEN;
    } else if(tolower(current) == 'e' && tolower(next) == 'n' &&
              pos+1 < *len && tolower(getval((*text)[pos+2])) == 'd') {
      nrtokens++;
      pos+=3;
      nrblocks--;
      mmu_set_uint8(&(*text)[tpos], TEND); tpos++;
    } else if(tolower(current) == 'n' && tolower(next) == 'u' &&
              pos+2 < *len &&
              (
                tolower(getval((*text)[pos+2])) == 'l' &&
                tolower(getval((*text)[pos+3])) == 'l'
              )
            ) {
      nrtokens++;
      *heapsize += sizeof(struct vm_vnull_t);

#ifdef DEBUG
      printf("[HEAP] VNULL: %lu\n", sizeof(struct vm_vnull_t));
#endif
      setval((*text)[tpos], VNULL); tpos++;
      pos+=4;
    } else if(current == ',') {
      nrtokens++;
      *heapsize += sizeof(struct vm_vnull_t);
      if(ctx != TCEVENT) {
        *bcsize += sizeof(struct vm_top_t);
      }

#ifdef DEBUG
      if(ctx != TCEVENT) {
        printf("[BC] OP_PUSH: %lu\n", sizeof(struct vm_top_t));
      }
      printf("[HEAP] VNULL: %lu\n", sizeof(struct vm_vnull_t));
#endif

      setval((*text)[tpos], TCOMMA); tpos++;
      pos++;
    } else if(current == '(') {
      if((pos+2 < *len && getval((*text)[pos+2]) != ')') ||
         (l_func_pos == pos)) {
        nrtokens++;
        setval((*text)[tpos], LPAREN); tpos++;
        l_lparen_pos = pos;
      } else {
        l_lparen_pos = 0;
      }
      nrhooks++;
      pos++;
    } else if(current == ')') {
      if(l_lparen_pos > 0 && ((pos-l_lparen_pos) >= 2 || l_lparen_pos == l_func_pos)) {
        nrtokens++;
        setval((*text)[tpos], RPAREN); tpos++;
      }
      nrhooks--;
      if(nrhooks < 0) {
        /* LCOV_EXCL_START*/
        /* FIXME */
        if((*len - pos) > 5) {
          logprintf_P(F("ERROR: missing matching '(' at '%.5s...'"), &(*text)[pos]);
        } else {
          logprintf_P(F("ERROR: missing matching '(' at '%.5s'"), &(*text)[pos]);
        }
        /* LCOV_EXCL_STOP*/
        return -1;
      }
      pos++;
    } else if(current == '=' && next != '=') {
      nrtokens++;
      pos++;
      mmu_set_uint8(&(*text)[tpos], TASSIGN); tpos++;

      ctx = TASSIGN;
      do_clear = 0;
    } else if(current == ';') {
      nrtokens++;
      pos++;
      setval((*text)[tpos], TSEMICOLON); tpos++;
      if(do_clear == 1) {
        *bcsize += sizeof(struct vm_top_t);
#ifdef DEBUG
        printf("[BC] OP_CLEAR: %lu\n", sizeof(struct vm_top_t));
#endif
      }
      ctx = TSEMICOLON;
      do_clear = 1;
    } else {
      uint16_t a = 0, b = *len-(pos)-1;
      char chr = getval((*text)[a]);

      int16_t len1 = 0;
      for(a=(pos);a<*len;a++) {
        chr = getval((*text)[a]);
        if(chr == ' ' || chr == '(' || chr == ')' || chr == ',' || chr == ';') {
          b = a-(pos);
          break;
        }
      }
      char *cpy = (char *)MALLOC(b+1);
      if(cpy == NULL) {
        OUT_OF_MEMORY;
      }
      memset(cpy, 0, b+1);
      for(a=0;a<b;a++) {
        cpy[a] = getval((*text)[pos+a]);
      }

      if((len1 = is_function((*text), &pos, b)) > -1) {
        *heapsize += sizeof(struct vm_vnull_t);
        *bcsize += sizeof(struct vm_top_t);
        *bcsize += sizeof(struct vm_top_t);

        if(ctx != TASSIGN) {
          ctx = TFUNCTION;
        }

#ifdef DEBUG
        printf("[BC] OP_PUSH: %lu\n", sizeof(struct vm_top_t));
        printf("[BC] OP_CALL: %lu\n", sizeof(struct vm_top_t));
        printf("[HEAP] VNULL: %lu\n", sizeof(struct vm_vnull_t));
#endif

        nrtokens++;
        setval((*text)[tpos], TFUNCTION); tpos++;
        setval((*text)[tpos], len1); tpos++;
        if(ctx != TASSIGN) {
          do_clear = 1;
        }
        l_func_pos = pos+b;
        pos += b;
      } else if((len1 = is_operator((*text), &pos, b)) > -1) {
        nrtokens++;
        do_test = 0;
        *bcsize += sizeof(struct vm_top_t);
        *heapsize += sizeof(struct vm_vnull_t);

#ifdef DEBUG
        printf("[BC] OP_OP: %lu\n", sizeof(struct vm_top_t));
        printf("[HEAP] VNULL: %lu\n", sizeof(struct vm_vnull_t));
#endif
        pos += b;

        setval((*text)[tpos], TOPERATOR); tpos++;
        setval((*text)[tpos], len1); tpos++;
      } else if(rule_options.is_variable_cb != NULL && (len1 = rule_options.is_variable_cb(cpy, b)) > -1) {
        /*
         * Check for double vars
         */
        {
          uint16_t y = 0, start = 0, len = 0;
          uint8_t type = 0, match = 0;
          /*
           * Check if space for this variable
           * was already accounted for. However,
           * don't look past the tokens not
           * already parsed, because that is where
           * the actual unparsed rule lives. This
           * is exactly why we count the nr of
           * tokens while parsing.
           */
          while(lexer_peek(text, y, &type, &start, &len) >= 0 && ++y < nrtokens && match == 0) {
            if(type == TVAR) {
              uint16_t a = 0;
              if(len == len1) {
                for(a=0;a<len;a++) {
                  if(getval((*text)[start+1+a]) != getval((*text)[pos+a])) {
                    break;
                  }
                }
                if(a == len1) {
                  match = 1;
                }
              }
            }
          }

          if(match == 0) {
            if(varstack_find(text, pos, len1) == -1) {
              *stacksize += sizeof(struct vm_vchar_t);
              *memsize += len1+1;
            }
            if(ctx == TIF || ctx == TASSIGN) {
              do_clear = 0;
              *bcsize += sizeof(struct vm_top_t);
            }
            if(ctx == TSEMICOLON) {
              *bcsize += sizeof(struct vm_top_t);
            }
            if(ctx == TCEVENT) {
              do_clear = 0;
            }
            if(ctx == TTHEN || ctx == TCEVENT) {
              *bcsize += sizeof(struct vm_top_t);
            }
            *heapsize += sizeof(struct vm_vnull_t);
#ifdef DEBUG
            printf("[STACK] VCHAR: %lu\n", sizeof(struct vm_vchar_t));
            printf("[HEAP] VNULL: %lu\n", sizeof(struct vm_vnull_t));
            if(ctx == TIF || ctx == TASSIGN || ctx == TSEMICOLON) {
              printf("[BC] OP_SETVAL: %lu\n", sizeof(struct vm_top_t));
            }
            if(ctx == TTHEN || ctx == TCEVENT) {
              printf("[BC] OP_SETVAL: %lu\n", sizeof(struct vm_top_t));
            }
#endif
          } else {
            if(ctx == TIF) {
              *bcsize += sizeof(struct vm_top_t);
              *heapsize += sizeof(struct vm_vnull_t);

#ifdef DEBUG
              printf("[HEAP] VNULL: %lu\n", sizeof(struct vm_vnull_t));
              printf("[BC] OP_GETVAL: %lu\n", sizeof(struct vm_top_t));
#endif

            } else if(ctx == TASSIGN || ctx == TSEMICOLON) {
              do_clear = 0;
              *bcsize += sizeof(struct vm_top_t);

#ifdef DEBUG
              printf("[BC] OP_SETVAL: %lu\n", sizeof(struct vm_top_t));
#endif

            } else if(ctx == TTHEN) {
              do_clear = 0;
              *bcsize += sizeof(struct vm_top_t);

#ifdef DEBUG
              printf("[BC] OP_SETVAL: %lu\n", sizeof(struct vm_top_t));
#endif

            } else if(ctx == TCEVENT) {
#ifdef DEBUG
              printf("[BC] OP_SETVAL: %lu\n", sizeof(struct vm_top_t));
#endif
              *bcsize += sizeof(struct vm_top_t);
            }
          }
          if(ctx == TFUNCTION || ctx == TEVENT) {
            *bcsize += sizeof(struct vm_top_t);

#ifdef DEBUG
            printf("[BC] OP_GETVAL: %lu\n", sizeof(struct vm_top_t));
#endif
          }
        }

        setval((*text)[tpos], TVAR); tpos++;
        uint16_t a = 0;
        for(a=0;a<len1;a++) {
          setval((*text)[tpos+a], getval((*text)[pos+a]));
        }
        nrtokens++;
        tpos += len1;
        pos += len1;
      } else if(rule_options.is_event_cb != NULL && (len1 = rule_options.is_event_cb(cpy, b)) > -1) {
        do_test = 0;
        nrtokens++;
        char start = 0, end = 0;
        if(pos+b+1 < *len) {
          start = getval((*text)[(pos)+b]);
          end = getval((*text)[(pos)+b+1]);
        }

        if(pos < *len) {
          lexer_parse_skip_characters((*text), *len, &pos);
        }
        int16_t s = pos;
        lexer_parse_string((*text), *len, &pos);

        {
          uint16_t len = pos - s;

          setval((*text)[tpos], TEVENT); tpos++;
          uint16_t a = 0;
          for(a=0;a<len;a++) {
            setval((*text)[tpos+a], getval((*text)[s+a]));
          }
          if(varstack_find(text, tpos, len) == -1) {
            *stacksize += sizeof(struct vm_vchar_t);
            *memsize += len+1;
          }
          l_func_pos = pos;
          tpos += len;
        }

        *heapsize += sizeof(struct vm_vnull_t);
        *bcsize += sizeof(struct vm_top_t);

        if(!(start == '(' && end == ')')) {
          *bcsize += sizeof(struct vm_top_t);
        }

#ifdef DEBUG
        if(!(start == '(' && end == ')')) {
          printf("[BC] OP_PUSH: %lu\n", sizeof(struct vm_top_t));
        }
        printf("[BC] OP_CALL: %lu\n", sizeof(struct vm_top_t));
        printf("[HEAP] VNULL: %lu\n", sizeof(struct vm_vnull_t));
#endif
        if(ctx != TASSIGN) {
          do_clear = 1;
          ctx = TEVENT;
        }
      } else {
        if((*len - pos) > 5) {
          logprintf_P(F("ERROR: unknown token '%.5s...'"), &(*text)[pos]);
        } else {
          logprintf_P(F("ERROR: unknown token '%.5s'"), &(*text)[pos]);
        }
        FREE(cpy);
        return -1;
      }
      FREE(cpy);
    }

    if(nrblocks == 0) {
      if(nrhooks > 0) {
        /* LCOV_EXCL_START*/
        /* FIXME */
        logprintf_P(F("ERROR: missing matching ')'"), &(*text)[pos]);
        /* LCOV_EXCL_STOP*/
        return -1;
      }

      nrtokens++;
      *bcsize += sizeof(struct vm_top_t);
#ifdef DEBUG
      printf("[BC] OP_RET: %lu\n", sizeof(struct vm_top_t));
#endif

      setval((*text)[tpos], TEOF); tpos++;

      uint16_t oldpos = pos;
      if(pos < *len) {
        lexer_parse_skip_characters((*text), *len, &pos);
      }
      if(*len == pos) {
        return 0;
      }

      *len = oldpos;
      return 0;
    }
    if(pos < *len) {
      lexer_parse_skip_characters((*text), *len, &pos);
    }
  }

  if(nrhooks > 0) {
    /* LCOV_EXCL_START*/
    /* FIXME */
    logprintf_P(F("ERROR: missing matching ')'"), &(*text)[pos]);
    return -1;
    /* LCOV_EXCL_STOP*/
  }

  *bcsize += sizeof(struct vm_top_t);
#ifdef DEBUG
  printf("[BC] OP_RET: %lu\n", sizeof(struct vm_top_t));
#endif
  setval((*text)[tpos], TEOF); tpos++;

  return 0;
}

static uint32_t vm_stack_push(uint16_t pos, unsigned char *in) {
  uint8_t type = 0, i = 0;
  uint16_t size = 0, ret = 0;

  ret = getval(stack->nrbytes);

  type = gettype(in[0]);

#ifdef DEBUG
  printf("%s %d %d\n", __FUNCTION__, __LINE__, ret);
#endif

  size = ret+rule_max_var_bytes();
  setval(stack->nrbytes, size);
  setval(stack->bufsize, MAX(getval(stack->bufsize), size));

  if(type == VCHAR) {
    struct vm_vptr_t *value = (struct vm_vptr_t *)&stack->buffer[ret];
    setval(value->type, VPTR);
    setval(value->value, pos/sizeof(struct vm_top_t));
  } else {
    for(i=0;i<4;i++) {
      setval(stack->buffer[ret+i], getval(in[i]));
    }
  }

  return ret;
}


static uint16_t vm_stack_del(uint16_t idx) {
#ifdef DEBUG
  printf("%s %d %d\n", __FUNCTION__, __LINE__, idx);
#endif

  uint16_t ret = rule_max_var_bytes(), i = 0;
  uint16_t nrbytes = getval(stack->nrbytes);
  for(i=0;i<nrbytes-idx-ret;i++) {
    setval(stack->buffer[idx+i], getval(stack->buffer[idx+ret+i]));
  }

  nrbytes -= ret;

  setval(stack->nrbytes, nrbytes);
  setval(stack->bufsize, MAX(getval(stack->bufsize), nrbytes));

  return ret;
}

static int16_t vm_val_pos(int8_t pos) {
  if(pos < 0) {
    return (((pos*-1)-1)*rule_max_var_bytes())+4;
  } else if(pos == 0) {
    return 4;
  } else {
    return ((pos-1)*rule_max_var_bytes())+4;
  }
}

static int32_t vm_val_posr(int8_t pos) {
  return ((pos-4)/rule_max_var_bytes()*-1)-1;
}

uint8_t rules_type(int8_t pos) {
  int16_t offset = vm_val_pos(pos);
  if(pos < 0) {
    offset = getval(stack->nrbytes)-offset;
  }
  if(offset >= 4) {
    if(getval(stack->buffer[offset]) == VPTR) {
      return VCHAR;
    } else {
      return getval(stack->buffer[offset]) & 0x1F;
    }
  } else {
    return 0;
  }
}

void rules_pushnil(void) {
  unsigned char *val = (unsigned char *)MALLOC(rule_max_var_bytes());
  if(val == NULL) {
    OUT_OF_MEMORY
  }
  memset(val, 0, rule_max_var_bytes());
  struct vm_vnull_t *node = (struct vm_vnull_t *)val;
  node->type = VNULL;

  vm_stack_push(0, val);
  FREE(val);
}

void rules_pushinteger(int nr) {
  unsigned char *val = (unsigned char *)MALLOC(rule_max_var_bytes());
  if(val == NULL) {
    OUT_OF_MEMORY
  }
  memset(val, 0, rule_max_var_bytes());
  struct vm_vinteger_t *node = (struct vm_vinteger_t *)val;
  node->type = VINTEGER;
  setval(node->value[0], ((uint32_t)nr >> 16) & 0xFF);
  setval(node->value[1], ((uint32_t)nr >> 8) & 0xFF);
  setval(node->value[2], ((uint32_t)nr) & 0xFF);

  vm_stack_push(0, val);
  FREE(val);
}

void rules_pushfloat(float nr) {
  float f = float32to27(nr);
  uint32_t x = 0;
  float2uint32(f, &x);

  unsigned char *val = (unsigned char *)MALLOC(rule_max_var_bytes());
  if(val == NULL) {
    OUT_OF_MEMORY
  }
  memset(val, 0, rule_max_var_bytes());
  struct vm_vfloat_t *node = (struct vm_vfloat_t *)val;

  setval(node->type, VFLOAT | ((((uint32_t)x >> 29) & 0x7) << 5));
  setval(node->value[0], ((uint32_t)x >> 21) & 0xFF);
  setval(node->value[1], ((uint32_t)x >> 13) & 0xFF);
  setval(node->value[2], ((uint32_t)x >> 5) & 0xFF);

  vm_stack_push(0, val);
  FREE(val);
}

void rules_pushstring(char *str) {
  uint16_t c = varstack_add(&str, 0, strlen(str), 0);
  assert(c >= 0);

  unsigned char *val = (unsigned char *)MALLOC(rule_max_var_bytes());
  if(val == NULL) {
    OUT_OF_MEMORY
  }
  memset(val, 0, rule_max_var_bytes());
  struct vm_vptr_t *node = (struct vm_vptr_t *)val;

  setval(node->type, VPTR);
  setval(node->value, c/sizeof(struct vm_top_t));

  vm_stack_push(0, val);
  FREE(val);
}

void rules_ref(const char *str) {
  int32_t c = varstack_find((char **)&str, 0, strlen(str));
  if(c == -1) {
    return;
  }

  struct vm_vchar_t *node = (struct vm_vchar_t *)&varstack->buffer[c];
  if(getval(node->fixed) == 0) {
    setval(node->ref, getval(node->ref)+1);
  }
}

void rules_unref(const char *str) {
  int32_t c = varstack_find((char **)&str, 0, strlen(str));
  if(c == -1) {
    return;
  }

  struct vm_vchar_t *node = (struct vm_vchar_t *)&varstack->buffer[c];
  if(getval(node->fixed) == 0) {
    setval(node->ref, getval(node->ref)-1);
    if(getval(node->ref) == 0) {
      FREE(node->value);
#if defined(DEBUG) || defined(COVERALLS)
      memused -= node->len+1;
#endif
      node->value = NULL;
      setval(node->len, 0);
    }
  }
}

const char *rules_tostring(int8_t pos) {
  int16_t offset = vm_val_pos(pos);
  if(pos < 0) {
    offset = getval(stack->nrbytes)-offset;
  }
  if(offset >= 4) {
    if(getval(stack->buffer[offset]) == VPTR) {
      struct vm_vptr_t *node = (struct vm_vptr_t *)&stack->buffer[offset];
      uint16_t pos = getval(node->value)*sizeof(struct vm_top_t);
      struct vm_vchar_t *var = (struct vm_vchar_t *)&varstack->buffer[pos];

      return (const char *)var->value;
    }
  }
  return NULL;
}

int rules_tointeger(int8_t pos) {
  int16_t offset = vm_val_pos(pos);
  if(pos < 0) {
    offset = getval(stack->nrbytes)-offset;
  }
  if(offset >= 4) {
    if(getval(stack->buffer[offset]) == VINTEGER) {
      struct vm_vinteger_t *node = (struct vm_vinteger_t *)&stack->buffer[offset];
      int val = 0;
      val |= getval(node->value[0]) << 16;
      val |= getval(node->value[1]) << 8;
      val |= getval(node->value[2]);

      /*
       * Correctly restore sign
       */
      if(val & 0x800000) {
        val |= 0xFF000000;
      }

      return val;
    }
  }
  return 0;
}

float rules_tofloat(int8_t pos) {
  int16_t offset = vm_val_pos(pos);
  if(pos < 0) {
    offset = getval(stack->nrbytes)-offset;
  }
  if(offset >= 4) {
    if((getval(stack->buffer[offset]) & 0x1F) == VFLOAT) {
      struct vm_vfloat_t *node = (struct vm_vfloat_t *)&stack->buffer[offset];
      uint32_t val = 0;

      val |= (getval(node->type) >> 5) << 29;
      val |= getval(node->value[0]) << 21;
      val |= getval(node->value[1]) << 13;
      val |= getval(node->value[2]) << 5;

      float f = 0;
      uint322float(val, &f);

      return f;
    }
  }
  return 0;
}

uint8_t rules_gettop(void) {
  return (getval(stack->nrbytes)-4) / rule_max_var_bytes();
}

void rules_remove(int8_t pos) {
  int16_t offset = vm_val_pos(pos);
  if(pos < 0) {
    offset = getval(stack->nrbytes)-offset;
  }
  vm_stack_del(offset);
}

static uint16_t bc_parent(struct rules_t *obj, uint8_t type, int16_t a, int16_t b, int16_t c) {
  uint16_t ret = 0, size = 0;
  ret = getval(obj->bc.nrbytes);

  size = ret+sizeof(struct vm_top_t);

  assert(size <= getval(obj->bc.bufsize));
  struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[ret];
  setval(node->type, type);
  setval(node->a, a);
  setval(node->b, b);
  setval(node->c, c);
  setval(obj->bc.nrbytes, size);

  return ret;
}

int32_t bc_before(uint16_t step) {
  int32_t i = step;

  i -= sizeof(struct vm_top_t);

  if(i < 0) {
    return -1;
  }
  return i;
}

int32_t bc_next(struct rules_t *obj, uint16_t step) {
  uint16_t i = step;
  uint16_t nrbytes = getval(obj->bc.nrbytes);

  i += sizeof(struct vm_top_t);

  if(i >= nrbytes) {
    return -1;
  }
  return i;
}

void bc_group(struct rules_t *obj, uint16_t start, uint16_t end) {
  uint16_t i = 0;

  if(start < end) {
    if(++group > 2) {
      group = 1;
    }
    for(i=start;i<end;i = bc_next(obj, i)) {
      set_group(obj->bc.buffer[i], group);
    }
  }
}

static int32_t vm_heap_push(struct rules_t *obj, uint8_t type, char **text, uint16_t start, uint16_t len, uint8_t forced) {
  uint16_t size = 0, ret = 0;
  uint16_t i = 0;

  ret = getval(obj->heap->nrbytes);

#ifdef DEBUG
  printf("%s %d %d\n", __FUNCTION__, __LINE__, ret);
#endif

  switch(type) {
    case TNUMBER:
    case TNUMBER1:
    case TNUMBER2:
    case TNUMBER3: {
      char tmp = 0;
      float var = 0;
      tmp = getval((*text)[start+1+len]);
      setval((*text)[start+1+len], 0);
      char *cpy = (char *)MALLOC(len+1);
      if(cpy == NULL) {
        OUT_OF_MEMORY;
      }
      memset(cpy, 0, len+1);
      for(uint16_t x=0;x<len;x++) {
        cpy[x] = (char)getval((*text)[start+1+x]);
      }
      var = atof(cpy);
      FREE(cpy);
      setval((*text)[start+1+len], tmp);

      float nr = 0;
      if(modff(var, &nr) == 0) {
        for(i=4;i<ret;i+=rule_max_var_bytes()) {
          if(gettype(obj->heap->buffer[i]) == VINTEGER) {
            struct vm_vinteger_t *old = (struct vm_vinteger_t *)&obj->heap->buffer[i];
            if(
              getval(old->value[0]) == (((uint32_t)var >> 16) & 0xFF) &&
              getval(old->value[1]) == (((uint32_t)var >> 8) & 0xFF) &&
              getval(old->value[2]) == (((uint32_t)var) & 0xFF)
            ) {
              return i;
            }
          }
        }

        size = ret+rule_max_var_bytes();
        struct vm_vinteger_t *value = (struct vm_vinteger_t *)&obj->heap->buffer[ret];
        setval(value->type, VINTEGER);
        setval(value->value[0], ((uint32_t)var >> 16) & 0xFF);
        setval(value->value[1], ((uint32_t)var >> 8) & 0xFF);
        setval(value->value[2], ((uint32_t)var) & 0xFF);

        setval(obj->heap->nrbytes, size);
        setval(obj->heap->bufsize, size);
      } else {
        float f = float32to27(var);
        uint32_t x = 0;
        float2uint32(f, &x);

        for(i=4;i<ret;i+=rule_max_var_bytes()) {
          if(gettype(obj->heap->buffer[i]) == VFLOAT) {
            struct vm_vfloat_t *old = (struct vm_vfloat_t *)&obj->heap->buffer[i];

            if(
              ((getval(old->type) & 0xE0) >> 5) == (((uint32_t)x >> 29) & 0x7) &&
              getval(old->value[0]) == (((uint32_t)x >> 21) & 0xFF) &&
              getval(old->value[1]) == (((uint32_t)x >> 13) & 0xFF) &&
              getval(old->value[2]) == (((uint32_t)x >> 5) & 0xFF)
            ) {
              return i;
            }
          }
        }

        size = ret+rule_max_var_bytes();
        struct vm_vfloat_t *value = (struct vm_vfloat_t *)&obj->heap->buffer[ret];

        setval(value->type, VFLOAT | ((((uint32_t)x >> 29) & 0x7) << 5));
        setval(value->value[0], ((uint32_t)x >> 21) & 0xFF);
        setval(value->value[1], ((uint32_t)x >> 13) & 0xFF);
        setval(value->value[2], ((uint32_t)x >> 5) & 0xFF);

        setval(obj->heap->nrbytes, size);
        setval(obj->heap->bufsize, size);
      }
    } break;
    case VINTEGER: {
      uint32_t val = 0;
      val |= (getval((*text)[start+1]) & 0xFF) << 24;
      val |= (getval((*text)[start+2]) & 0xFF) << 16;
      val |= (getval((*text)[start+3]) & 0xFF) << 8;
      val |= (getval((*text)[start+4]) & 0xFF);

      for(i=4;i<ret;i+=rule_max_var_bytes()) {
        if(gettype(obj->heap->buffer[i]) == VINTEGER) {
          struct vm_vinteger_t *old = (struct vm_vinteger_t *)&obj->heap->buffer[i];
          if(
            getval(old->value[0]) == ((val >> 16) & 0xFF) &&
            getval(old->value[1]) == ((val >> 8) & 0xFF) &&
            getval(old->value[2]) == (val & 0xFF)
          ) {
            return i;
          }
        }
      }

      size = ret+rule_max_var_bytes();

      struct vm_vinteger_t *value = (struct vm_vinteger_t *)&obj->heap->buffer[ret];
      setval(value->type, VINTEGER);

      setval(value->value[0], ((uint32_t)val >> 16) & 0xFF);
      setval(value->value[1], ((uint32_t)val >> 8) & 0xFF);
      setval(value->value[2], ((uint32_t)val) & 0xFF);

      setval(obj->heap->nrbytes, size);
      setval(obj->heap->bufsize, size);
    } break;
    case VFLOAT: {
      uint32_t tmp = 0;

      tmp |= (getval((*text)[start+1]) & 0xFF) << 24;
      tmp |= (getval((*text)[start+2]) & 0xFF) << 16;
      tmp |= (getval((*text)[start+3]) & 0xFF) << 8;
      tmp |= (getval((*text)[start+4]) & 0xFF);

      for(i=4;i<ret;i+=rule_max_var_bytes()) {
        if(gettype(obj->heap->buffer[i]) == VFLOAT) {
          struct vm_vfloat_t *old = (struct vm_vfloat_t *)&obj->heap->buffer[i];
          if(
              ((getval(old->type) & 0xE0) >> 5) == (((uint32_t)tmp >> 29) & 0x7) &&
              getval(old->value[0]) == (((uint32_t)tmp >> 21) & 0xFF) &&
              getval(old->value[1]) == (((uint32_t)tmp >> 13) & 0xFF) &&
              getval(old->value[2]) == (((uint32_t)tmp >> 5) & 0xFF)
            ) {
            return i;
          }
        }
      }

      size = ret+rule_max_var_bytes();
      struct vm_vfloat_t *value = (struct vm_vfloat_t *)&obj->heap->buffer[ret];

      setval(value->type, VFLOAT | ((((uint32_t)tmp >> 29) & 0x7) << 5));
      setval(value->value[0], ((uint32_t)tmp >> 21) & 0xFF);
      setval(value->value[1], ((uint32_t)tmp >> 13) & 0xFF);
      setval(value->value[2], ((uint32_t)tmp >> 5) & 0xFF);

      setval(obj->heap->nrbytes, size);
      setval(obj->heap->bufsize, size);
    } break;
    case VNULL: {
      size = ret+rule_max_var_bytes();

      if(forced == 0) {
        for(i=4;i<ret;i+=rule_max_var_bytes()) {
          if(gettype(obj->heap->buffer[i]) == VNULL) {
            if((start == 1 && get_group(obj->heap->buffer[i]) > 0) ||
               (start == 0 && get_group(obj->heap->buffer[i])) == 0) {
              return i;
            }
          }
        }
      }

      struct vm_vnull_t *value = (struct vm_vnull_t *)&obj->heap->buffer[ret];
      setval(value->type, VNULL);
      setval(obj->heap->nrbytes, size);
      setval(obj->heap->bufsize, size);
      if(start == 1) {
        set_group(obj->heap->buffer[ret], 1);
      }
    } break;
    /* LCOV_EXCL_START*/
    default: {
      logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
      return -1;
    } break;
    /* LCOV_EXCL_STOP*/
  }

  return ret;
}

static int32_t vm_heap_next(struct rules_t *obj, uint8_t type, uint8_t skip) {
  uint16_t ret = 0, cnt = 0;
  uint16_t i = 0;

  ret = getval(obj->heap->nrbytes);

#ifdef DEBUG
  printf("%s %d %d\n", __FUNCTION__, __LINE__, ret);
#endif

  switch(type) {
    case VNULL: {
      for(i=4;i<ret;i+=rule_max_var_bytes()) {
        if(gettype(obj->heap->buffer[i]) == VNULL) {
          if(cnt >= skip && get_group(obj->heap->buffer[i]) == 0) {
            return i;
          }
          cnt++;
        }
      }
    } break;
    /* LCOV_EXCL_START*/
    default: {
      logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
      return -1;
    } break;
    /* LCOV_EXCL_STOP*/
  }

  return -1;
}

static int32_t bc_whatfirst(char **text, uint16_t pos, uint8_t token) {
  uint16_t start = 0, len = 0, has_paren = 0, last_paren = -1;
  uint8_t type = 0;
  int8_t nrhooks = 0;

  while(1) {
    /*
     * Start looking for the furthest
     * nestable token, so we can cache
     * it first.
     */
    /* LCOV_EXCL_START*/
    if(lexer_peek(text, pos, &type, &start, &len) < 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
      break;
    }
    /* LCOV_EXCL_STOP*/
    if(type == TEOF || type == token) {
      break;
    }
    if(type == LPAREN) {
      nrhooks++;
      if(pos > has_paren) {
        has_paren = pos;
      }
    }
    if(type == RPAREN) {
      break;
    }
    pos++;
  }
  pos--;
  nrhooks = 0;
  last_paren = has_paren;
  while(1) {
    /* LCOV_EXCL_START*/
    if(lexer_peek(text, pos, &type, &start, &len) < 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
      break;
    }
    /* LCOV_EXCL_STOP*/
    if(pos == 0) {
      break;
    }
    if(type == LPAREN) {
      nrhooks--;
    }
    if(has_paren > 0 && nrhooks < 0) {
      has_paren = last_paren;
      break;
    }
    pos--;
  }
  if(has_paren > 0) {
    /* LCOV_EXCL_START*/
    if(lexer_peek(text, has_paren-1, &type, &start, &len) < 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
      return -1;
    }
    /* LCOV_EXCL_STOP*/

    if(type == TFUNCTION || type == TEVENT) {
      has_paren--;
    }
  }

  return has_paren;
}

static void bc_assign_slots(struct rules_t *obj) {
  int32_t a = 0, c = 0;
  uint16_t start = 0, end = 0, tmp = 0;
  uint8_t vars = 0, min = 0, max = 0, changed = 0;

  while(1) {
    vars = 0, min = 0, max = 0, a = 0, c = 0, start = 0;
    for(a=end;a<getval(obj->bc.nrbytes);a = bc_next(obj, a)) {
      if(gettype(obj->bc.buffer[a]) != OP_JMP) {
        break;
      }
    }
    end = a;
    start = end;

    for(a=end;a<getval(obj->bc.nrbytes);a = bc_next(obj, a)) {
      if(a == -1) {
        return;
      }
      struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[a];
      if(max == 0) {
        if((int8_t)getval(node->a) > 0 &&
           gettype(obj->bc.buffer[a]) != OP_JMP &&
           gettype(obj->bc.buffer[a]) != OP_SETVAL) {
          if(!(gettype(obj->bc.buffer[a]) == OP_PUSH && getval(node->c) == 1)) {
            max = MAX(max, (int8_t)getval(node->a));
          } else {
            max = 1;
          }
          start = a;
        }
      } else {
        if(gettype(obj->bc.buffer[a]) != OP_JMP &&
           gettype(obj->bc.buffer[a]) != OP_SETVAL &&
           !(gettype(obj->bc.buffer[a]) == OP_PUSH && getval(node->c) == 1)
          ) {
          max = MAX(max, (int8_t)getval(node->a));
        }
        if(tmp > 0 && gettype(obj->bc.buffer[a]) == OP_SETVAL &&
           gettype(obj->bc.buffer[tmp]) == OP_CLEAR && bc_before(a) >= 0) {
          end = bc_before(a);
          break;
        }
        if(((c = bc_before(a)) >= 0) &&
           ((((int8_t)getval(node->a) < 0 && gettype(obj->bc.buffer[a]) != OP_PUSH)) ||
           gettype(obj->bc.buffer[a]) == OP_JMP ||
           gettype(obj->bc.buffer[a]) == OP_TEST ||
           gettype(obj->bc.buffer[a]) == OP_RET)) {
          end = c;
          break;
        } else if(gettype(obj->bc.buffer[a]) == OP_SETVAL ||
                  gettype(obj->bc.buffer[a]) == OP_CLEAR) {
          /*
           * An assignment or a call statement closes an
           * independent statement. Without this split the
           * temporary slot counter of the next pass runs
           * negative over several statements and collides
           * with the heap slots of constants (#1006).
           */
          end = a;
          break;
        }
      }
      tmp = a;
    }
    tmp = bc_next(obj, end);

    vars = max*2;
    min = vars;

    /*
     * Calculate minimum amount of
     * NULL slots to store temporary
     * variables
     */
    for(a=start;a<=end;a = bc_next(obj, a)) {
      if(a == -1) {
        break;
      }

      struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[a];

      int8_t d = (int8_t)getval(x->a);
      if(!is_op_and_math(gettype(obj->bc.buffer[a])) &&
         gettype(obj->bc.buffer[a]) != OP_GETVAL &&
         gettype(obj->bc.buffer[a]) != OP_CLEAR &&
         (gettype(obj->bc.buffer[a]) == OP_PUSH && (d < 0 || d >= min))) {
        continue;
      }
      if(gettype(obj->bc.buffer[a]) == OP_CALL && getval(x->a) == 0) {
        continue;
      }
      if(gettype(obj->bc.buffer[a]) == OP_SETVAL) {
        continue;
      }

      int32_t e = bc_before(a);
      struct vm_top_t *z = NULL;
      if(e >= 0) {
        z = (struct vm_top_t *)&obj->bc.buffer[e];
      }

      if(z != NULL && gettype(x->type) == OP_CALL &&
         gettype(z->type) == OP_PUSH &&
         (int8_t)getval(z->a) >= min) {
      } else if(gettype(x->type) != OP_GETVAL &&
         (((int8_t)getval(x->b) >= vars || (int8_t)getval(x->c) >= vars))) {
        vars = MAX((int8_t)getval(x->c), (int8_t)getval(x->b));
      } else {
        vars--;
      }
      min = MIN(vars, min);

      if(gettype(obj->bc.buffer[a]) != OP_SETVAL &&
         gettype(obj->bc.buffer[a]) != OP_CLEAR &&
         !(gettype(obj->bc.buffer[a]) == OP_PUSH && getval(x->c) == 1)) {
        setval(x->a, vars);
      }

      for(c=a;c<=end;c = bc_next(obj, c)) {
        if(c == -1) {
          break;
        }
        if(a == c) {
          continue;
        }
        struct vm_top_t *z = (struct vm_top_t *)&obj->bc.buffer[c];
        if(
           gettype(obj->bc.buffer[c]) != OP_CLEAR &&
           gettype(obj->bc.buffer[c]) != OP_SETVAL &&
           !(gettype(obj->bc.buffer[c]) == OP_PUSH && getval(z->c) == 1)) {
          if((int8_t)getval(z->a) == d) {
            setval(z->a, vars);
          }
        }
        if(gettype(obj->bc.buffer[c]) != OP_GETVAL &&
           gettype(obj->bc.buffer[c]) != OP_CALL &&
           gettype(obj->bc.buffer[c]) != OP_CLEAR &&
           gettype(obj->bc.buffer[c]) != OP_PUSH) {
          if((int8_t)getval(z->b) == d) {
            setval(z->b, vars);
          }
          if((int8_t)getval(z->c) == d) {
            if((int8_t)getval(z->b) == vars) {
              if((int8_t)getval(z->a) == vars) {
                if(gettype(obj->bc.buffer[c]) != OP_CLEAR &&
                   gettype(obj->bc.buffer[c]) != OP_SETVAL &&
                   !(gettype(obj->bc.buffer[c]) == OP_PUSH && getval(z->c) == 1)) {
                  setval(z->a, vars-1);
                }
              }
              vars--;
              min = MIN(vars, min);
              if(gettype(obj->bc.buffer[a]) != OP_CLEAR &&
                 gettype(obj->bc.buffer[a]) != OP_SETVAL &&
                 !(gettype(obj->bc.buffer[a]) == OP_PUSH && getval(z->c) == 1)) {
                setval(x->a, vars);
              }
            }
            setval(z->c, vars);
          }
        }
      }
    }

    /*
     * Reassign NULL slots to heap slots
     */
    uint8_t offset = 0, first = 1;
    int32_t e = 0;
    for(a=start;a<=end;a = bc_next(obj, a)) {
      if(a == -1) {
        break;
      }

      struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[a];

      int8_t d = (int8_t)getval(x->a);

      tmp = bc_next(obj, a);

      if(!is_op_and_math(gettype(obj->bc.buffer[a])) &&
         gettype(obj->bc.buffer[a]) != OP_GETVAL &&
         (gettype(obj->bc.buffer[a]) == OP_PUSH && d < 0)) {
        continue;
      }
      if(a == end && gettype(obj->bc.buffer[a]) == OP_SETVAL) {
        continue;
      }
      if(gettype(obj->bc.buffer[a]) == OP_CLEAR) {
        continue;
      }
      if(gettype(obj->bc.buffer[a]) == OP_CALL && getval(x->a) == 0) {
        continue;
      }
      if(d >= min) {
        if(tmp > 0 && gettype(obj->bc.buffer[a]) == OP_CALL &&
           gettype(obj->bc.buffer[tmp]) != OP_CLEAR) {
          offset++;
        } else if(gettype(obj->bc.buffer[a]) != OP_PUSH) {
          int16_t b = vm_val_pos((int8_t)getval(x->b));
          int16_t c = vm_val_pos((int8_t)getval(x->c));

          /*
           * If NULL was meant as a variable,
           * make sure it is preserved as NULL
           */
          if((gettype(obj->heap->buffer[b]) == VNULL || gettype(obj->heap->buffer[c]) == VNULL) && first == 1) {
            offset++;
          }
        }
        e = vm_heap_next(obj, VNULL, offset);
        if(e == -1) {
          e = vm_heap_push(obj, VNULL, NULL, 0, 0, 1);
        }
        first = 0, changed = 0;
        e = vm_val_posr(e);

        for(c=start;c<=end;c = bc_next(obj, c)) {
          if(c == -1) {
            break;
          }
          struct vm_top_t *z = (struct vm_top_t *)&obj->bc.buffer[c];
          if(gettype(obj->bc.buffer[c]) != OP_SETVAL &&
             gettype(obj->bc.buffer[c]) != OP_CLEAR &&
             !(gettype(obj->bc.buffer[c]) == OP_PUSH && getval(z->c) == 1)) {
            if((int8_t)getval(z->a) == d) {
              changed = 1;
              setval(z->a, e);
            }
          }
          if(gettype(obj->bc.buffer[c]) != OP_GETVAL &&
             gettype(obj->bc.buffer[c]) != OP_CALL &&
             gettype(obj->bc.buffer[c]) != OP_CLEAR &&
             gettype(obj->bc.buffer[c]) != OP_PUSH) {
            if((int8_t)getval(z->b) == d) {
              changed = 1;
              setval(z->b, e);
            }
          }
          if(gettype(obj->bc.buffer[c]) != OP_SETVAL &&
             gettype(obj->bc.buffer[c]) != OP_GETVAL &&
             gettype(obj->bc.buffer[c]) != OP_CALL &&
             gettype(obj->bc.buffer[c]) != OP_CLEAR &&
             gettype(obj->bc.buffer[c]) != OP_PUSH) {
            if((int8_t)getval(z->c) == d) {
              changed = 1;
              setval(z->c, e);
            }
          }
        }
        if(changed == 1) {
          offset++;
        }
      }
    }
    end = bc_next(obj, end);
  }
}

static int16_t bc_find_math_dep(struct rules_t *obj, uint16_t start, uint16_t a) {
  struct vm_top_t *tmp = NULL;
  int32_t pos = start;
  while((pos = bc_before(pos)) != -1) {
    tmp = (struct vm_top_t *)&obj->bc.buffer[pos];
    if(a == getval(tmp->a)) {
      break;
    }
  }
  return pos;
}

static void bc_math_move_closest(struct rules_t *obj, int16_t limit) {
  int16_t tmp1 = -1, tmp1B = -1, tmp1C = -1;
  uint16_t nrbytes = bc_before(getval(obj->bc.nrbytes));

  while(nrbytes > limit) {
    struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[nrbytes];

    if(gettype(x->type) == OP_GETVAL) {
      tmp1 = nrbytes;
      tmp1B = -1;

      if(getval(x->a) >= 0) {
        tmp1B = tmp1;
        while((tmp1B = bc_before(tmp1B)) != -1 && tmp1B >= limit) {
          struct vm_top_t *xB = (struct vm_top_t *)&obj->bc.buffer[tmp1B];
          if(gettype(xB->type) == OP_GETVAL || gettype(xB->type) == OP_CALL) {
            continue;
          }
          if(getval(x->a) == getval(xB->c) && gettype(xB->type) != OP_GETVAL) {
            break;
          }
        }
      }

      if(tmp1B != -1 && tmp1B >= limit) {
        while(tmp1B < tmp1) {
          struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[tmp1B];

          if((tmp1B = bc_next(obj, tmp1B)) == -1) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d %d"), __FUNCTION__, __LINE__);
            exit(-1);
            /* LCOV_EXCL_STOP*/
          }

          if(tmp1B < limit) {
            break;
          }

          struct vm_top_t *y = (struct vm_top_t *)&obj->bc.buffer[tmp1B];
          int8_t xT = getval(x->type), xA = getval(x->a), xB = getval(x->b), xC = getval(x->c);
          int8_t yA = getval(y->a);

          setval(x->type, getval(y->type));
          setval(x->b, getval(y->b));
          setval(x->c, getval(y->c));

          setval(y->type, xT);
          setval(y->b, xB);
          setval(y->c, xC);

          tmp1C = limit;
          while(tmp1C != -1) {
            struct vm_top_t *z = (struct vm_top_t *)&obj->bc.buffer[tmp1C];

            if(gettype(z->type) != OP_GETVAL &&
               gettype(z->type) != OP_CALL) {
              if(getval(z->c) == xA) {
                setval(z->c, yA);
              } else if(getval(z->c) == yA) {
                setval(z->c, xA);
              }
              if(getval(z->b) == xA) {
                setval(z->b, yA);
              } else if(getval(z->b) == yA) {
                setval(z->b, xA);
              }
            }
            tmp1C = bc_next(obj, tmp1C);
          }
        }
      }
    }
    x = (struct vm_top_t *)&obj->bc.buffer[nrbytes];
    if(gettype(x->type) == OP_GETVAL) {
      tmp1 = nrbytes;
      tmp1B = -1;

      if(getval(x->a) >= 0) {
        tmp1B = tmp1;
        while((tmp1B = bc_before(tmp1B)) != -1 && tmp1B >= limit) {
          struct vm_top_t *xB = (struct vm_top_t *)&obj->bc.buffer[tmp1B];
          if(gettype(xB->type) == OP_GETVAL || gettype(xB->type) == OP_CALL) {
            continue;
          }
          if(getval(x->a) == getval(xB->b)) {
            break;
          }
        }
      }

      if(tmp1B != -1 && tmp1B >= limit) {
        while(tmp1B < tmp1) {
          struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[tmp1B];

          if((tmp1B = bc_next(obj, tmp1B)) == -1) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d %d"), __FUNCTION__, __LINE__);
            exit(-1);
            /* LCOV_EXCL_STOP*/
          }

          if(tmp1B < limit) {
            break;
          }

          struct vm_top_t *y = (struct vm_top_t *)&obj->bc.buffer[tmp1B];
          int8_t xT = getval(x->type), xA = getval(x->a), xB = getval(x->b), xC = getval(x->c);
          int8_t yA = getval(y->a);

          setval(x->type, getval(y->type));
          setval(x->b, getval(y->b));
          setval(x->c, getval(y->c));

          setval(y->type, xT);
          setval(y->b, xB);
          setval(y->c, xC);

          tmp1C = limit;
          while(tmp1C != -1) {
            struct vm_top_t *z = (struct vm_top_t *)&obj->bc.buffer[tmp1C];

            if(gettype(z->type) != OP_GETVAL &&
               gettype(z->type) != OP_CALL) {
              if(getval(z->c) == xA) {
                setval(z->c, yA);
              } else if(getval(z->c) == yA) {
                setval(z->c, xA);
              }
              if(getval(z->b) == xA) {
                setval(z->b, yA);
              } else if(getval(z->b) == yA) {
                setval(z->b, xA);
              }
            }
            tmp1C = bc_next(obj, tmp1C);
          }
        }
      }
    }

    x = (struct vm_top_t *)&obj->bc.buffer[nrbytes];
    if(is_op_and_math(gettype(x->type))) {
      tmp1 = nrbytes;
      tmp1C = -1;
      if(getval(x->c) >= 0) {
        tmp1C = tmp1;
        while((tmp1C = bc_before(tmp1C)) != -1) {
          struct vm_top_t *xC = (struct vm_top_t *)&obj->bc.buffer[tmp1C];

          if(getval(x->c) == getval(xC->a)) {
            break;
          }
        }
      }

      if(tmp1C != -1 && tmp1B >= limit) {
        tmp1B = bc_find_math_dep(obj, tmp1, getval(x->b));
        struct vm_top_t *xC = (struct vm_top_t *)&obj->bc.buffer[tmp1C];
        struct vm_top_t *xB = NULL;
        if(tmp1B != -1) {
          xB = (struct vm_top_t *)&obj->bc.buffer[tmp1B];
        }

        if(xB == NULL || gettype(xC->type) == OP_GETVAL || (((int8_t)getval(xB->b) >= 0)+((int8_t)getval(xB->c) >= 0) < ((int8_t)getval(xC->b) >= 0)+((int8_t)getval(xC->c) >= 0))) {
          while(tmp1C < tmp1) {
            struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[tmp1C];
            if(!is_op_and_math(gettype(x->type)) && gettype(x->type) != OP_GETVAL) {
              break;
            }

            if((tmp1C = bc_next(obj, tmp1C)) == -1) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d %d"), __FUNCTION__, __LINE__);
              exit(-1);
              /* LCOV_EXCL_STOP*/
            }

            if(tmp1C == tmp1 || tmp1C < limit) {
              break;
            }

            struct vm_top_t *y = (struct vm_top_t *)&obj->bc.buffer[tmp1C];
            int8_t xT = getval(x->type), xA = getval(x->a), xB = getval(x->b), xC = getval(x->c);
            int8_t yA = getval(y->a);

            setval(x->type, getval(y->type));
            setval(x->b, getval(y->b));
            setval(x->c, getval(y->c));

            setval(y->type, xT);
            setval(y->b, xB);
            setval(y->c, xC);

            tmp1B = limit;
            while(tmp1B != -1) {
              struct vm_top_t *z = (struct vm_top_t *)&obj->bc.buffer[tmp1B];

              if(gettype(z->type) != OP_GETVAL &&
                 gettype(z->type) != OP_CALL) {
                if(getval(z->c) == xA) {
                  setval(z->c, yA);
                } else if(getval(z->c) == yA) {
                  setval(z->c, xA);
                }
                if(getval(z->b) == xA) {
                  setval(z->b, yA);
                } else if(getval(z->b) == yA) {
                  setval(z->b, xA);
                }
              }
              tmp1B = bc_next(obj, tmp1B);
            }
          }
        }
      }
    }

    x = (struct vm_top_t *)&obj->bc.buffer[nrbytes];
    if(is_op_and_math(gettype(x->type))) {
      tmp1 = nrbytes;
      tmp1B = -1;

      if(getval(x->b) >= 0) {
        tmp1B = tmp1;
        while((tmp1B = bc_before(tmp1B)) != -1) {
          struct vm_top_t *xB = (struct vm_top_t *)&obj->bc.buffer[tmp1B];
          if(getval(x->b) == getval(xB->a)) {
            break;
          }
        }
      }

      if(tmp1B != -1 && tmp1B >= limit) {
        tmp1C = bc_find_math_dep(obj, tmp1, getval(x->c));
        struct vm_top_t *xB = (struct vm_top_t *)&obj->bc.buffer[tmp1B];
        struct vm_top_t *xC = NULL;
        if(tmp1C != -1) {
          xC = (struct vm_top_t *)&obj->bc.buffer[tmp1C];
        }

        if(xC == NULL || gettype(xB->type) == OP_GETVAL || (((int8_t)getval(xB->b) >= 0)+((int8_t)getval(xB->c) >= 0) < ((int8_t)getval(xC->b) >= 0)+((int8_t)getval(xC->c) >= 0))) {
          while(tmp1B < tmp1) {
            struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[tmp1B];
            if(!is_op_and_math(gettype(x->type)) && gettype(x->type) != OP_GETVAL) {
              break;
            }

            if((tmp1B = bc_next(obj, tmp1B)) == -1) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d %d"), __FUNCTION__, __LINE__);
              exit(-1);
              /* LCOV_EXCL_STOP*/
            }

            if(tmp1B == tmp1 || tmp1B < limit) {
              break;
            }

            struct vm_top_t *y = (struct vm_top_t *)&obj->bc.buffer[tmp1B];
            int8_t xT = getval(x->type), xA = getval(x->a), xB = getval(x->b), xC = getval(x->c);
            int8_t yA = getval(y->a);

            setval(x->type, getval(y->type));
            setval(x->b, getval(y->b));
            setval(x->c, getval(y->c));

            setval(y->type, xT);
            setval(y->b, xB);
            setval(y->c, xC);

            tmp1C = limit;
            while(tmp1C != -1) {
              struct vm_top_t *z = (struct vm_top_t *)&obj->bc.buffer[tmp1C];

              if(gettype(z->type) != OP_GETVAL &&
                 gettype(z->type) != OP_CALL) {
                if(getval(z->c) == xA) {
                  setval(z->c, yA);
                } else if(getval(z->c) == yA) {
                  setval(z->c, xA);
                }
                if(getval(z->b) == xA) {
                  setval(z->b, yA);
                } else if(getval(z->b) == yA) {
                  setval(z->b, xA);
                }
              }
              tmp1C = bc_next(obj, tmp1C);
            }
          }
        }
      }
    }

    nrbytes = bc_before(nrbytes);
  }
}

static int32_t bc_parse_math_order(char **text, struct rules_t *obj, uint16_t *pos, uint8_t *cnt) {
  uint16_t start = 0, len = 0;
  int32_t first = 0, step = 0, bc_in = 0, heap_in = 0, limit = 0;
  int16_t d = 0;
  uint8_t a = 0, b = 0, c = 0;

  if(lexer_peek(text, (*pos), &a, &start, &len) < 0) {
    /* LCOV_EXCL_START*/
    logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
    return -1;
    /* LCOV_EXCL_STOP*/
  }

  switch(a) {
    case VPTR: {
      /*
       * This is the only way to retrieve a VPTR
       * in the (*text) variable.
       */

      uint16_t p = ((getval((*text)[start+sizeof(uint8_t)]) & 0xFF) << 8);
      p |= getval((*text)[start+sizeof(uint8_t)+sizeof(uint8_t)]) & 0xFF;

      struct vm_top_t *op = (struct vm_top_t *)&obj->bc.buffer[p];
      first = p;
      heap_in = (int8_t)getval(op->a);
      bc_in = p;

      (*pos)++;
    } break;
    case TVAR:
    case VNULL:
    case VINTEGER:
    case VFLOAT:
    case TNUMBER1:
    case TNUMBER2:
    case TNUMBER3: {
      if(a == TVAR) {
        uint16_t x = varstack_add(text, start+1, len, 1);
        heap_in = ++(*cnt);
        bc_in = first = bc_parent(obj, OP_GETVAL, heap_in, x/sizeof(struct vm_vchar_t), 0);
      } else {
        if(a == VNULL) {
          heap_in = vm_heap_push(obj, a, text, 1, 0, 0);
        } else {
          heap_in = vm_heap_push(obj, a, text, start, len, 0);
        }
        heap_in = vm_val_posr(heap_in);
        bc_in = heap_in;
      }
      (*pos)++;
    } break;
    default: {
      logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
      return -1;
    }
  }

  while(1) {
    if(lexer_peek(text, (*pos), &b, &start, &len) < 0 || b != TOPERATOR) {
      break;
    }
    uint8_t idx = getval((*text)[start+1]);

    /* LCOV_EXCL_START*/
    if(idx > nr_rule_operators) {
      logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
      return -1;
    }
    /* LCOV_EXCL_STOP*/

    (*pos)++;
    if(lexer_peek(text, (*pos), &c, &start, &len) >= 0) {
      switch(c) {
        case VPTR: {
          /*
           * This is the only way to retrieve a VPTR
           * in the (*text) variable.
           */

          uint16_t p = ((getval((*text)[start+sizeof(uint8_t)]) & 0xFF) << 8);
          p |= getval((*text)[start+sizeof(uint8_t)+sizeof(uint8_t)]) & 0xFF;

          struct vm_top_t *op = (struct vm_top_t *)&obj->bc.buffer[p];
          d = (int8_t)getval(op->a);
          b = p;

          (*pos)++;
        } break;
        case TVAR:
        case VNULL:
        case TSTRING:
        case TNUMBER1:
        case TNUMBER2:
        case TNUMBER3:
        case VINTEGER:
        case VFLOAT: {
          (*pos)++;
          uint16_t a = 0;
          if(c == TVAR) {
            a = varstack_add(text, start+1, len, 1);
            b = ++(*cnt);
            bc_parent(obj, OP_GETVAL, b, a/sizeof(struct vm_vchar_t), 0);
            d = b;
          } else {
            if(c == VNULL) {
              d = vm_heap_push(obj, c, text, 1, 0, 0);
            } else {
              d = vm_heap_push(obj, c, text, start, len, 0);
            }
            d = vm_val_posr(d);
          }
        } break;
        default: {
          logprintf_P(F("ERROR: Expected a parenthesis block, function, number or variable"));
          return -1;
        } break;
      }
    }

    step = bc_parent(obj, rule_operators[idx].opcode, ++(*cnt), heap_in, d);

    if(*cnt > (INT8_MAX/2)) {
      logprintf_P(F("ERROR: Too many stacked conditions"));
      return -1;
    }

    uint16_t nrbytes = getval(obj->bc.nrbytes);
    while(nrbytes > 0) {
      struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[nrbytes];
      if(getval(x->a) == 1) {
        limit = nrbytes;
        break;
      }
      nrbytes = bc_before(nrbytes);
    }

    if(step >= 0 && bc_in >= 0 && bc_in >= first && step >= first) {
      int16_t a_is_op = -1, b_is_op = -1, i = 0;
      uint8_t a_type = gettype(obj->bc.buffer[step]);
      uint8_t b_type = gettype(obj->bc.buffer[bc_in]);
      uint8_t a_immu = get_group(obj->bc.buffer[step]);
      uint8_t b_immu = get_group(obj->bc.buffer[bc_in]);

      if(is_op_and_math(b_type) && is_op_and_math(a_type) && a_immu == 0 && b_immu == 0) {
        for(i=0;i<nr_rule_operators;i++) {
          if(a_type == rule_operators[i].opcode) {
            a_is_op = i;
            break;
          }
        }

        for(i=0;i<nr_rule_operators;i++) {
          if(b_type == rule_operators[i].opcode) {
            b_is_op = i;
            break;
          }
        }

        uint8_t b_p = rule_operators[b_is_op].precedence;
        uint8_t a_p = rule_operators[a_is_op].precedence;
        uint8_t a_a = rule_operators[a_is_op].associativity;

        if(a_p > b_p || (b_p == a_p && a_a == 2)) {
          int32_t tmp1 = bc_in, tmp2 = step, tmp2B = -1;

          if(a_a == 1) {
            tmp1 = step, tmp2 = step;

            tmp1 = bc_before(tmp2);
            while(tmp2 >= limit && tmp1 >= limit) {

              struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[tmp2];

              if(gettype(x->type) != OP_GETVAL) {
                int8_t a = (int8_t)getval(x->type), d = (int8_t)getval(x->c);

                tmp2B = bc_find_math_dep(obj, tmp2, getval(x->b));
                if(tmp2B == -1) {
                  break;
                }

                struct vm_top_t *z = (struct vm_top_t *)&obj->bc.buffer[tmp2B];
                if(is_op_and_math(gettype(x->type)) && is_op_and_math(gettype(z->type))) {
                  for(i=0;i<nr_rule_operators;i++) {
                    if(gettype(x->type) == rule_operators[i].opcode) {
                      a_is_op = i;
                      break;
                    }
                  }

                  for(i=0;i<nr_rule_operators;i++) {
                    if(gettype(z->type) == rule_operators[i].opcode) {
                      b_is_op = i;
                      break;
                    }
                  }

                  b_p = rule_operators[b_is_op].precedence;
                  a_p = rule_operators[a_is_op].precedence;
                  a_a = rule_operators[a_is_op].associativity;

                  if(get_group(obj->bc.buffer[tmp2B]) != 0) {
                    break;
                  }

                  if(!(a_p > b_p || (b_p == a_p && a_a == 2))) {
                    break;
                  }

                  if(gettype(z->type) == OP_GETVAL) {
                    break;
                  }

                  setval(x->type, getval(z->type));
                  setval(x->b, getval(z->b));
                  setval(x->c, getval(z->a));

                  setval(z->type, a);
                  setval(z->b, getval(z->c));
                  setval(z->c, d);

                  bc_math_move_closest(obj, limit);

                  tmp2 = tmp2B;
                } else {
                  tmp2 = tmp1;
                }
              } else {
                tmp2 = tmp1;
              }
              tmp1 = bc_before(tmp2);
            }
          } else {
            tmp1 = step, tmp2 = step;
            tmp1 = bc_before(tmp2);
            while(tmp2 >= limit && tmp1 >= limit) {
              struct vm_top_t *x = (struct vm_top_t *)&obj->bc.buffer[tmp2];

              if(is_op_and_math(gettype(x->type))) {
                for(i=0;i<nr_rule_operators;i++) {
                  if(gettype(x->type) == rule_operators[i].opcode) {
                    a_is_op = i;
                    break;
                  }
                }

                a_a = rule_operators[a_is_op].associativity;
                if(a_a == 2) {
                  int8_t a = (int8_t)getval(x->type), d = (int8_t)getval(x->c);

                  tmp2B = bc_find_math_dep(obj, tmp2, getval(x->b));
                  if(tmp2B == -1) {
                    tmp2 = tmp1;
                    tmp1 = bc_before(tmp2);
                    continue;
                  }

                  struct vm_top_t *z = (struct vm_top_t *)&obj->bc.buffer[tmp2B];

                  if(get_group(obj->bc.buffer[tmp2B]) != 0) {
                    tmp2 = tmp1;
                    tmp1 = bc_before(tmp2);
                    continue;
                  }

                  if(getval(z->type) == OP_GETVAL) {
                    break;
                  }

                  setval(x->type, getval(z->type));
                  setval(x->b, getval(z->b));
                  setval(x->c, getval(z->a));

                  setval(z->type, a);
                  setval(z->b, getval(z->c));
                  setval(z->c, d);

                  bc_math_move_closest(obj, limit);
                } else {
                  tmp2 = tmp1;
                }
              } else {
                tmp2 = tmp1;
              }
              tmp1 = bc_before(tmp2);
            }
          }
        }
      }
    }
    heap_in = (*cnt);
    bc_in = step;
  }

  return step;
}

uint16_t lexer_clear(struct rules_t *obj, char **text, uint16_t start, uint16_t end) {
  uint16_t i = 0, nr = 0, x = 0, y = 0;
  uint16_t pos = 0;
  uint8_t loop = 1, type = 0;


  while(loop) {
    type = getval((*text)[i]);
    if(nr == start) {
      y = i;
    }
    switch(type) {
      case TELSEIF:
      case TIF:{
        if(nr >= start && nr <= end) {
          x += 2;
        }
        i += 2;
      } break;
      case VNULL:
      case TSEMICOLON:
      case TEND:
      case TASSIGN:
      case RPAREN:
      case TCOMMA:
      case LPAREN:
      case TELSE:
      case TTHEN: {
        if(nr >= start && nr <= end) {
          x += 1;
        }
        i += 1;
      } break;
      case TNUMBER1: {
        if(nr >= start && nr <= end) {
          x += 2;
        }
        i += 2;
      } break;
      case TNUMBER2: {
        if(nr >= start && nr <= end) {
          x += 3;
        }
        i += 3;
      } break;
      case TNUMBER3: {
        if(nr >= start && nr <= end) {
          x += 4;
        }
        i += 4;
      } break;
      case VINTEGER: {
        /*
         * Sizeof should reflect
         * sizeof of vm_vinteger_t
         * value
         */
        if(nr >= start && nr <= end) {
          x += 1+sizeof(uint32_t);
        }
        i += 1+sizeof(uint32_t);
      } break;
      case VFLOAT: {
        /*
         * sizeof should reflect
         * sizeof of vm_vfloat_t
         * value
         */
        if(nr >= start && nr <= end) {
          x += 1+sizeof(uint32_t);
        }
        i += 1+sizeof(float);
      } break;
      case VPTR: {
        if(nr >= start && nr <= end) {
          x += sizeof(struct vm_vptr_t);
        }
        i += sizeof(struct vm_vptr_t);
      } break;
      case TFUNCTION:
      case TOPERATOR: {
        if(nr >= start && nr <= end) {
          x += 2;
        }
        i += 2;
      } break;
      case TEOF: {
        if(nr >= start && nr <= end) {
          x += 1;
        }
        i += 1;
        loop = 0;
      } break;
      case TSTRING:
      case TEVENT:
      case TVAR: {
        if(nr >= start && nr <= end) {
          x++;
        }
        i++;
        uint8_t current = getval((*text)[i]);

        /*
         * Consider tokens above 31 as regular characters
         */
        while(current >= 32 && current <= 128) {
          if(nr >= start && nr <= end) {
            ++x;
          }
          ++i;
          current = getval((*text)[i]);
        }
      } break;
      /* LCOV_EXCL_START*/
      default: {
        logprintf_P(F("FATAL: Internal error in %s #%d %d"), __FUNCTION__, __LINE__);
      } break;
      /* LCOV_EXCL_STOP*/
    }
    nr++;
  }
  /*
   * Check if we can move this upwards instead of downwards
   */
  for(pos=0;pos<i-(y+x);pos++) {
    setval((*text)[y+sizeof(struct vm_vptr_t)+pos], getval((*text)[y+x+pos]));
  }
  for(pos=0;pos<sizeof(struct vm_vptr_t);pos++) {
    setval((*text)[y+pos], 0);
  }

  {
    int32_t tmp = bc_before(getval(obj->bc.nrbytes));
    if(tmp >= 0) {
      /*
       * This is the only way to store a VPTR
       * in the (*text) variable.
       */
      setval((*text)[y], VPTR);
      y += sizeof(uint8_t);
      setval((*text)[y], (tmp >> 8) & 0xFF);
      y += sizeof(uint8_t);
      setval((*text)[y], tmp & 0xFF);
    } else {
      /* LCOV_EXCL_START*/
        logprintf_P(F("FATAL: Internal error in %s #%d %d"), __FUNCTION__, __LINE__);
      /* LCOV_EXCL_STOP*/
    }
  }

  return pos;
}

static int16_t rule_create(char **text, struct rules_t *obj) {
  int32_t rewind = -1, in_child = -1;
  uint16_t start = 0, len = 0, pos = 0, ret = 0, val = 0;
  uint16_t loop = 1, paren[2] = { 0 };
  uint8_t type = 0, go = 0, mathcnt = 0, depth = 1;

#ifdef DEBUG
  {
    uint16_t pos1 = 0, start1 = 0, len1 = 0;
    uint8_t type1 = 0;
    while(lexer_peek(text, pos1, &type1, &start1, &len1) >= 0) {
      printf("%d %s %d\n", pos1, token_names[type1].name, len1);
      pos1++;
    }
  }
#endif

  /* LCOV_EXCL_START*/
  if(lexer_peek(text, 0, &type, &start, &len) < 0) {
    logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
    return -1;
  }
  /* LCOV_EXCL_STOP*/

  if(type != TIF && type != TEVENT) {
    logprintf_P(F("ERROR: Expected an 'if' or an 'on' statement"));
    return -1;
  }

  go = type;

  {
    while(lexer_peek(text, pos, &type, &start, &len) >= 0) {
      if(type == TVAR) {
        varstack_add(text, start+1, len, 1);
      } else if(type == TEVENT) {
        varstack_add(text, start+1, len, 1);
      }
      pos++;
    }
  }
  pos = 0;

  while(loop) {
#ifdef ESP8266
    ESP.wdtFeed();
#endif
#ifdef DEBUG
    printf("%s %d %d %d %d %s\n", __FUNCTION__, __LINE__, depth, pos, getval(obj->bc.nrbytes), token_names[go].name);
#endif

    switch(go) {
      case TTHEN:
      case TELSE:
      case TELSEIF: {
        uint8_t a = 0;
        if(go == TTHEN) {
          if(pos == 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
          uint16_t tmp = pos-1;
          while(tmp > 0 && lexer_peek(text, tmp, &a, &start, &len) > 0 && a != TIF && a != TELSEIF) {
            tmp--;
          }
        }
        if(a != TELSEIF) {
          int32_t lastjmp = getval(obj->bc.nrbytes);
          if(lastjmp > 0) {
            struct vm_top_t *jmp = NULL;

            while((lastjmp = bc_before(lastjmp))) {
              if(gettype(obj->bc.buffer[lastjmp]) == OP_JMP) {
                jmp = (struct vm_top_t *)&obj->bc.buffer[lastjmp];
                if((int8_t)getval(jmp->b) == depth) {
                  int16_t step = getval(obj->bc.nrbytes);
                  setval(jmp->a, ((step-lastjmp)/sizeof(struct vm_top_t))+1);
                  setval(jmp->b, 0);
                  break;
                }
              }
            }
          }
        }

        mathcnt = 0;
        if(ret != TEVENT) {
          bc_parent(obj, OP_JMP, 0, depth, 0);
        }

        if(lexer_peek(text, pos, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }
        switch(type) {
          case TELSEIF: {
            if(go == TTHEN) {
              logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
              return -1;
            }
            go = TIF;
            ret = go;
          } break;
          case TEVENT:
          case TFUNCTION:
          case TIF:
          case TVAR: {
            ret = go;
            go = type;
          } break;
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
        }
      } break;
      case TEVENT: {
        if(pos == 0) {
          if(lexer_peek(text, pos, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }

          uint16_t idx = varstack_add(text, start+1, len, 1);
          struct vm_vchar_t *chr = (struct vm_vchar_t *)&varstack->buffer[idx];
          obj->name = (char *)chr->value;

          if(lexer_peek(text, pos+1, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
          switch(type) {
            case LPAREN: {
              in_child = pos;
              rewind = pos;
              go = TFUNCTION;
              ret = TEVENT;
              pos++;
            } break;
            case TEVENT:
            case TTHEN:
            case TEND: {
              go = TTHEN;
              ret = TEVENT;
              pos += 2;
            } break;
            /* LCOV_EXCL_START*/
            default: {
              logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
              return -1;
            } break;
            /* LCOV_EXCL_STOP*/
          }
        } else {
          if(lexer_peek(text, pos, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
          switch(type) {
            case LPAREN:
            case RPAREN:
            case TCOMMA:
            case TEND:
            case TEVENT:
            case TOPERATOR:
            case TTHEN: {
              go = TFUNCTION;
              ret = TEVENT;
            } break;
            case TSEMICOLON: {
              go = TIF;
              ret = TEVENT;
            } break;
            /* LCOV_EXCL_START*/
            default: {
              logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
              return -1;
            } break;
           /* LCOV_EXCL_STOP*/
          }
        }
      } break;
      case TIF: {
        /*
         * Check if we've entered a nested if/else block
         */
        if(go == TIF && pos > 0) {
          uint16_t tmp = pos-1;

          uint8_t a = 0;
          while(tmp > 0 && lexer_peek(text, tmp, &a, &start, &len) > 0 && a != TIF && a != TEND && a != TTHEN) {
            tmp--;
          }
          if(a == TIF) {
            depth++;
          }
        }

        if(lexer_peek(text, pos, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }

        switch(type) {
          case TELSEIF:
          case TEND:
          case TIF:
          case TSEMICOLON:
          case TTHEN:
          case TFUNCTION:
          case TVAR: {
          } break;
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
        }

        uint16_t tmp = pos;
        if(type != TSEMICOLON) {
          paren[0] = getval(obj->bc.nrbytes);
          int32_t pos1 = bc_whatfirst(text, pos, TTHEN);
          if(pos1 == -1) {
            return -1;
          } else {
            pos = pos1;
          }
        }
        if(type != TSEMICOLON && pos > 0) {
          in_child = pos;
          if(lexer_peek(text, pos, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
          switch(type) {
            case TFUNCTION:
            case LPAREN: {
              go = type;
            } break;
            /* LCOV_EXCL_START*/
            default: {
              logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
              return -1;
            } break;
            /* LCOV_EXCL_STOP*/
          }
        } else {
          pos = tmp;

          if(lexer_peek(text, pos+2, &type, &start, &len) >= 0 && type == TOPERATOR) {
            pos++;
            go = TOPERATOR;
            ret = TIF;
            // continue;
          } else if(lexer_peek(text, pos, &type, &start, &len) <= 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          } else {
            switch(ret) {
              /*
               * In case of single token IF conditions:
               * if 3 then ...
               * if $a then ...
               * if max(0) then ...
               */
              case TFUNCTION:
              case TNUMBER: {
                /*
                 * Make sure we are inside IF and TELSEIF
                 */
                uint16_t tmp = pos;
                uint8_t a = 0;
                while(tmp > 0 && lexer_peek(text, --tmp, &a, &start, &len) >= 0) {
                  if(a == TTHEN || a == TELSE || a == TELSEIF || a == TIF) {
                    break;
                  }
                }
                if(a == TIF || a == TELSEIF) {
                  bc_parent(obj, OP_TEST, val, 0, 0);
                }
              } break;
            }
          }
        }

        switch(type) {
          case TSEMICOLON: {
            uint16_t a  = bc_parent(obj, OP_CLEAR, 0, 0, 0);
            {
              struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[bc_before(a)];
              setval(node->a, 0);
            }
            pos++;

            if(lexer_peek(text, pos, &type, &start, &len) <= 0) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }
            switch(type) {
              case TELSE: {
                go = TELSE;
                ret = TIF;
                pos++;
                continue;
              } break;
              case TELSEIF:
              case TIF:
              case TEVENT:
              case TFUNCTION:
              case LPAREN: {
                /*
                 * A call/expression statement is followed by a sibling
                 * statement (nested if, event, or function call) rather
                 * than closing the block. Reset mathcnt here too, for the
                 * same reason as the TEND case below (#894): otherwise it
                 * leaks into the next statement's slot allocation.
                 */
                mathcnt = 0;
                go = type;
                ret = TIF;
                continue;
              } break;
              case TEND: {
                if(lexer_peek(text, pos-1, &type, &start, &len) <= 0) {
                  /* LCOV_EXCL_START*/
                  logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                  return -1;
                  /* LCOV_EXCL_STOP*/
                }
                if(type != TSEMICOLON) {
                  /* LCOV_EXCL_START*/
                  logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
                  return -1;
                  /* LCOV_EXCL_STOP*/
                }
                /*
                 * A call/expression statement closes this if-block. Reset the
                 * temporary slot counter so a following sibling if-condition
                 * starts fresh. Otherwise mathcnt leaks across the block and
                 * bc_parse_math_order's backward slot search overshoots into
                 * this block, relocating its trailing logical operator (#894).
                 */
                mathcnt = 0;
                go = TEND;
                ret = TIF;
                continue;
              } break;
              case TVAR: {
                go = TVAR;
                ret = TIF;
                continue;
              } break;
              /* LCOV_EXCL_START*/
              default: {
                logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
                return -1;
              } break;
              /* LCOV_EXCL_STOP*/
            }
          } break;
          case TVAR: {
            go = TVAR;
            ret = TIF;
            continue;
          } break;
          case TEVENT:
          case TFUNCTION:
          case LPAREN: {
            rewind = tmp;
            go = type;
            ret = TIF;
            continue;
          } break;
          case TTHEN: {
            pos++;
            ret = go;
            go = type;
            continue;
          } break;
          case TELSEIF:
          case TIF:
          case TOPERATOR: {
          } break;
          /* LCOV_EXCL_START*/
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
          /* LCOV_EXCL_STOP*/
        }
        if(lexer_peek(text, pos+1, &type, &start, &len) <= 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }
        switch(type) {
          case VPTR: {
            uint16_t p = ((getval((*text)[start+sizeof(uint8_t)]) & 0xFF) << 8);
            p |= getval((*text)[start+sizeof(uint8_t)+sizeof(uint8_t)]) & 0xFF;
            struct vm_top_t *op = (struct vm_top_t *)&obj->bc.buffer[p];

            if(gettype(op->type) == OP_CALL) {
              pos+=2;
              ret = TFUNCTION;
              go = TIF;
            } else {
              if(lexer_peek(text, pos+2, &type, &start, &len) <= 0) {
                /* LCOV_EXCL_START*/
                logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                return -1;
                /* LCOV_EXCL_STOP*/
              }

              if(type == TTHEN) {
                pos+=3;
                ret = go;
                go = type;
              } else {
                /* LCOV_EXCL_START*/
                logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
                return -1;
                /* LCOV_EXCL_STOP*/
              }
            }
          } break;
          case TVAR: {
            pos++;
            go = TVAR;
            ret = TIF;
            goto TVAR_AS_VALUE;
          } break;
          case VNULL:
          case VINTEGER:
          case VFLOAT:
          case TSTRING:
          case TNUMBER1:
          case TNUMBER2:
          case TNUMBER3:{
            pos++;
            go = TNUMBER1;
            ret = TIF;
          } break;
          case TTHEN:
          case TASSIGN:
          case TEOF:
          case TEND:
          case TOPERATOR: {
          } break;
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
        }
      } break;
      case VNULL:
      case VINTEGER:
      case VFLOAT:
      case TSTRING:
      case TNUMBER1:
      case TNUMBER2:
      case TNUMBER3:{
      TVAR_AS_VALUE:
        if(lexer_peek(text, pos+1, &type, &start, &len) >= 0 && type == TOPERATOR) {
        } else if(lexer_peek(text, pos, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }

        if(type == TOPERATOR) {
          go = TOPERATOR;
          ret = TNUMBER1;
        } else if(type == RPAREN) {
          if(in_child == -1) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
          if(lexer_peek(text, in_child, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }

          if(ret == TNUMBER || ret == TOPERATOR || ret == TSTRING) {
            if(type == TFUNCTION || (type == TEVENT && in_child > 0)) {
              if(ret == TSTRING) {
                bc_parent(obj, OP_PUSH, val, 0, 1);
              } else {
                bc_parent(obj, OP_PUSH, val, 0, 0);
              }
            }
          }

          /*
           * Don't go back to `in_child` if we are
           * inside a simple hook set: if (3 == 3) then
           *                              ^      ^
           * instead of: $a = max(3, 3)
           * where we can return to TFUNCTION
           */
          if(type == LPAREN) {
            go = RPAREN;
            ret = LPAREN;
          } else {
            ret = go;
            go = type;
          }
        } else {
          go = ret;
          ret = TNUMBER;

          int32_t a = 0;
          if(in_child > -1) {
            if(lexer_peek(text, in_child, &type, &start, &len) < 0) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }
            a = type;
          }

          if(lexer_peek(text, pos, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }

          if(a == TEVENT && in_child == 0) {
            if(type == TVAR) {
              uint16_t c = varstack_add(text, start+1, len, 1);
              uint16_t d = bc_parent(obj, OP_SETVAL, c/sizeof(struct vm_vchar_t), 0, 0);
              int32_t e = bc_before(d);
              if(e >= 0 && gettype(obj->bc.buffer[e]) != OP_GETVAL) {
                mathcnt = 0;
              }
            } else {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }
          } else {
            if(type == TSTRING) {
              uint16_t c = varstack_add(text, start+1, len, 1);
              a = (c/sizeof(struct vm_vchar_t))+1;
              ret = TSTRING;
            } else if(type == TVAR) {
              uint16_t c = varstack_add(text, start+1, len, 1);
              // a = vm_heap_push(obj, VNULL, NULL, 0, 0, 0);
              // a = vm_val_posr(a);
              a = ++mathcnt;
              bc_parent(obj, OP_GETVAL, a, c/sizeof(struct vm_vchar_t), 0);
            } else {
              if(type == VNULL) {
                a = vm_heap_push(obj, type, text, 1, 0, 0);
              } else {
                a = vm_heap_push(obj, type, text, start, len, 0);
              }
              a = vm_val_posr(a);
            }
          }

          if(lexer_peek(text, pos+1, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
          switch(type) {
            case TCOMMA:
            case TSEMICOLON:
            case TTHEN:
            case RPAREN: {
            } break;
            default: {
              logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
              return -1;
            } break;
          }

          val = a;
          pos++;
        }
      } break;
      case TASSIGN: {
        uint16_t tmp = pos;
        paren[0] = getval(obj->bc.nrbytes);
        int32_t pos1 = bc_whatfirst(text, pos, TSEMICOLON);
        if(pos1 == -1) {
          return -1;
        } else {
          pos = pos1;
        }
        if(pos > 0) {
          in_child = pos;
          if(lexer_peek(text, pos, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
          go = type;
        } else {
          pos = tmp;
          if(lexer_peek(text, pos+1, &type, &start, &len) >= 0 && type == LPAREN) {
          } else if(lexer_peek(text, pos+2, &type, &start, &len) >= 0 && type == TOPERATOR) {
            pos++;
          } else if(lexer_peek(text, pos, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
        }

        switch(type) {
          case TASSIGN: {
            pos++;
          } break;
          case TFUNCTION: {
            rewind = tmp;
            ret = go;
            go = type;
            pos++;
          } break;
          case TSEMICOLON: {
            ret = go;
            go = TVAR;
          } break;
          case VPTR: {
            pos++;
            go = TVAR;
            ret = TASSIGN;
          } break;
          case LPAREN: {
            rewind = tmp;
          } break;
          case TVAR: {
            ret = go;
            go = type;
            goto TVAR_AS_VALUE;
          }
          case TSTRING:
          case TNUMBER1:
          case TNUMBER2:
          case TNUMBER3:
          case TOPERATOR:
          case VFLOAT:
          case VINTEGER:
          case VNULL: {
            ret = go;
            go = type;
          } break;
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
        }
      } break;
      case TVAR: {
        if(lexer_peek(text, pos, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }

        switch(type) {
          case TVAR: {
            pos++;
          } break;
          case TASSIGN: {
            go = TASSIGN;
            ret = TVAR;
          } break;
          case TSEMICOLON: {
            uint16_t tmp = pos;
            while(lexer_peek(text, tmp, &type, &start, &len) > 0 && type != TASSIGN) {
              tmp--;
            }
            if(lexer_peek(text, tmp-1, &type, &start, &len) < 0) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }
            if(type != TVAR) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }

            uint16_t a = varstack_add(text, start+1, len, 1);

            uint16_t b = bc_parent(obj, OP_SETVAL, a/sizeof(struct vm_vchar_t), val, 0);
            int32_t c = bc_before(b);
            if(c >= 0 && gettype(obj->bc.buffer[c]) != OP_GETVAL) {
              mathcnt = 0;
            }
            pos++;

            if(lexer_peek(text, pos, &type, &start, &len) < 0) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }
            if(type == TELSE) {
              pos++;
            }
            ret = go;
            go = type;
          } break;
          /* LCOV_EXCL_START*/
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
          /* LCOV_EXCL_STOP*/
        }

      } break;
      case TFUNCTION: {
        if(lexer_peek(text, pos, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }
        switch(type) {
          case TEVENT:
          case TFUNCTION: {
            if(rewind == -1) {
              rewind = pos;
            }
            in_child = pos;
            paren[0] = getval(obj->bc.nrbytes);
            int32_t pos1 = bc_whatfirst(text, pos, TSEMICOLON);
            if(pos1 == -1) {
              return -1;
            } else {
              pos = pos1;
            }
            if(pos > 0) {
              in_child = pos;
              if(lexer_peek(text, pos, &type, &start, &len) < 0) {
                /* LCOV_EXCL_START*/
                logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                return -1;
                /* LCOV_EXCL_STOP*/
              }
              pos++;
              go = type;
              ret = TFUNCTION;
            } else {
              logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
              return -1;
            }
          } break;
          case RPAREN: {
            if(in_child == -1) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }
            if(lexer_peek(text, in_child, &type, &start, &len) < 0) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }

            if(type == TEVENT) {
              if(in_child > 0) {
                uint16_t idx = varstack_add(text, start+1, len, 1);
                bc_parent(obj, OP_CALL, ++mathcnt, idx/sizeof(struct vm_vchar_t), 1);
              }
            } else {
              uint8_t var = getval((*text)[start+1]);
              bc_parent(obj, OP_CALL, ++mathcnt, var, 0);
              val = mathcnt;
            }

            go = RPAREN;
            ret = TFUNCTION;
          } break;
          case TSEMICOLON: {
            go = TVAR;
            ret = TFUNCTION;
          } break;
          case LPAREN: {
            go = LPAREN;
            ret = TFUNCTION;
          } break;
          case TCOMMA: {
            pos++;
            go = LPAREN;
            ret = TFUNCTION;
          } break;
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
        }
      } break;
      case TOPERATOR: {
        /*
         * The return value is the positition
         * of the root operator
         */
        int32_t step = bc_parse_math_order(text, obj, &pos, &mathcnt);
        if(step == -1) {
          return -1;
        }
        struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[step];
        val = getval(node->a);

        go = ret;
        ret = TOPERATOR;
      } break;
      case RPAREN: {
        if(in_child == -1) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }

        if(lexer_peek(text, pos+1, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }

        switch(type) {
          case TCOMMA:
          case TSEMICOLON:
          case TOPERATOR:
          case RPAREN:
          case TTHEN: {
          } break;
          default: {
            /* LCOV_EXCL_START*/
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          } break;
        }

        lexer_clear(obj, text, in_child, pos);
        paren[1] = getval(obj->bc.nrbytes);
        bc_group(obj, paren[0], paren[1]);

        pos = rewind;
        rewind = -1;
        in_child = -1;
        if(lexer_peek(text, pos, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }
        switch(type) {
          case TELSEIF: {
            ret = go;
            go = TIF;
          } break;
          case TASSIGN:
          case TEVENT:
          case TFUNCTION:
          case TIF:
          case TVAR:
          case VPTR: {
            ret = go;
            go = type;
          } break;
          /* LCOV_EXCL_START*/
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
          /* LCOV_EXCL_STOP*/
        }
      } break;
      case VPTR: {
        if(lexer_peek(text, pos, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }
        /*
         * This is the only way to retrieve a VPTR
         * in the (*text) variable.
         */
        uint16_t p = ((getval((*text)[start+sizeof(uint8_t)]) & 0xFF) << 8);
        p |= getval((*text)[start+sizeof(uint8_t)+sizeof(uint8_t)]) & 0xFF;

        struct vm_top_t *op = (struct vm_top_t *)&obj->bc.buffer[p];

        if(in_child == -1) {
          if(lexer_peek(text, 0, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
        } else {
          if(lexer_peek(text, in_child, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }
        }

        switch(type) {
          case LPAREN:
          case TEVENT:
          case TFUNCTION:
          case TIF:
          case VPTR:
          break;
          /* LCOV_EXCL_START*/
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
          /* LCOV_EXCL_STOP*/
        }

        if(type == VPTR && in_child == -1) {
          if(pos == 0) {
            go = TIF;
            ret = LPAREN;
            pos+=2;
          } else {
            go = TIF;
            ret = LPAREN;
            pos++;
          }
        } else if(type == TFUNCTION || type == TEVENT) {
          ret = LPAREN;
          go = type;
          pos++;
        } else if(gettype(op->type) == OP_CALL) {
          pos++;
          ret = TFUNCTION;
          go = type;
        } else {
          pos++;
          ret = LPAREN;
          go = type;
        }
      } break;
      case LPAREN: {
        if(lexer_peek(text, pos+2, &type, &start, &len) >= 0 && type == TOPERATOR) {
          uint8_t a = type;
          /*
           * TOPERATOR is outside of LPAREN / RPAREN set
           * ((1 + 2) / (2 + 3)) + 2
           *                     ^
           */
          if(lexer_peek(text, pos+1, &type, &start, &len) >= 0 && type == RPAREN) {
            /*
             * Check if this is a single argument function call:
             * $a = max(5);
             */
            if(lexer_peek(text, pos-1, &type, &start, &len) >= 0 && type == LPAREN) {
              if(lexer_peek(text, pos, &type, &start, &len) < 0) {
                /* LCOV_EXCL_START*/
                logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                return -1;
                /* LCOV_EXCL_STOP*/
              }
            } else {
              /*
               * If all previous isn't the case, continue normally
               */
              if(lexer_peek(text, pos, &type, &start, &len) < 0) {
                /* LCOV_EXCL_START*/
                logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                return -1;
                /* LCOV_EXCL_STOP*/
              }
            }
          } else {
            if(lexer_peek(text, pos, &type, &start, &len) >= 0 && type == TCOMMA) {
              bc_parent(obj, OP_PUSH, val, 0, ret == TSTRING);
            }
            type = a;
            pos++;
          }
        } else if(lexer_peek(text, pos, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }

        switch(type) {
          case TOPERATOR: {
            go = TOPERATOR;
            ret = LPAREN;
          } break;
          case RPAREN: {
            if(in_child == -1) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }
            if(lexer_peek(text, in_child, &type, &start, &len) < 0) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }

            if(ret == TNUMBER || ret == TOPERATOR || ret == TSTRING) {
              if(type == TFUNCTION || (type == TEVENT && in_child > 0)) {
                if(ret == TSTRING) {
                  bc_parent(obj, OP_PUSH, val, 0, 1);
                } else {
                  bc_parent(obj, OP_PUSH, val, 0, 0);
                }
              }
            }

            /*
             * Don't go back to `in_child` if we are
             * inside a simple hook set: if (3 == 3) then
             *                              ^      ^
             * instead of: $a = max(3, 3)
             * where we can return to TFUNCTION
             */
            if(type == LPAREN) {
              go = RPAREN;
              ret = LPAREN;
            } else {
              ret = go;
              go = type;
            }
          } break;
          case TVAR: {
            ret = go;
            go = type;
            goto TVAR_AS_VALUE;
          } break;
          case LPAREN: {
            pos++;
            if(lexer_peek(text, pos, &type, &start, &len) < 0) {
              /* LCOV_EXCL_START*/
              logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
              return -1;
              /* LCOV_EXCL_STOP*/
            }
            if(type == TTHEN) {
              logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
              return -1;
            }
          } break;
          case VNULL:
          case VINTEGER:
          case VFLOAT:
          case TSTRING:
          case TNUMBER1:
          case TNUMBER2:
          case TNUMBER3: {
            ret = LPAREN;
            go = type;
          } break;
          case TCOMMA: {
            if(ret == TNUMBER || ret == TSTRING) {
              uint8_t a = 0;
              if(in_child == -1) {
                /* LCOV_EXCL_START*/
                logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                return -1;
                /* LCOV_EXCL_STOP*/
              }
              if(lexer_peek(text, in_child, &a, &start, &len) < 0) {
                /* LCOV_EXCL_START*/
                logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                return -1;
                /* LCOV_EXCL_STOP*/
              }

              if(a == TFUNCTION || (a == TEVENT && in_child > 0)) {
                if(ret == TSTRING) {
                  bc_parent(obj, OP_PUSH, val, 0, 1);
                } else {
                  bc_parent(obj, OP_PUSH, val, 0, 0);
                }
              }
            } else if(ret == TOPERATOR) {
              bc_parent(obj, OP_PUSH, val, 0, 0);
            }

            ret = LPAREN;
            go = TFUNCTION;
          } break;
          case VPTR: {
            if(ret == TFUNCTION) {
              uint8_t a = 0;
              if(in_child == -1) {
                /* LCOV_EXCL_START*/
                logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                return -1;
                /* LCOV_EXCL_STOP*/
              }
              if(lexer_peek(text, in_child, &a, &start, &len) < 0) {
                /* LCOV_EXCL_START*/
                logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                return -1;
                /* LCOV_EXCL_STOP*/
              }

              if(a == TFUNCTION || a == TEVENT) {
                if(lexer_peek(text, pos, &a, &start, &len) < 0) {
                  /* LCOV_EXCL_START*/
                  logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
                  return -1;
                  /* LCOV_EXCL_STOP*/
                }

                /*
                 * This is the only way to retrieve a VPTR
                 * in the (*text) variable.
                 */
                uint16_t p = ((getval((*text)[start+sizeof(uint8_t)]) & 0xFF) << 8);
                p |= getval((*text)[start+sizeof(uint8_t)+sizeof(uint8_t)]) & 0xFF;

                struct vm_top_t *op = (struct vm_top_t *)&obj->bc.buffer[p];

                bc_parent(obj, OP_PUSH, getval(op->a), 0, 0);
              }
            }

            go = VPTR;
            ret = LPAREN;
          } break;
          /* LCOV_EXCL_START*/
          default: {
            logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
            return -1;
          } break;
          /* LCOV_EXCL_STOP*/
        }
      } break;
      case TEOF:
      case TEND: {
        int32_t lastjmp = getval(obj->bc.nrbytes);
        if(lexer_peek(text, pos, &type, &start, &len) < 0) {
          /* LCOV_EXCL_START*/
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }

        if(type == TEOF) {
          bc_parent(obj, OP_RET, 0, 0, 0);

          bc_assign_slots(obj);

          loop = 0;
        } else if(type == TEND) {
          pos++;
          if(lexer_peek(text, pos, &type, &start, &len) < 0) {
            /* LCOV_EXCL_START*/
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
            /* LCOV_EXCL_STOP*/
          }

          int16_t step = getval(obj->bc.nrbytes);
          struct vm_top_t *jmp = NULL;

          while((lastjmp = bc_before(lastjmp))) {
            if(gettype(obj->bc.buffer[lastjmp]) == OP_JMP) {
              jmp = (struct vm_top_t *)&obj->bc.buffer[lastjmp];
              if((int8_t)getval(jmp->b) == depth) {
                setval(jmp->b, 0);
                setval(jmp->a, ((step-lastjmp)/sizeof(struct vm_top_t)));
              }
            }
          }

          switch(type) {
            case TELSE: {
              pos++;
            } break;
            case TEND:
            case TEOF:
            case TIF:
            case TVAR:
            case TFUNCTION:
            case TEVENT:
            case TELSEIF: {
            } break;
            /* LCOV_EXCL_START*/
            default: {
              logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
              return -1;
            } break;
            /* LCOV_EXCL_STOP*/
          }

          ret = TIF;
          go = type;
          depth--;
        } else {
          /* LCOV_EXCL_START*/
          logprintf_P(F("ERROR: Unexpected token (%d)"), __LINE__);
          return -1;
          /* LCOV_EXCL_STOP*/
        }
      } break;
    }
  }

  return 0;
}

int8_t rule_run(struct rules_t *obj, uint8_t validate) {
  uint16_t pos = 0;
  uint8_t t = 0;

  /*
   * This approach is much faster than a switch
   * Initialize once for even better performance
   */

  if(jmptbl[1] == NULL) {
    void *tmp[JMPSIZE] = {
      NULL,             //                0
      &&STEP_OP_MATH,   // OP_EQ,         1
      &&STEP_OP_MATH,   // OP_NE,         2
      &&STEP_OP_MATH,   // OP_LT,         3
      &&STEP_OP_MATH,   // OP_LE,         4
      &&STEP_OP_MATH,   // OP_GT,         5
      &&STEP_OP_MATH,   // OP_GE,         6
      &&STEP_OP_MATH,   // OP_AND,        7
      &&STEP_OP_MATH,   // OP_OR,         8
      &&STEP_OP_MATH,   // OP_SUB,        9
      &&STEP_OP_MATH,   // OP_ADD,        10
      &&STEP_OP_MATH,   // OP_DIV,        11
      &&STEP_OP_MATH,   // OP_MUL,        12
      &&STEP_OP_MATH,   // OP_POW,        13
      &&STEP_OP_MATH,   // OP_MOD,        14
      &&STEP_TEST,      // OP_JMP,        15
      &&STEP_JMP,       // OP_JMP,        15
      &&STEP_SETVAL,    // OP_SETVAL,     16
      &&STEP_GETVAL,    // OP_GETVAL,     17
      &&STEP_PUSH,      // OP_PUSH,       18
      &&STEP_CALL,      // OP_CALL,       19
      &&STEP_CLEAR,     // OP_CLEAR,      20
      &&STEP_RET,       // OP_RET         21
      &&STEP_OP_EQ,     // OP_EQ          22
      &&STEP_OP_NE,     // OP_NE          23
      &&STEP_OP_LT,     // OP_LT          24
      &&STEP_OP_LE,     // OP_LE          25
      &&STEP_OP_GT,     // OP_GT          26
      &&STEP_OP_GE,     // OP_GE          27
      &&STEP_OP_AND,    // OP_AND         28
      &&STEP_OP_OR,     // OP_OR          29
      &&STEP_OP_SUB,    // OP_SUB         30
      &&STEP_OP_ADD,    // OP_ADD         31
      &&STEP_OP_DIV,    // OP_DIV         32
      &&STEP_OP_MUL,    // OP_MUL         33
      &&STEP_OP_POW,    // OP_POW         34
      &&STEP_OP_MOD,    // OP_MOD         35
    };
    memcpy(&jmptbl, &tmp, sizeof(tmp));
  }

  memset(stack->buffer, 0, getval(stack->bufsize));
  setval(stack->nrbytes, 4);

/*****************/
  BEGIN:
    uint8_t type = gettype(obj->bc.buffer[pos]);
#ifdef DEBUG
    printf("rule #%d, pos: %lu, op_id: %d, op: %s\n", obj->nr, pos/sizeof(struct vm_top_t), type, op_names[type].name);
#endif
    goto *jmptbl[type];
/*****************/

/*****************/
  STEP_OP_MATH: {
    struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[pos];
    int16_t a = vm_val_pos((int8_t)getval(node->a));
    int16_t b = vm_val_pos((int8_t)getval(node->b));
    int16_t c = vm_val_pos((int8_t)getval(node->c));

#if defined(DEBUG) || defined(COVERALLS)
    if((int8_t)getval(node->a) >= 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    if((int8_t)getval(node->b) >= 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    if((int8_t)getval(node->c) >= 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    if(b > getval(obj->heap->nrbytes)) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    if(c > getval(obj->heap->nrbytes)) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
#endif

    float nr = 0, var = 0;
    float x = 0, y = 0;
    uint8_t x_type = gettype(obj->heap->buffer[b]);
    uint8_t y_type = gettype(obj->heap->buffer[c]);

    if(x_type == VINTEGER) {
      struct vm_vinteger_t *node1 = (struct vm_vinteger_t *)&obj->heap->buffer[b];
      uint32_t val = 0;

      val |= getval(node1->value[0]) << 16;
      val |= getval(node1->value[1]) << 8;
      val |= getval(node1->value[2]);

      /*
       * Correctly restore sign
       */
      if(val & 0x800000) {
        val |= 0xFF000000;
        x = ((float)(val*-1))*-1;
      } else {
        x = (float)val;
      }
    } else if(x_type == VFLOAT) {
      struct vm_vfloat_t *node = (struct vm_vfloat_t *)&obj->heap->buffer[b];
      uint32_t val = 0;

      val |= (getval(node->type) >> 5) << 29;
      val |= getval(node->value[0]) << 21;
      val |= getval(node->value[1]) << 13;
      val |= getval(node->value[2]) << 5;

      uint322float(val, &x);
    } else if(x_type == VNULL) {
#ifndef DEBUG
      goto STEP_IS_NULL;
#endif
    } else if(is_math(type)) {
      uint8_t i = 0;
      const char *op = NULL;
      for(i=0;i<nr_rule_operators;i++) {
        if(rule_operators[i].opcode == type) {
          op = rule_operators[i].name;
          break;
        }
      }
      logprintf_P(F("ERROR: cannot compute %s with a left char value"), op);
      return -1;
    } else if(is_op_and_math(type)) {
      uint8_t i = 0;
      const char *op = NULL;
      for(i=0;i<nr_rule_operators;i++) {
        if(rule_operators[i].opcode == type) {
          op = rule_operators[i].name;
          break;
        }
      }
      logprintf_P(F("ERROR: cannot compare %s with a left char value"), op);
      return -1;
    }
    if(y_type == VINTEGER) {
      struct vm_vinteger_t *node1 = (struct vm_vinteger_t *)&obj->heap->buffer[c];
      uint32_t val = 0;
      val |= getval(node1->value[0]) << 16;
      val |= getval(node1->value[1]) << 8;
      val |= getval(node1->value[2]);

      /*
       * Correctly restore sign
       */
      if(val & 0x800000) {
        val |= 0xFF000000;
        y = ((float)(val*-1))*-1;
      } else {
        y = (float)val;
      }
    } else if(y_type == VFLOAT) {
      struct vm_vfloat_t *node = (struct vm_vfloat_t *)&obj->heap->buffer[c];
      uint32_t val = 0;

      val |= (getval(node->type) >> 5) << 29;
      val |= getval(node->value[0]) << 21;
      val |= getval(node->value[1]) << 13;
      val |= getval(node->value[2]) << 5;

      uint322float(val, &y);
    } else if(y_type == VNULL) {
      goto STEP_IS_NULL;
    } else if(is_math(type)) {
      uint8_t i = 0;
      const char *op = NULL;
      for(i=0;i<nr_rule_operators;i++) {
        if(rule_operators[i].opcode == type) {
          op = rule_operators[i].name;
          break;
        }
      }
      logprintf_P(F("ERROR: cannot compute %s with a right char value"), op);
      return -1;
    } else if(is_op_and_math(type)) {
      uint8_t i = 0;
      const char *op = NULL;
      for(i=0;i<nr_rule_operators;i++) {
        if(rule_operators[i].opcode == type) {
          op = rule_operators[i].name;
          break;
        }
      }
      logprintf_P(F("ERROR: cannot compare %s with a right char value"), op);
      return -1;
    }
#ifdef DEBUG
    if(x_type == VNULL) {
      goto STEP_IS_NULL;
    }
#endif

    goto *jmptbl[type+22];

    STEP_OP_ADD:
      var = x+y;
      goto STEP_MATH_RESULT;
    STEP_OP_DIV:
      var = x/y;
      goto STEP_MATH_RESULT;
    STEP_OP_SUB:
      var = x-y;
      goto STEP_MATH_RESULT;
    STEP_OP_MUL:
      var = x*y;
      goto STEP_MATH_RESULT;
    STEP_OP_POW:
      var = powf(x, y);
      goto STEP_MATH_RESULT;
    STEP_OP_MOD:
      var = fmodf(x, y);
      goto STEP_MATH_RESULT;
    STEP_OP_EQ:
      t = var = (fabsf(x - y) < EPSILON);
      goto STEP_OP_RESULT;
    STEP_OP_NE:
      t = var = (fabsf(x - y) >= EPSILON);
      goto STEP_OP_RESULT;
    STEP_OP_OR:
      t = var = (x > 0 || y > 0);
      goto STEP_OP_RESULT;
    STEP_OP_AND:
      t = var = (x > 0 && y > 0);
      goto STEP_OP_RESULT;
    STEP_OP_LT:
      t = var = (x < y);
      goto STEP_OP_RESULT;
    STEP_OP_LE:
      t = var = (x <= y);
      goto STEP_OP_RESULT;
    STEP_OP_GT:
      t = var = (x > y);
      goto STEP_OP_RESULT;
    STEP_OP_GE:
      t = var = (x >= y);
      goto STEP_OP_RESULT;

    t = 1;

    STEP_IS_NULL: {
#ifdef DEBUG
    {
      const char *op = NULL;
      uint8_t i = 0;
      for(i=0;i<nr_rule_operators;i++) {
        if(rule_operators[i].opcode == type) {
          op = rule_operators[i].name;
          break;
        }
      }

      if(x_type != VNULL && y_type != VNULL) {
        printf("\t%g %s %g = %g -> %d\n", (double)x, op, (double)y, (double)var, vm_val_posr(a));
      } else if(x_type != VNULL && y_type == VNULL) {
        printf("\t%g %s NULL = %g -> %d\n", (double)x, op, (double)var, vm_val_posr(a));
      } else if(x_type == VNULL && y_type != VNULL) {
        printf("\tNULL %s %g = %g -> %d\n", op, (double)y, (double)var, vm_val_posr(a));
      } else {
        printf("\tNULL %s NULL = NULL -> %d\n", op, vm_val_posr(a));
      }
    }
#endif

      struct vm_vnull_t *value = (struct vm_vnull_t *)&obj->heap->buffer[a];
      setval(value->type, VNULL);

      pos += sizeof(struct vm_top_t);
      goto BEGIN;
    }

    STEP_OP_RESULT: {
#ifdef DEBUG
      {
        const char *op = NULL;
        uint8_t i = 0;
        for(i=0;i<nr_rule_operators;i++) {
          if(rule_operators[i].opcode == type) {
            op = rule_operators[i].name;
            break;
          }
        }

        if(x_type == VNULL && y_type == VNULL) {
          printf("\tNULL %s %g = %g -> %d\n", op, (double)y, (double)var, vm_val_posr(a));
        } else if(x_type != VNULL && y_type == VNULL) {
          printf("\tNULL %s %g = %g -> %d\n", op, (double)x, (double)var, vm_val_posr(a));
        } else if(x_type != VNULL && y_type != VNULL) {
          printf("\t%g %s %g = %g -> %d\n", (double)x, op, (double)y, (double)var, vm_val_posr(a));
        } else {
          printf("\tNULL %s NULL = NULL -> %d\n", op, vm_val_posr(a));
        }
      }
#endif
      struct vm_vinteger_t *value = (struct vm_vinteger_t *)&obj->heap->buffer[a];
      setval(value->type, VINTEGER);
      setval(value->value[0], 0);
      setval(value->value[1], 0);
      setval(value->value[2], var);

      pos += sizeof(struct vm_top_t);
      goto BEGIN;
    }

    STEP_MATH_RESULT: {
#ifdef DEBUG
      {
        const char *op = NULL;
        uint8_t i = 0;
        for(i=0;i<nr_rule_operators;i++) {
          if(rule_operators[i].opcode == type) {
            op = rule_operators[i].name;
            break;
          }
        }

        if(x_type == VNULL && y_type == VNULL) {
          printf("\tNULL %s %g = %g -> %d\n", op, (double)y, (double)var, vm_val_posr(a));
        } else if(x_type != VNULL && y_type == VNULL) {
          printf("\tNULL %s %g = %g -> %d\n", op, (double)x, (double)var, vm_val_posr(a));
        } else if(x_type != VNULL && y_type != VNULL) {
          printf("\t%g %s %g = %g -> %d\n", (double)x, op, (double)y, (double)var, vm_val_posr(a));
        } else {
          printf("\tNULL %s NULL = NULL -> %d\n", op, vm_val_posr(a));
        }
      }
#endif
      if(modff(var, &nr) == 0) {
        struct vm_vinteger_t *value = (struct vm_vinteger_t *)&obj->heap->buffer[a];
        setval(value->type, VINTEGER);
        setval(value->value[0], ((uint32_t)var >> 16) & 0xFF);
        setval(value->value[1], ((uint32_t)var >> 8) & 0xFF);
        setval(value->value[2], ((uint32_t)var) & 0xFF);
      } else {
        float f = float32to27(var);
        uint32_t x = 0;
        float2uint32(f, &x);

        struct vm_vfloat_t *value = (struct vm_vfloat_t *)&obj->heap->buffer[a];
        setval(value->type, VFLOAT | ((((uint32_t)x >> 29) & 0x7) << 5));
        setval(value->value[0], ((uint32_t)x >> 21) & 0xFF);
        setval(value->value[1], ((uint32_t)x >> 13) & 0xFF);
        setval(value->value[2], ((uint32_t)x >> 5) & 0xFF);
      }

      pos += sizeof(struct vm_top_t);
      goto BEGIN;
    }
  }
/*****************/

/*****************/
  STEP_JMP: {
    struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[pos];
#if defined(DEBUG) || defined(COVERALLS)
    /* LCOV_EXCL_START*/
    if((uint8_t)getval(node->a) <= 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    /* LCOV_EXCL_STOP*/
#endif

    if(t == 1 || validate == 1) {
      pos += sizeof(struct vm_top_t);
    } else {
      pos += (sizeof(struct vm_top_t)*(int8_t)getval(node->a));
    }

    t = 0;

    goto BEGIN;
  }
/*****************/

/*****************/
  STEP_TEST: {
    struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[pos];
    int16_t a = vm_val_pos((int8_t)getval(node->a));

    float x = 0;
    uint8_t x_type = gettype(obj->heap->buffer[a]);

    if(x_type == VINTEGER) {
      struct vm_vinteger_t *node1 = (struct vm_vinteger_t *)&obj->heap->buffer[a];
      uint32_t val = 0;

      val |= getval(node1->value[0]) << 16;
      val |= getval(node1->value[1]) << 8;
      val |= getval(node1->value[2]);

      /*
       * Correctly restore sign
       */
      if(val & 0x800000) {
        val |= 0xFF000000;
        x = ((float)(val*-1))*-1;
      } else {
        x = (float)val;
      }
      t = (x > 0);
    } else if(x_type == VFLOAT) {
      struct vm_vfloat_t *node = (struct vm_vfloat_t *)&obj->heap->buffer[a];
      uint32_t val = 0;

      val |= (getval(node->type) >> 5) << 29;
      val |= getval(node->value[0]) << 21;
      val |= getval(node->value[1]) << 13;
      val |= getval(node->value[2]) << 5;

      uint322float(val, &x);
      t = (x > 0);
    } else if(x_type == VNULL) {
      t = 0;
    }

#ifdef DEBUG
    if(t == 1) {
      printf("\t= TRUE\n");
    } else {
      printf("\t= FALSE\n");
    }
#endif


    pos += sizeof(struct vm_top_t);
    goto BEGIN;
  }
/*****************/

/*****************/
  STEP_GETVAL: {
    struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[pos];

    uint16_t b = (int8_t)getval(node->b)*sizeof(struct vm_vchar_t);
    int16_t a = vm_val_pos(getval(node->a));

#if defined(DEBUG) || defined(COVERALLS)
    if((int8_t)getval(node->b) < 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    if((int8_t)getval(node->a) >= 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    if(a > getval(obj->heap->nrbytes)) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
#endif

    vm_stack_push(b, &varstack->buffer[b]);
    rule_options.vm_value_get(obj);

#if defined(DEBUG) || defined(COVERALLS)
    /* LCOV_EXCL_START*/
    if(rules_gettop() < 2) {
      logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
      return -1;
    }
#endif

    /* LCOV_EXCL_STOP*/
    switch(rules_type(-1)) {
      case VNULL: {
        struct vm_vnull_t *upd = (struct vm_vnull_t *)&obj->heap->buffer[a];
        setval(upd->type, VNULL);
      } break;
      case VINTEGER: {
        int32_t x = rules_tointeger(-1);
        struct vm_vinteger_t *upd = (struct vm_vinteger_t *)&obj->heap->buffer[a];
        setval(upd->type, VINTEGER);
        setval(upd->value[0], ((uint32_t)x >> 16) & 0xFF);
        setval(upd->value[1], ((uint32_t)x >> 8) & 0xFF);
        setval(upd->value[2], ((uint32_t)x) & 0xFF);
      } break;
      case VCHAR: {
        int16_t offset = vm_val_pos(-1);
        offset = getval(stack->nrbytes)-offset;

        if(offset >= 4) {
          if(getval(stack->buffer[offset]) == VPTR) {
            struct vm_vptr_t *node = (struct vm_vptr_t *)&stack->buffer[offset];

            struct vm_vptr_t *upd = (struct vm_vptr_t *)&obj->heap->buffer[a];
            setval(upd->type, VPTR);
            setval(upd->value, getval(node->value));
          } else {
            return -1;
          }
        } else {
          return -1;
        }
      } break;
      case VFLOAT: {
        float f = rules_tofloat(-1);
        uint32_t x = 0;
        float2uint32(f, &x);

        struct vm_vfloat_t *upd = (struct vm_vfloat_t *)&obj->heap->buffer[a];

        setval(upd->type, VFLOAT | ((((uint32_t)x >> 29) & 0x7) << 5));
        setval(upd->value[0], ((uint32_t)x >> 21) & 0xFF);
        setval(upd->value[1], ((uint32_t)x >> 13) & 0xFF);
        setval(upd->value[2], ((uint32_t)x >> 5) & 0xFF);

      } break;
      /* LCOV_EXCL_START*/
      default: {
        logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
        return -1;
      } break;
      /* LCOV_EXCL_STOP*/
    }

    rules_remove(-1);
    rules_remove(-1);

    pos += sizeof(struct vm_top_t);

    goto BEGIN;
  }
/*****************/

/*****************/
  STEP_SETVAL: {
    struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[pos];

#if defined(DEBUG) || defined(COVERALLS)
    if((int8_t)getval(node->a) < 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    // if((int8_t)getval(node->b) > 0) {
      // logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      // return -1;
    // }
#endif

    if((int8_t)getval(node->b) < 0) {
      uint16_t a = (int8_t)getval(node->a)*sizeof(struct vm_vchar_t);
      int16_t b = vm_val_pos((int8_t)getval(node->b));

#if defined(DEBUG) || defined(COVERALLS)
      if(b > getval(obj->heap->nrbytes)) {
        logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
        return -1;
      }
#endif

#ifdef DEBUG
      struct vm_vchar_t *var = (struct vm_vchar_t *)&varstack->buffer[a];
      switch(gettype(obj->heap->buffer[b])) {
        case VINTEGER: {
          struct vm_vinteger_t *out = (struct vm_vinteger_t *)&obj->heap->buffer[b];
          uint32_t val = 0;
          val |= getval(out->value[0]) << 16;
          val |= getval(out->value[1]) << 8;
          val |= getval(out->value[2]);

          /*
           * Correctly restore sign
           */
          if(val & 0x800000) {
            val |= 0xFF000000;
          }
          printf("%s = %d\n", (const char *)var->value, val);
        } break;
        case VNULL: {
          printf("%s = NULL\n", (const char *)var->value);
        } break;
      }
#endif
      vm_stack_push(a, &varstack->buffer[a]);
      vm_stack_push(b, &obj->heap->buffer[b]);

      rule_options.vm_value_set(obj);

      rules_remove(-1);
      rules_remove(-1);

      memset(stack->buffer, 0, getval(stack->bufsize));
      setval(stack->nrbytes, 4);
    } else if((int8_t)getval(node->b) > 0) {
      uint16_t a = (int8_t)getval(node->a)*sizeof(struct vm_vchar_t);
      uint16_t b = (int8_t)(getval(node->b)-1)*sizeof(struct vm_vchar_t);

#if defined(DEBUG) || defined(COVERALLS)
      if(b > varstack->nrbytes) {
        logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
        return -1;
      }
#endif

      vm_stack_push(a, &varstack->buffer[a]);
      vm_stack_push(b, &varstack->buffer[b]);

      rule_options.vm_value_set(obj);

      rules_remove(-1);
      rules_remove(-1);

      memset(stack->buffer, 0, getval(stack->bufsize));
      setval(stack->nrbytes, 4);
    } else { // node->b == 0
      uint16_t a = (int8_t)getval(node->a)*sizeof(struct vm_vchar_t);
      uint16_t b = ((int8_t)getval(node->b)+1)*rule_max_var_bytes();

      if(rules_gettop() >= 1) {
        vm_stack_push(a, &varstack->buffer[a]);
        vm_stack_push(b, &stack->buffer[b]);
        rules_remove((int8_t)getval(node->b)+1);
      } else {
        vm_stack_push(a, &varstack->buffer[a]);
        rules_pushnil();
      }

      rule_options.vm_value_set(obj);

      rules_remove(-1);
      rules_remove(-1);
    }
    pos += sizeof(struct vm_top_t);

    goto BEGIN;
  }

  STEP_PUSH: {
    struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[pos];
    if((int8_t)getval(node->a) < 0) {
      int16_t a = vm_val_pos((int8_t)getval(node->a));

#if defined(DEBUG) || defined(COVERALLS)
      if(a > getval(obj->heap->nrbytes)) {
        logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
        return -1;
      }
#endif

      vm_stack_push(a, &obj->heap->buffer[a]);
    } else {
      uint16_t a = (uint8_t)(getval(node->a)-1)*sizeof(struct vm_vchar_t);

#if defined(DEBUG) || defined(COVERALLS)
      if(a > varstack->nrbytes) {
        logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
        return -1;
      }
#endif

      vm_stack_push(a, &varstack->buffer[a]);
    }

    pos += sizeof(struct vm_top_t);

    goto BEGIN;
  }
/*****************/

/*****************/
  STEP_CALL: {
    struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[pos];
    int16_t a = vm_val_pos((int8_t)getval(node->a));
    uint16_t b = (int8_t)getval(node->b);
    uint16_t c = (int8_t)getval(node->c);

#if defined(DEBUG) || defined(COVERALLS)
    if((int8_t)getval(node->a) > 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    if((int8_t)getval(node->b) < 0) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    if((int8_t)getval(node->c) < 0 || (int8_t)getval(node->c) > 1) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
    if(a > getval(obj->heap->nrbytes)) {
      logprintf_P(F("FATAL: Internal error in %s #%d pos (%d)"), __FUNCTION__, __LINE__, pos/4);
      return -1;
    }
#endif

    if(c == 0) {
      if(rule_functions[b].callback() != 0) {
        /* LCOV_EXCL_START*/
        logprintf_P(F("FATAL: function call '%s' failed"), rule_functions[b].name);
        return -1;
        /* LCOV_EXCL_STOP*/
      }
      if(rules_gettop() == 1 && a != 4) {
        switch(rules_type(-1)) {
          case VNULL: {
            struct vm_vnull_t *upd = (struct vm_vnull_t *)&obj->heap->buffer[a];
            setval(upd->type, VNULL);
          } break;
          case VINTEGER: {
            int32_t x = rules_tointeger(-1);
            struct vm_vinteger_t *upd = (struct vm_vinteger_t *)&obj->heap->buffer[a];
            setval(upd->type, VINTEGER);
            setval(upd->value[0], ((uint32_t)x >> 16) & 0xFF);
            setval(upd->value[1], ((uint32_t)x >> 8) & 0xFF);
            setval(upd->value[2], ((uint32_t)x) & 0xFF);
          } break;
          case VCHAR: {
            int16_t offset = vm_val_pos(-1);
            offset = getval(stack->nrbytes)-offset;

            if(offset >= 4) {
              if(getval(stack->buffer[offset]) == VPTR) {
                struct vm_vptr_t *node = (struct vm_vptr_t *)&stack->buffer[offset];
                struct vm_vptr_t *upd = (struct vm_vptr_t *)&obj->heap->buffer[a];
                setval(upd->type, VPTR);
                setval(upd->value, getval(node->value));
              } else {
                return -1;
              }
            } else {
              return -1;
            }
          } break;
          case VFLOAT: {
            float f = rules_tofloat(-1);
            uint32_t x = 0;
            float2uint32(f, &x);

            struct vm_vfloat_t *upd = (struct vm_vfloat_t *)&obj->heap->buffer[a];

            setval(upd->type, VFLOAT | ((((uint32_t)x >> 29) & 0x7) << 5));
            setval(upd->value[0], ((uint32_t)x >> 21) & 0xFF);
            setval(upd->value[1], ((uint32_t)x >> 13) & 0xFF);
            setval(upd->value[2], ((uint32_t)x >> 5) & 0xFF);
          } break;
          /* LCOV_EXCL_START*/
          default: {
            logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
            return -1;
          } break;
          /* LCOV_EXCL_STOP*/
        }

        rules_remove(-1);
      }
    } else {
      struct vm_vchar_t *var = (struct vm_vchar_t *)&varstack->buffer[b*sizeof(struct vm_vchar_t)];

      if(rule_options.event_cb(obj, var->value) == 1) {
        setval(obj->cont, pos+sizeof(struct vm_top_t));

        obj = obj->ctx.go;
        pos = 0;

#ifdef DEBUG
      printf("\n");
#endif

        goto BEGIN;
      } else {
        while(rules_gettop() > 0) {
          rules_remove(1);
        }
      }
    }

    pos += sizeof(struct vm_top_t);

    goto BEGIN;
  }
/*****************/

/*****************/
  STEP_CLEAR: {

    memset(stack->buffer, 0, getval(stack->bufsize));
    setval(stack->nrbytes, 4);
    pos += sizeof(struct vm_top_t);

    goto BEGIN;
  }

  STEP_RET: {
    if(rule_options.done_cb != NULL) {
      rule_options.done_cb(obj);
    }
    if(obj->ctx.ret != NULL) {
      struct rules_t *newctx = obj->ctx.ret;
      obj->ctx.ret = NULL;
      obj->ctx.go = NULL;

      obj = newctx;
      pos = getval(obj->cont);

#ifdef DEBUG
      printf("\n");
#endif

    } else {
      setval(obj->cont, 0);

      return 0;
    }
    goto BEGIN;
  }
}

#ifdef DEBUG
static void print_heap(struct rules_t *obj) {
  uint16_t size = getval(obj->heap->nrbytes), i = 0;

  for(i=4;i<size;i+=rule_max_var_bytes()) {
    uint8_t type = gettype(obj->heap->buffer[i]);
    printf("%2lu\t", i/sizeof(struct vm_top_t));
    switch(type) {
      case VPTR: {
        struct vm_vptr_t *node = (struct vm_vptr_t *)&obj->heap->buffer[i];
        printf("VPTR\t\t%d\n", getval(node->value));
      } break;
      case VINTEGER: {
        struct vm_vinteger_t *node = (struct vm_vinteger_t *)&obj->heap->buffer[i];
        uint32_t val = 0;
        val |= getval(node->value[0]) << 16;
        val |= getval(node->value[1]) << 8;
        val |= getval(node->value[2]);

        /*
         * Correctly restore sign
         */
        if(val & 0x800000) {
          val |= 0xFF000000;
        }

        printf("VINTEGER\t%d\n", val);
      } break;
      case VFLOAT: {
        struct vm_vfloat_t *node = (struct vm_vfloat_t *)&obj->heap->buffer[i];
        uint32_t val = 0;

        val |= (getval(node->type) >> 5) << 29;
        val |= getval(node->value[0]) << 21;
        val |= getval(node->value[1]) << 13;
        val |= getval(node->value[2]) << 5;

        float f = 0;
        uint322float(val, &f);
        printf("VFLOAT\t\t%g\n", (double)f);
      } break;
      case VNULL: {
        printf("VNULL\n");
      } break;
    }
  }
}

static void print_varstack(void) {
  uint16_t i = 0;

  for(i=0;i<varstack->nrbytes;i++) {
    printf("%2lu ", i/sizeof(struct vm_vchar_t));

    switch(varstack->buffer[i]) {
      case VCHAR: {
        struct vm_vchar_t *node = (struct vm_vchar_t *)&varstack->buffer[i];
        printf("%d %d", node->fixed, node->ref);
        if(node->value != NULL) {
          printf("\t%s\n", node->value);
        } else {
          printf("\n");
        }
      } break;
      /* LCOV_EXCL_START*/
      default: {
        logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
        return;
      } break;
      /* LCOV_EXCL_STOP*/
    }
    i += sizeof(struct vm_vchar_t)-1;
  }
}

static void print_stack(struct rules_t *obj) {
  uint16_t size = getval(stack->nrbytes), i = 0;

  for(i=4;i<size;i+=rule_max_var_bytes()) {
    uint8_t type = getval(stack->buffer[i]);
    printf("%2d\t", vm_val_posr(i*-1));
    switch(type) {
      case VINTEGER: {
        struct vm_vinteger_t *node = (struct vm_vinteger_t *)&stack->buffer[i];
        uint32_t val = 0;
        val |= getval(node->value[0]) << 16;
        val |= getval(node->value[1]) << 8;
        val |= getval(node->value[2]);

        /*
         * Correctly restore sign
         */
        if(val & 0x800000) {
          val |= 0xFF000000;
        }

        printf("VINTEGER\t%d\n", val);
      } break;
      case VFLOAT: {
        struct vm_vfloat_t *node = (struct vm_vfloat_t *)&stack->buffer[i];
        uint32_t val = 0;

        val |= (getval(node->type) >> 5) << 29;
        val |= getval(node->value[0]) << 21;
        val |= getval(node->value[1]) << 13;
        val |= getval(node->value[2]) << 5;

        float f = 0;
        uint322float(val, &f);

        printf("VFLOAT\t\t%g\n", (double)f);
      } break;
      case VNULL: {
        printf("VNULL\n");
      } break;
      case VPTR: {
        struct vm_vptr_t *node = (struct vm_vptr_t *)&stack->buffer[i];
        uint16_t pos = getval(node->value)*sizeof(struct vm_top_t);
        struct vm_vchar_t *val = (struct vm_vchar_t *)&varstack->buffer[pos];
        printf("VCHAR\t%s\n", val->value);
      } break;
      /* LCOV_EXCL_START*/
      default: {
        logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
        return;
      } break;
      /* LCOV_EXCL_STOP*/
    }
  }
}

static void print_bytecode(struct rules_t *obj) {
  uint16_t i = 0;
  uint16_t nrbytes = getval(obj->bc.nrbytes);
  uint8_t type = 0;
  for(i=0;i<nrbytes;i = bc_next(obj, i)) {
    printf("%2lu\t", i/sizeof(struct vm_top_t));
    type = gettype(obj->bc.buffer[i]);

    struct vm_top_t *node = (struct vm_top_t *)&obj->bc.buffer[i];
    printf("%s\t", op_names[type].name);
    if(strlen(op_names[type].name) < 8) {
      printf("\t");
    }
    if(type == OP_JMP) {
      printf("%lu\t", (uint8_t)getval(node->a)+(i/sizeof(struct vm_top_t)));
    } else {
      printf("%d\t", (int8_t)getval(node->a));
    }
    printf("%d\t", (int8_t)getval(node->b));
    printf("%d\t", (int8_t)getval(node->c));
    if(get_group(obj->bc.buffer[i])) {
      printf(" // %d", get_group(obj->bc.buffer[i]));
    }
    printf("\n");
  }
}
#endif

#if defined(DEBUG) || defined(COVERALLS)
uint16_t rules_memused(void) {
  return memused;
}
#endif

void rules_gc(struct rules_t ***rules, uint8_t *nrrules) {
  uint16_t i = 0;

  FREE(*rules);
  *rules = NULL;
  *nrrules = 0;

  if(varstack != NULL) {
    for(i=0;i<varstack->nrbytes;i++) {
      switch(varstack->buffer[i]) {
        case VCHAR: {
          struct vm_vchar_t *node = (struct vm_vchar_t *)&varstack->buffer[i];
          FREE(node->value);
        } break;
        /* LCOV_EXCL_START*/
        default: {
          logprintf_P(F("FATAL: Internal error in %s #%d"), __FUNCTION__, __LINE__);
          return;
        } break;
        /* LCOV_EXCL_STOP*/
      }
      i += sizeof(struct vm_vchar_t)-1;
    }
    if(varstack->nrbytes > 0 && varstack->buffer != NULL) {
      FREE(varstack->buffer);
    }
    FREE(varstack);
  }

  if(stack != NULL) {
    stack->bufsize = 0;
    stack->nrbytes = 0;
    stack = NULL;
  }

  varstack = NULL;

#if defined(DEBUG) || defined(COVERALLS)
  memused = 0;
#endif
}

int8_t rule_initialize(struct pbuf *input, struct rules_t ***rules, uint8_t *nrrules, struct pbuf *mempool, void *userdata) {
  struct pbuf *mempool_rule = NULL;
  uint16_t newlen = getval(input->tot_len), max_varstack_size = 4;
  uint16_t heapsize = 4, bcsize = 0, varsize = 0, memsize = 0;
  if(varstack == NULL) {
    if((varstack = (struct rule_stack_t *)MALLOC(sizeof(struct rule_stack_t))) == NULL) {
      OUT_OF_MEMORY
    }
    memset(varstack, 0, sizeof(struct rule_stack_t));
#if defined(DEBUG) || defined(COVERALLS)
    memused += sizeof(struct rule_stack_t);
#endif
  }

  if(stack != NULL) {
    if(getval(stack->bufsize) > max_varstack_size) {
      max_varstack_size = getval(stack->bufsize);
    }
  }

  if(newlen == 0) {
    return 1;
  }

  if(getval(((char *)input->payload)[0]) == 0) {
    return 1;
  }

  {
    char *a = (char *)input->payload;
    while(mempool) {
      char *b = (char *)mempool->payload;
      /*
       * Check if input is location inside this mempool
       */
      if(&a[0] >= &b[0] && &a[0] <= &b[mempool->tot_len]) {
        if((mempool->len+sizeof(struct rules_t)) >= input->len) {
          mempool = mempool->next;
          continue;
        }
      }
      if((mempool->len+sizeof(struct rules_t)) >= mempool->tot_len) {
        mempool = mempool->next;
        continue;
      } else {
        break;
      }
    }
    if(mempool == NULL) {
      logprintf_P(F("FATAL #%d: ruleset too large, out of memory"), __LINE__);
      return -1;
    }
  }

  mempool_rule = mempool;
  if((*rules = (struct rules_t **)REALLOC(*rules, sizeof(struct rules_t **)*((*nrrules)+1))) == NULL) {
    OUT_OF_MEMORY
  }
  (*rules)[*nrrules] = (struct rules_t *)&((unsigned char *)mempool->payload)[mempool->len];
  memset((*rules)[*nrrules], 0, sizeof(struct rules_t));
  mempool->len += sizeof(struct rules_t);

  (*rules)[*nrrules]->userdata = userdata;
  struct rules_t *obj = (*rules)[*nrrules];
#if defined(DEBUG) || defined(COVERALLS)
  memused += sizeof(struct rules_t **);
  memused += sizeof(struct rule_timer_t);
#endif

  setval(obj->nr, (*nrrules)+1);

  (*nrrules)++;

  obj->ctx.go = NULL;
  obj->ctx.ret = NULL;
  obj->name = NULL;

/*LCOV_EXCL_START*/
#if defined(ESP8266) || defined(ESP32)
  timestamp.first = micros();
#else
  clock_gettime(CLOCK_MONOTONIC, &timestamp.first);
#endif
/*LCOV_EXCL_STOP*/

  if(rule_prepare((char **)&input->payload, &bcsize, &heapsize, &varsize, &memsize, &newlen) == -1 ||
    (varsize/sizeof(struct vm_vchar_t)) > INT8_MAX ||
    ((varstack->nrbytes + varsize) / sizeof(struct vm_vchar_t)) > INT8_MAX) {
    if(varsize/sizeof(struct vm_vchar_t) > INT8_MAX) {
      logprintf_P(F("ERROR: maximum number of 127 variables reached"));
    }
    if((varstack->nrbytes + varsize) / sizeof(struct vm_vchar_t) > INT8_MAX) {
      logprintf_P(F("ERROR: maximum number of 127 variables reached"));
    }
    if((*rules = (struct rules_t **)REALLOC(*rules, sizeof(struct rules_t **)*((*nrrules)))) == NULL) {
      OUT_OF_MEMORY
    }
    mempool_rule->len -= sizeof(struct rules_t);
    (*nrrules)--;
#if defined(DEBUG) || defined(COVERALLS)
    memused = 0;
#endif
    return -1;
  }

/*LCOV_EXCL_START*/
#if defined(ESP8266) || defined(ESP32)
  timestamp.second = micros();

  logprintf_P(F("rule #%d was prepared in %d microseconds"), mmu_get_uint8(&obj->nr), timestamp.second - timestamp.first);
#else
  clock_gettime(CLOCK_MONOTONIC, &timestamp.second);

  printf("rule #%d was prepared in %.6f seconds\n", obj->nr,
    ((double)timestamp.second.tv_sec + 1.0e-9*timestamp.second.tv_nsec) -
    ((double)timestamp.first.tv_sec + 1.0e-9*timestamp.first.tv_nsec));
#endif
/*LCOV_EXCL_STOP*/

#if defined(ESP8266) || defined(ESP32)
  if((heapsize % 4) != 0) {
    Serial.println("Rules bytecode not 4 byte aligned!");
    exit(-1);
  }
  if((bcsize % 4) != 0) {
    Serial.println("Rules bytecode not 4 byte aligned!");
    exit(-1);
  }
  if((varsize % 4) != 0) {
    Serial.println("Rules bytecode not 4 byte aligned!");
    exit(-1);
  }
#endif

  {
    {
      char *a = (char *)input->payload;
      while(mempool) {
        char *b = (char *)mempool->payload;
        /*
         * Check if input is location inside this mempool
         */
        if(&a[0] >= &b[0] && &a[0] <= &b[mempool->tot_len]) {
          if((mempool->len+bcsize+heapsize+varsize) >= input->len) {
            mempool = mempool->next;
            continue;
          }
        }
        if((mempool->len+bcsize+heapsize+varsize) >= mempool->tot_len) {
          mempool = mempool->next;
          continue;
        } else {
          break;
        }
      }
      if(mempool == NULL) {
        logprintf_P(F("FATAL #%d: ruleset too large, out of memory"), __LINE__);
        if((*rules = (struct rules_t **)REALLOC(*rules, sizeof(struct rules_t **)*((*nrrules)))) == NULL) {
          OUT_OF_MEMORY
        }
        mempool_rule->len -= sizeof(struct rules_t);
        (*nrrules)--;
#if defined(DEBUG) || defined(COVERALLS)
        memused = 0;
#endif
        return -1;
      }
    }

    setval(obj->bc.bufsize, bcsize);

    obj->bc.buffer = (unsigned char *)&((unsigned char *)mempool->payload)[mempool->len];

    mempool->len += bcsize;
    memset(obj->bc.buffer, 0, bcsize);

    obj->heap = (struct rule_stack_t *)&((unsigned char *)mempool->payload)[mempool->len];
    setval(obj->heap->nrbytes, 4);
    setval(obj->heap->bufsize, 4);
    obj->heap->buffer = &((unsigned char *)mempool->payload)[mempool->len+sizeof(struct rule_stack_t)];

    mempool->len += heapsize+sizeof(struct rule_stack_t);

    stack = (struct rule_stack_t *)&((unsigned char *)mempool->payload)[mempool->len];
    setval(stack->bufsize, max_varstack_size);
    setval(stack->nrbytes, 4);
    stack->buffer = &((unsigned char *)mempool->payload)[mempool->len+sizeof(struct rule_stack_t)];

    if(varsize > 0) {
      if((varstack->buffer = (unsigned char *)REALLOC(varstack->buffer, varstack->bufsize+varsize)) == NULL) {
        OUT_OF_MEMORY
      }
      memset(&varstack->buffer[varstack->bufsize], 0, varsize);
      varstack->bufsize += varsize;
#if defined(DEBUG) || defined(COVERALLS)
      memused += varsize;
#endif
    }

    /*LCOV_EXCL_START*/
#if defined(ESP8266) || defined(ESP32)
    timestamp.first = micros();
#else
    clock_gettime(CLOCK_MONOTONIC, &timestamp.first);
#endif
    /*LCOV_EXCL_STOP*/
    if(rule_create((char **)&input->payload, obj) == -1) {
      if((*rules = (struct rules_t **)REALLOC(*rules, sizeof(struct rules_t **)*((*nrrules)))) == NULL) {
        OUT_OF_MEMORY
      }
      mempool_rule->len -= sizeof(struct rules_t);
      (*nrrules)--;
#if defined(DEBUG) || defined(COVERALLS)
      memused = 0;
#endif
      return -1;
    }

/*LCOV_EXCL_START*/
#if defined(ESP8266) || defined(ESP32)
    timestamp.second = micros();

    logprintf_P(F("rule #%d bytecode was created in %d microseconds"), getval(obj->nr), timestamp.second - timestamp.first);
    logprintf_P(F("bytecode: %d/%d, heap: %d/%d, stack: %d/%d bytes, varstack: %d/%d bytes, varstack slots: %d/127"),
      getval(obj->bc.nrbytes),
      getval(obj->bc.bufsize),
      getval(obj->heap->nrbytes),
      getval(obj->heap->bufsize),
      ((stack == NULL) ? 0 : getval(stack->nrbytes)),
      ((stack == NULL) ? 0 : getval(stack->bufsize)),
      ((varstack->nrbytes == 0) ? 0 : varstack->nrbytes),
      (varstack->bufsize),
	  varstack->nrbytes / sizeof(struct vm_vchar_t)
    );
#else
    clock_gettime(CLOCK_MONOTONIC, &timestamp.second);

    printf("rule #%d bytecode was created in %.6f seconds\n", obj->nr,
      ((double)timestamp.second.tv_sec + 1.0e-9*timestamp.second.tv_nsec) -
      ((double)timestamp.first.tv_sec + 1.0e-9*timestamp.first.tv_nsec));

    printf("bytecode: %d/%d, heap: %d/%d, stack: %d/%d bytes, varstack: %d/%d bytes\n",
      getval(obj->bc.nrbytes),
      getval(obj->bc.bufsize),
      getval(obj->heap->nrbytes),
      getval(obj->heap->bufsize),
      ((stack == NULL) ? 0 : getval(stack->nrbytes)),
      ((stack == NULL) ? 0 : getval(stack->bufsize)),
      ((varstack->nrbytes == 0) ? 0 : varstack->nrbytes),
      (varstack->bufsize)
    );
#endif
/*LCOV_EXCL_STOP*/

    setval(input->len, getval(input->len) + newlen);
    if(getval(((char *)input->payload)[newlen]) == 0) {
      setval(input->len, getval(input->len) + 1);
    }
    setval(input->tot_len, getval(input->tot_len) - newlen);
  }

/*LCOV_EXCL_START*/
#ifdef DEBUG
  #if !defined(ESP8266) && !defined(ESP32)
    print_bytecode(obj);
    printf("\n");
    print_heap(obj);
    printf("\n");
    print_varstack();
    printf("\n");
  #endif
#endif

#if defined(DEBUG) or defined(COVERALLS)
  printf("varstack: %d, memsize: %d\n", varsize, memsize);
  printf("heap expected %d, got %d\n", heapsize, getval(obj->heap->nrbytes));
  assert(heapsize >= getval(obj->heap->nrbytes));
  printf("bc expected %d, got %d\n", bcsize, getval(obj->bc.nrbytes));
  assert(bcsize == getval(obj->bc.nrbytes));
  printf("bcsize: %d, heapsize: %d\n", getval(obj->bc.nrbytes), getval(obj->heap->nrbytes));
#endif
/*LCOV_EXCL_STOP*/

/*LCOV_EXCL_START*/
#if defined(ESP8266) || defined(ESP32)
  timestamp.first = micros();
#else
  clock_gettime(CLOCK_MONOTONIC, &timestamp.first);
#endif
/*LCOV_EXCL_STOP*/

  if(rule_run(obj, 1) == -1) {
    return -1;
  }

/*LCOV_EXCL_START*/
#if defined(ESP8266) || defined(ESP32)
  timestamp.second = micros();

  logprintf_P(F("rule #%d was executed in %d microseconds"), getval(obj->nr), timestamp.second - timestamp.first);
  logprintf_P(F("bytecode: %d/%d, heap: %d/%d, stack: %d/%d bytes, varstack: %d/%d bytes, varstack slots: %d/127"),
    getval(obj->bc.nrbytes),
    getval(obj->bc.bufsize),
    getval(obj->heap->nrbytes),
    getval(obj->heap->bufsize),
    ((stack == NULL) ? 0 : getval(stack->nrbytes)),
    ((stack == NULL) ? 0 : getval(stack->bufsize)),
    ((varstack->nrbytes == 0) ? 0 : varstack->nrbytes),
    (varstack->bufsize),
	varstack->nrbytes / sizeof(struct vm_vchar_t)
  );
#else
  clock_gettime(CLOCK_MONOTONIC, &timestamp.second);

  printf("rule #%d was executed in %.6f seconds\n", obj->nr,
    ((double)timestamp.second.tv_sec + 1.0e-9*timestamp.second.tv_nsec) -
    ((double)timestamp.first.tv_sec + 1.0e-9*timestamp.first.tv_nsec));

  printf("bytecode: %d/%d, heap: %d/%d, stack: %d/%d bytes, varstack %d/%d bytes\n",
    getval(obj->bc.nrbytes),
    getval(obj->bc.bufsize),
    getval(obj->heap->nrbytes),
    getval(obj->heap->bufsize),
    ((stack == NULL) ? 0 : getval(stack->nrbytes)),
    ((stack == NULL) ? 0 : getval(stack->bufsize)),
    ((varstack->nrbytes == 0) ? 0 : varstack->nrbytes),
    (varstack->bufsize)
  );
#endif
/*LCOV_EXCL_STOP*/

  if(stack != NULL) {
/*LCOV_EXCL_START*/
    if((getval(stack->bufsize) % 4) != 0) {
#if defined(ESP8266) || defined(ESP32)
      Serial.printf("Rules AST not 4 byte aligned!\n");
#else
      printf("Rules AST not 4 byte aligned!\n");
#endif
      exit(-1);
/*LCOV_EXCL_STOP*/
    }
  }

  return 0;
}
