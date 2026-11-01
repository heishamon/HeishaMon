/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifdef __linux__
  #pragma GCC diagnostic ignored "-Wwrite-strings"

  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <unistd.h>
  #include <errno.h>
  #include <ctype.h>
  #include <string.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/time.h>
  #include <time.h>

  #include "webserver.h"
  #include "strncasestr.h"
  #include "strnstr.h"
  #include "unittest.h"
  #include "base64.h"
  #include "sha1.h"
#else
  #define LWIP_INTERNAL

  #include <Arduino.h>
  #include <WiFiClient.h>
  #include <WiFiServer.h>

  #define LWIP_SO_RCVBUF 1

  #include "strncasestr.h"
  #include "strnstr.h"

  #include "lwip/opt.h"
  #include "lwip/tcp.h"
  #include "lwip/inet.h"
  #include "lwip/dns.h"
  #include "lwip/init.h"
  #include "lwip/errno.h"

  #include "webserver.h"
  #include "base64.h"
  #include "sha1.h"

  #include <errno.h>
#endif

#define MIN(a,b) (((a)<(b))?(a):(b))
#ifndef ERR_OK
  #define ERR_OK 0
#endif

#if defined(ESP8266)
  #define loggingSerial Serial1
#elif defined(ESP32)
  #define loggingSerial Serial //usb serial CDC
#endif


void log_message(char *string);

struct webserver_client_t clients[WEBSERVER_MAX_CLIENTS];
#if defined(ESP8266) || defined(ESP32)
static tcp_pcb *async_server = NULL;
static WiFiServer sync_server(0);
#endif
static uint8_t *rbuffer = NULL;

/*
  safe write is necessary for esp8266 so it doesn't block
  on esp32 wifi is multithreaded, not necessary but also not implemented on availableforWrite so skip it there
*/
static int16_t safe_write(struct webserver_t *client, const uint8_t *buf, uint16_t len) {
#if defined(ESP8266)
  uint32_t start = millis();
  while(client->client->availableForWrite() == 0) {
    if((unsigned long)(millis() - start) > 1000) {
      // client is too slow, give up
      client->step = WEBSERVER_CLIENT_CLOSE;
      return -1;
    }
    yield();
  }
#endif
  return client->client->write(buf, len);
}

static int16_t safe_write_P(struct webserver_t *client, PGM_P buf, uint16_t len) {
#if defined(ESP8266)
  uint32_t start = millis();
  while(client->client->availableForWrite() == 0) {
    if((unsigned long)(millis() - start) > 1000) {
      // client is too slow, give up
      client->step = WEBSERVER_CLIENT_CLOSE;
      return -1;
    }
    yield();
  }
#endif
  return client->client->write_P((char *)buf, len);
}

#if defined(ESP8266) || defined(ESP32)
static uint16_t tcp_write_P(tcp_pcb *pcb, PGM_P buf, uint16_t len, uint8_t flags) {
  char *str = (char *)malloc(len+1);
  if(str == NULL) {
    loggingSerial.printf("Out of memory %s:#%d\n", __FUNCTION__, __LINE__);
    ESP.restart();
    exit(-1);
  }
  memset(str, 0, len+1);
  strncpy_P(str, buf, len);
  uint16_t ret = tcp_write(pcb, str, len, flags);
  free(str);
  return ret;
}
#endif

int16_t urldecode(const unsigned char *src, int src_len, unsigned char *dst, int dst_len, int is_form_url_encoded) {
  int i, j, a, b;

#define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')

  for(i = j = 0; i < src_len && j < dst_len - 1; i++, j++) {
    if(src[i] == '%' && i < src_len - 2 &&
      isxdigit(*(const unsigned char *)(src + i + 1)) &&
      isxdigit(*(const unsigned char *)(src + i + 2))) {
      a = tolower(*(const unsigned char *)(src + i + 1));
      b = tolower(*(const unsigned char *)(src + i + 2));
      dst[j] = (char)((HEXTOI(a) << 4) | HEXTOI(b));
      i += 2;
    } else if(is_form_url_encoded && src[i] == '+') {
      dst[j] = ' ';
    } else {
      dst[j] = src[i];
    }
  }

  dst[j] = '\0'; // Null-terminate the destination

  return i >= src_len ? j : -1;
}

static int webserver_parse_post(struct webserver_t *client, uint16_t size) {
  struct arguments_t args;
  unsigned char *ptrA = (unsigned char *)memchr(client->buffer, '=', size);
  unsigned char *ptrB = (unsigned char *)memchr(client->buffer, ' ', size);
  unsigned char *ptrC = (unsigned char *)memchr(client->buffer, '&', size);
  unsigned char *ptrD = (unsigned char *)strnstr(client->buffer, "\r\n", client->ptr);
  unsigned char *ptrE = NULL;
  unsigned char *ptrF = NULL;
  char c = '=';
  int16_t pos = 0;
  int16_t posA = WEBSERVER_BUFFER_SIZE+1, posB = WEBSERVER_BUFFER_SIZE+1;
  int16_t posC = WEBSERVER_BUFFER_SIZE+1, posD = WEBSERVER_BUFFER_SIZE+1;

  if(ptrD != NULL) {
    posD = ptrD - client->buffer;
  }
  if(ptrA != NULL) {
    posA = ptrA - client->buffer;
    if((ptrD == NULL) || (posD > posA)) {
      ptrE = ptrA;
    }
  }
  if(ptrB != NULL) {
    posB = ptrB - client->buffer;
  }
  if(ptrC != NULL) {
    posC = ptrC - client->buffer;
    if((ptrD == NULL) || (posD > posC)) {
      ptrF = ptrC;
    }
  }

  pos = MIN(posA, MIN(posB, posC));

  if(ptrA != NULL || ptrB != NULL || ptrC != NULL) {
    if(posB == pos) {
      c = ' ';
      ptrA = ptrB;
    }
    if(posC == pos) {
      c = '&';
      ptrA = ptrC;
    }

    /*
     * & delimiter
     */

    unsigned char *ptr1 = (unsigned char *)memchr(&client->buffer[pos+1], '&', size-(pos+1));
    if(ptr1 != NULL) {
      uint16_t pos1 = ptr1-client->buffer;

      int16_t pos2 = urldecode(client->buffer,
                pos + 1,
                client->buffer,
                pos + 1, 1);

      if(pos2 > -1) {
        client->buffer[pos2 - 1] = 0;
      } else {
        client->buffer[pos] = 0;
      }

      int16_t pos3 = urldecode(&client->buffer[pos+1],
                ((pos1-1)-pos) + 1,
                &client->buffer[pos+1],
                ((pos1-1)-pos) + 1, 1);

      if(pos3 > -1) {
        client->buffer[pos + 1 + pos3] = 0;
      } else {
        client->buffer[pos1] = 0;
      }

      args.name = &client->buffer[0];
      args.value = &client->buffer[pos+1];
      args.len = (pos1-1)-pos;
      if(pos3 > -1) {
        args.len = pos3 - 1;
      }

      if(client->callback != NULL) {
        if(client->callback(client, &args) == -1) {
          return -1; /*LCOV_EXCL_LINE*/
        }
      }

      if(pos2 > -1) {
        client->buffer[pos2-1] = c;
        client->buffer[pos] = ' ';
      } else {
        client->buffer[pos] = c;
      }
      if(pos3 > -1) {
        client->buffer[pos + 1 + pos3] = '&';
        client->buffer[pos1] = ' ';
      } else {
        client->buffer[pos1] = '&';
      }

      memmove(&client->buffer[0], &client->buffer[pos1+1], size-(pos1+1));
      client->ptr = size-(pos1+1);
      client->buffer[client->ptr] = 0;

      return 1;
    }

    if(client->readlen + size == client->totallen) {
      int16_t pos2 = urldecode(client->buffer,
                pos + 1,
                client->buffer,
                pos + 1, 1);

      if(pos2 > -1) {
        client->buffer[pos2 - 1] = 0;
      } else {
        client->buffer[pos] = 0;
      }

      int16_t pos3 = urldecode(&client->buffer[pos+1],
                (size - (pos + 1)) + 1,
                &client->buffer[pos+1],
                (size - (pos + 1)) + 1, 1);

      if(pos3 > -1) {
        client->buffer[pos + 1 + pos3] = 0;
      } else {
        client->buffer[size] = 0;
      }

      args.name = &client->buffer[0];
      args.value = &client->buffer[pos+1];
      args.len = size - (pos + 1);

      if(pos3 > -1) {
        args.len = pos3 - 1;
      }

      if(client->callback != NULL) {
        if(client->callback(client, &args) == -1) {
          return -1; /*LCOV_EXCL_LINE*/
        }
      }

      memmove(&client->buffer[0], &client->buffer[size], size);
      client->ptr = 0;
      client->buffer[client->ptr] = 0;

      return 0;
    }

    ptr1 = (unsigned char *)memrchr(client->buffer, '%', size);
    if(ptr1 != NULL) {
      uint16_t pos1 = ptr1 - client->buffer;
      /*
       * A encoded character always start with a
       * percentage mark followed by two numbers.
       * To properly decode an url we need to
       * keep those together.
       */
      if(pos1+2 >= WEBSERVER_BUFFER_SIZE) {
        int16_t pos2 = urldecode(client->buffer,
                  pos + 1,
                  client->buffer,
                  pos + 1, 1);

        if(pos2 > -1) {
          client->buffer[pos2 - 1] = 0;
        } else {
          client->buffer[pos] = 0;
        }

        int16_t pos3 = urldecode(&client->buffer[pos+1],
                  (pos1 - (pos + 1)) + 1,
                  &client->buffer[pos+1],
                  (pos1 - (pos + 1)) + 1, 1);

        client->buffer[pos1] = 0;

        args.name = &client->buffer[0];
        args.value = &client->buffer[pos+1];
        args.len = (pos1 - (pos + 1)) + 1;

        if(pos3 > -1) {
          args.len = pos3 - 1;
        }

        if(client->callback != NULL) {
          if(client->callback(client, &args) == -1) {
            return -1; /*LCOV_EXCL_LINE*/
          }
        }

        client->buffer[pos1] = '%';

        if(pos2 > -1) {
          client->buffer[pos2-1] = c;
          client->buffer[pos] = ' ';
          pos = pos2;
        } else {
          client->buffer[pos] = c;
        }

        memmove(&client->buffer[pos+1], &client->buffer[pos1], (size-pos1));
        client->ptr = (size - (pos1 - pos)) + 1;
        client->buffer[client->ptr] = 0;

        return 1;
      }
    }

    if((client->ptr >= WEBSERVER_BUFFER_SIZE || ptrD != NULL) && strncmp((char *)&client->buffer[pos+1], "HTTP/1.", 7) != 0) {
      /*
       * GET end delimiter before HTTP/1.1
       */

      ptr1 = (unsigned char *)memchr(&client->buffer[pos+1], ' ', size - (pos + 1));
      unsigned char *ptr2 = (unsigned char *)memchr(&client->buffer[pos], '&', size - (pos));
      char d = ' ';

      if(ptr1 != NULL || ptr2 != NULL) {
        if((ptr1 == NULL && ptr2 != NULL) || (ptr1 != NULL && ptr2 != NULL && ptr2 < ptr1)) {
          ptr1 = ptr2;
          d = '&';
        }
        uint16_t pos1 = ptr1 - client->buffer;
        int16_t pos2 = urldecode(client->buffer,
                  pos + 1,
                  client->buffer,
                  pos + 1, 1);

        if(pos2 > -1) {
          client->buffer[pos2 - 1] = 0;
        } else {
          client->buffer[pos] = 0;
        }

        int16_t pos3 = -1;
        if(d == ' ') {
          pos3 = urldecode(&client->buffer[pos+1],
                    (pos1 - (pos + 1)) + 1,
                    &client->buffer[pos+1],
                    (pos1 - (pos + 1)) + 1, 1);

          if(pos3 > -1) {
            /*
             * If the data was urlencoded, the final data
             * has a smaller length. Therefor the 0 delimiter
             * must be placed to that new position.
             */
            client->buffer[pos+pos3] = 0;
          } else {
            client->buffer[pos1] = 0;
          }
        }

        args.name = &client->buffer[0];
        if(d == ' ') {
          args.value = &client->buffer[pos+1];
          args.len = size - (pos + 1);
          if(pos3 > -1) {
            args.len = pos3 - 1;
          }
        } else {
          args.value = NULL;
          args.len = 0;
        }

        if(client->callback != NULL) {
          if(client->callback(client, &args) == -1) {
            return -1; /*LCOV_EXCL_LINE*/
          }
        }

        if(pos3 > -1) {
          /*
           * If the data was urlencoded, the final data is
           * has a smaller length. Therefor, the data following
           * the original urlencoded data must be moved forward.
           * The size that was read until now must be decreased
           * as well.
           */
          memmove(&client->buffer[pos+pos3], &client->buffer[pos1], client->ptr-(pos3+pos));
          size -= (pos1-(pos3+pos));
          pos1 = (pos+pos3);
          client->buffer[pos1] = d;
        } else {
          client->buffer[pos1] = d;
        }

        if(d == '&') {
          pos1++;
        }

        memmove(&client->buffer[0], &client->buffer[pos1], size-(pos1));
        client->ptr = size-(pos1);
        client->buffer[client->ptr] = 0;

      } else {
        int16_t pos2 = urldecode(client->buffer,
                  pos + 1,
                  client->buffer,
                  pos + 1, 1);

        if(pos2 > -1) {
          client->buffer[pos2 - 1] = 0;
        } else {
          client->buffer[pos] = 0;
        }

        int16_t pos3 = urldecode(&client->buffer[pos+1],
                  size - (pos + 1) + 1,
                  &client->buffer[pos+1],
                  size - (pos + 1) + 1, 1);

        args.name = &client->buffer[0];
        args.value = &client->buffer[pos+1];
        args.len = size - (pos + 1);

        if(pos3 > -1) {
          args.len = pos3 - 1;
        }

        if(client->callback != NULL) {
          if(client->callback(client, &args) == -1) {
            return -1; /*LCOV_EXCL_LINE*/
          }
        }

        if(pos2 > -1) {
          client->buffer[pos2-1] = c;
          client->buffer[pos] = d;
          pos = pos2;
        } else {
          client->buffer[pos] = c;
        }

        memmove(&client->buffer[pos+1], &client->buffer[size], size-(pos+1));
        client->ptr = (pos+1);

        client->buffer[client->ptr] = 0;

        return 1;
      }
    }

    if(ptrE == NULL && ptrF == NULL && ptrA != NULL && (posB == WEBSERVER_BUFFER_SIZE+1 || posB > 0)) {
      uint16_t pos1 = ptrA-client->buffer;

      int16_t pos2 = urldecode(client->buffer,
                pos + 1,
                client->buffer,
                pos + 1, 1);

      if(pos2 > -1) {
        client->buffer[pos2 - 1] = 0;
      } else {
        client->buffer[pos] = 0;
      }

      args.name = &client->buffer[0];
      args.value = NULL;
      args.len = 0;

      if(client->callback != NULL) {
        if(client->callback(client, &args) == -1) {
          return -1; /*LCOV_EXCL_LINE*/
        }
      }

      if(pos2 > -1) {
        client->buffer[pos2-1] = c;
        client->buffer[pos] = ' ';
      } else {
        client->buffer[pos] = c;
      }

      memmove(&client->buffer[0], &client->buffer[pos1], size-(pos1));
      client->ptr = size-(pos1);
      client->buffer[client->ptr] = 0;

      return 0;
    }

  }

  return 0;
}

int8_t http_parse_request(struct webserver_t *client, uint8_t **buf, uint16_t *len) {
  uint16_t hasread = MIN(WEBSERVER_BUFFER_SIZE-client->ptr, *len);
  uint16_t lastptr = 0xFFFF, lastlen = 0xFFFF;
  uint8_t lastsubstep = 0xFF;
  while((*len > 0) || (strnstr(client->buffer, "\r\n\r\n", client->ptr) != NULL)) {
    /*
     * A malformed request can leave the parser without any
     * way to progress while the loop condition stays true
     * (e.g. a complete request still in the buffer). Bail
     * out instead of spinning forever and blocking the
     * main loop.
     */
    if(client->ptr == lastptr && *len == lastlen && client->substep == lastsubstep) {
      return -1;
    }
    lastptr = client->ptr;
    lastlen = *len;
    lastsubstep = client->substep;

    hasread = MIN(WEBSERVER_BUFFER_SIZE-client->ptr, (*len));
    memcpy(&client->buffer[client->ptr], &(*buf)[0], hasread);

    client->ptr += hasread;
    memmove(&(*buf)[0], &(*buf)[hasread], (*len)-hasread);

    *len -= hasread;

    /*
     * Request method
     */
    if(client->substep == 0) {
      if(memcmp_P(client->buffer, PSTR("GET "), 4) == 0) {
        client->method = 0;
        if(client->callback != NULL) {
          client->step = WEBSERVER_CLIENT_REQUEST_METHOD;
          if(client->callback != NULL) {
            if(client->callback(client, (void *)"GET") == -1) {
              client->step = WEBSERVER_CLIENT_CLOSE;
              return -1;
            }
          }
          client->step = WEBSERVER_CLIENT_READ_HEADER;
        }
        memmove(&client->buffer[0], &client->buffer[4], client->ptr-4);
        client->ptr -= 4;
        client->substep = 1;
      }
      if(memcmp_P(client->buffer, PSTR("POST "), 5) == 0) {
        client->method = 1;
        client->reqtype = 0;
        client->step = WEBSERVER_CLIENT_REQUEST_METHOD;
        if(client->callback != NULL) {
          if(client->callback(client, (void *)"POST") == -1) {
            client->step = WEBSERVER_CLIENT_CLOSE;
            return -1;
          }
        }
        client->step = WEBSERVER_CLIENT_READ_HEADER;
        memmove(&client->buffer[0], &client->buffer[5], client->ptr-5);
        client->ptr -= 5;
        client->substep = 1;
      }
      if(client->substep == 0) {
        /*
         * Unsupported request method (HEAD, OPTIONS, binary
         * probes from network scanners, etc.)
         */
        if(client->ptr >= 5 || memchr(client->buffer, '\n', client->ptr) != NULL) {
          return -1;
        }
        return 1;
      }
    }
      /*
       * Request URI
       */
    if(client->substep == 1) {
      unsigned char *ptr1 = (unsigned char *)memchr(client->buffer, '?', client->ptr);
      unsigned char *ptr2 = (unsigned char *)memchr(client->buffer, ' ', client->ptr);
      if(ptr2 == NULL || (ptr1 != NULL && ptr2 > ptr1)) {
        if(ptr1 == NULL) {
          if(client->ptr == WEBSERVER_BUFFER_SIZE) {
            // Request URI two long
            return -1;
          } else {
            return 1;
          }
        } else {
          uint16_t pos = ptr1-client->buffer;
          client->buffer[pos] = 0;
          client->substep = 2;
          client->step = WEBSERVER_CLIENT_REQUEST_URI;
          if(client->callback != NULL) {
            if(client->callback(client, client->buffer) == -1) {
              client->step = WEBSERVER_CLIENT_CLOSE;
              return -1;
            }
          }
          memmove(&client->buffer[0], &client->buffer[pos+1], client->ptr-(pos+1));
          client->ptr -= (pos+1);
        }
      } else {
        uint16_t pos = ptr2-client->buffer;
        client->buffer[pos] = 0;
        client->substep = 2;
        client->step = WEBSERVER_CLIENT_REQUEST_URI;
        if(client->callback != NULL) {
          if(client->callback(client, client->buffer) == -1) {
            client->step = WEBSERVER_CLIENT_CLOSE;
            return -1;
          }
        }
        client->buffer[pos] = ' ';
        memmove(&client->buffer[0], &client->buffer[pos], client->ptr-(pos));
        client->ptr -= pos;
      }
    }
      /*
       * Parse GET/POST
       */	
    if(client->substep == 2) {
      client->step = WEBSERVER_CLIENT_ARGS;
      int ret = webserver_parse_post(client, client->ptr);
      client->step = WEBSERVER_CLIENT_READ_HEADER;

      if(ret == -1) {
        return -1; /*LCOV_EXCL_LINE*/
      }

      if(ret == 1) {
        continue;
      }

      if(client->ptr >= 4) {
        if(memcmp_P(client->buffer, PSTR(" HTTP/1."), 8) == 0) {
          client->substep = 3;
        } else {
          continue;
        }
      }
    }
      /*
       * Cleanup request from buffer
       */	
    if(client->substep == 3) {
      uint16_t i = 0;
      while(i < client->ptr-2) {
        if(memcmp_P(&client->buffer[i], PSTR("\r\n"), 2) == 0) {
          memmove(&client->buffer[0], &client->buffer[i+2], client->ptr-(i+2));
          client->ptr -= (i + 2);
          client->substep = 4;
          break;
        }
        i++;
      }
    }
      /*
       * Get headers
       */	
    if(client->substep == 4) {
      unsigned char *ptrEnd = (unsigned char *)strnstr(client->buffer, "\r\n\r\n", client->ptr);
      uint16_t posEnd = client->ptr;
      if(ptrEnd != NULL) {
        posEnd = (ptrEnd-client->buffer) + 4;
      } 
      unsigned char *ptr = (unsigned char *)memchr(client->buffer, ':', posEnd);

      while(ptr != NULL) {
        struct arguments_t args;
        uint16_t i = ptr-client->buffer, x = 0;
        client->buffer[i] = 0;
        args.name = &client->buffer[0];
        args.value = NULL;
        x = i;
        i++;
        /*
         * Make sure we can at least compare
         * the double \r\n\r\n at the end
         * of the header
         */
        while(i <= client->ptr-4) {
          if(memcmp_P(&client->buffer[i], PSTR("\r\n"), 2) == 0 ||
             (client->ptr == WEBSERVER_BUFFER_SIZE && i == WEBSERVER_BUFFER_SIZE-4)) {
            while(client->buffer[x+1] == ' ') {
              x++;
            }
            args.value = &client->buffer[x+1];
            args.len = (i-x)-1;
            if((client->ptr == WEBSERVER_BUFFER_SIZE && i == WEBSERVER_BUFFER_SIZE-4)) {
              args.len += 2;
            }

            if(memcmp_P(args.name, PSTR("Content-Length"), 14) == 0) {
              char tmp[args.len+1];
              memset(&tmp, 0, args.len+1);
              memcpy(tmp, &client->buffer[x+1], args.len);
              client->totallen = atoi(tmp);
            }
            if(memcmp_P(args.name, PSTR("Sec-WebSocket-Version"), 21) == 0) {
              client->is_websocket = 1;
            }
            if(memcmp_P(args.name, PSTR("Sec-WebSocket-Key"), 17) == 0) {
              char tmp[args.len+1];
              memset(&tmp, 0, args.len+1);
              memcpy(tmp, &client->buffer[x+1], args.len);
              if((client->data.websockkey = strdup(tmp)) == NULL) {
#if defined(ESP8266) || defined(ESP32)
                loggingSerial.printf("Out of memory %s:#%d\n", __FUNCTION__, __LINE__);
                ESP.restart();
                exit(-1);
#endif
              }
            }
            if(memcmp_P(args.name, PSTR("Content-Type"), 12) == 0) {
              if(strncasestr(&client->buffer[x+1], "multipart/form-data", client->ptr-(x+1)) != NULL) {
                client->reqtype = 1;
                char tmp[args.len+1];
                memset(&tmp, 0, args.len+1);
                memcpy(tmp, &client->buffer[x+1], args.len);
                {
                  char *ptr = strstr(tmp, "boundary=");
                  uint8_t pos = (ptr-tmp)+strlen("boundary=");
                  memmove(&tmp[0], &tmp[pos], args.len-pos);
                  tmp[args.len-pos] = 0;
                  if((client->data.boundary = strdup(tmp)) == NULL) {
#if defined(ESP8266) || defined(ESP32)
                    loggingSerial.printf("Out of memory %s:#%d\n", __FUNCTION__, __LINE__);
                    ESP.restart();
                    exit(-1);
#endif
                  }
                }
              }
            }
            client->step = WEBSERVER_CLIENT_HEADER;
            if(client->callback != NULL) {
              if(client->callback(client, &args) == -1) {
                client->step = WEBSERVER_CLIENT_CLOSE;
                return -1;
              }
            }
            client->step = WEBSERVER_CLIENT_READ_HEADER;

            client->buffer[i] = 0;
            if((client->ptr == WEBSERVER_BUFFER_SIZE && i == WEBSERVER_BUFFER_SIZE-4)) {
              memmove(&client->buffer[x], &client->buffer[i+2], client->ptr-(i+2));
              client->buffer[x-1] = ':';
              client->ptr -= (i + 2 - x);
              posEnd -= (i + 2 - x);
            } else {
              memmove(&client->buffer[0], &client->buffer[i+2], client->ptr-(i+2));
              client->ptr -= (i + 2);
              posEnd -= (i + 2);
            }
            break;
          }
          i++;
        }
        if(args.value == NULL) {
          client->buffer[x] = ':';
          break;
        }
        ptr = (unsigned char *)memchr(client->buffer, ':', posEnd);
      }

      if(client->ptr >= 2 && memcmp_P(client->buffer, PSTR("\r\n"), 2) == 0) {
        memmove(&client->buffer[0], &client->buffer[2], client->ptr-2);
        client->ptr -= 2;
        client->readlen = 0;
        if(client->ptr == 0 && *len > 0) {
          client->substep = 5;
          continue;
        }
        return 0;
      }
    }
    if(client->substep == 5) {
      return 0;
    }
  }

  return 1;
}

int http_parse_body(struct webserver_t *client, char *buf, uint16_t len) {
  uint16_t hasread = MIN(WEBSERVER_BUFFER_SIZE-client->ptr, len);
  uint16_t pos = 0;

  while(1) {
    if(pos < len) {
      hasread = MIN(WEBSERVER_BUFFER_SIZE-client->ptr, len-pos);;
      memcpy(&client->buffer[client->ptr], &buf[pos], hasread);
      client->ptr += hasread;
      pos += hasread;
    }

    uint16_t toread = client->ptr;
    int ret = webserver_parse_post(client, client->ptr);
    if(ret == 1 || ret == 0) {
      client->readlen += (toread - client->ptr);
    }

    if(ret == -1) {
      return -1; /*LCOV_EXCL_LINE*/
    }
    if(ret == 0) {
      break;
    }
  }

  return 0;
}

char *strnstr(const char *haystack, const char *needle, size_t len) {
  int i;
  size_t needle_len;

  if(0 == (needle_len = strnlen(needle, len))) {
    return (char *)haystack;
  }

  for(i=0; i<=(int)(len-needle_len); i++) {
    if((haystack[0] == needle[0]) && (0 == strncmp(haystack, needle, needle_len))) {
      return (char *)haystack;
    }

    haystack++;
  }
  return NULL;
}

int http_parse_multipart_body(struct webserver_t *client, unsigned char *buf, uint16_t len) {
  uint16_t hasread = MIN(WEBSERVER_BUFFER_SIZE-client->ptr, len);
  uint16_t rpos = 0, loop = 1;
  while((rpos < len) || ((loop == 1) && (client->ptr > 0))) {
    hasread = MIN(WEBSERVER_BUFFER_SIZE-client->ptr, len-rpos);
    memcpy(&client->buffer[client->ptr], &buf[rpos], hasread);
    client->ptr += hasread;
    rpos += hasread;
    loop = 1;
	
    while(loop) {
      switch(client->substep) {
        // Boundary
        case 0: {
          unsigned char *ptr = strnstr(client->buffer, client->data.boundary, client->ptr);
          unsigned char *ptr1 = (unsigned char *)memchr(client->buffer, '=', client->ptr);
          uint16_t pos1 = 0;
          if(ptr1 != NULL) {
            pos1 = (ptr1-client->buffer)+1;
          }
          if(ptr != NULL) {
            uint16_t pos = (ptr-client->buffer)+strlen(client->data.boundary);
			
            if(pos1 > pos) {
              /*
               * Only compensate for the key at the
               * beginning of the buffer when at least
               * one key has been encountered. This is
               * the case when the boundary is placed
               * after the key.
               */
              pos1 = 0;
            }
            if(pos+1 <= client->ptr) {
              if(client->buffer[pos] == '\r' && client->buffer[pos+1] == '\n') {
                memmove(&client->buffer[0], &client->buffer[pos+1], client->ptr-(pos+1));
                client->ptr = client->ptr-(pos+1);
                client->buffer[client->ptr] = 0;
                /*
                 * pos1 is NOT subtracted here: it exists to compensate for a
                 * field-name marker that case 4/case 6's short-name path used
                 * to (wrongly) credit into readlen. Now that those cases
                 * correctly exclude the marker themselves (see their fixes
                 * below), there is nothing left here to compensate for -
                 * subtracting pos1 would double-discount it and desync
                 * readlen from the true byte count whenever a field's name
                 * marker is still retained in the buffer.
                 */
                client->readlen += (pos+1);
                client->substep = 1;
              }
            }
            if(pos+3 <= client->ptr) {
              if(client->buffer[pos] == '-' && client->buffer[pos+1] == '-' &&
                client->buffer[pos+2] == '\r' && client->buffer[pos+3] == '\n') {
                // See the comment on the mid-boundary branch above for why
                // pos1 is not subtracted here either.
                client->readlen += (pos+4);
                if(client->readlen == client->totallen) {
                  if(client->data.boundary != NULL) {
                    free(client->data.boundary);
                    client->data.boundary = NULL;
                  }
                  return 0;
                } else {
                  // Error, content length does not match end boundary
                  return -1;
                }
              }
            }
          } else if(client->ptr < WEBSERVER_BUFFER_SIZE) {
            loop = 0;
          } else {
            /*
             * We encountered the boundary delimiter, but
             * it wasn't the one from this request, but it
             * was part of the POST body
             */
            client->substep = 8;
          }
          if(client->substep == 0) {
            loop = 0;
          }
        } break;
        // Content-Disposition
        case 1: {
          unsigned char *ptr = strncasestr(client->buffer, "content-disposition:", client->ptr);
          if(ptr != NULL) {
            uint16_t pos = (ptr-client->buffer)+strlen("content-disposition:");
            while(client->buffer[pos++] == ' ');
            pos--;
            memmove(&client->buffer[0], &client->buffer[pos], client->ptr-(pos));
            client->ptr = client->ptr-(pos);
            client->buffer[client->ptr] = 0;
            client->readlen += pos;
            client->substep = 2;
          } else {
            loop = 0;
          }
        } break;
        // End of content-disposition
        case 2: {
          unsigned char *ptr = (unsigned char *)memchr(client->buffer, ';', client->ptr);
          if(ptr != NULL) {
            uint16_t pos = (ptr-client->buffer+1);
            while(client->buffer[pos++] == ' ');
            pos--;
            memmove(&client->buffer[0], &client->buffer[pos], client->ptr-(pos));
            client->ptr -= pos;
            client->buffer[client->ptr] = 0;
            client->readlen += pos;
            client->substep = 3;
          } else {
            loop = 0;
          }
        } break;
        // Name
        case 3: {
          unsigned char *ptr = strncasestr(client->buffer, "name=\"", client->ptr);
          if(ptr != NULL) {
            uint16_t pos = (ptr-client->buffer)+strlen("name=\"");
            memmove(&client->buffer[0], &client->buffer[pos], client->ptr-(pos));
            client->ptr = client->ptr-(pos);
            client->readlen += pos;
            client->substep = 6;
          } else {
            loop = 0;
          }
        } break;
        // Filename etc.
        case 4: {
          unsigned char *ptr = strncasestr(client->buffer, "\";", client->ptr);
          if(ptr != NULL) {
            uint16_t pos = (ptr-client->buffer);
            // First try to find \r\n\r\n (end of all headers)
            unsigned char *ptr1 = strnstr(&client->buffer[pos], "\r\n\r\n", client->ptr-pos);
            if(ptr1 != NULL) {
                // Double CRLF found - skip directly to data
                client->buffer[pos++] = '=';
                uint16_t pos1 = (ptr1-client->buffer);
                uint16_t newlen = client->ptr-((pos1+4)-pos);
                /*
                 * The memmove must only copy the bytes that are actually
                 * still valid past pos1+4 (client->ptr-(pos1+4)), NOT
                 * newlen: newlen is the FINAL ptr value, which already
                 * accounts for the retained "name=" marker staying at
                 * buffer position 0..pos-1. Using newlen as the copy length
                 * here reads past the end of the valid buffered content
                 * into stale memory.
                 */
                memmove(&client->buffer[pos], &client->buffer[pos1+4], client->ptr-(pos1+4));
                client->ptr = newlen;
                /*
                 * Only credit the actual header bytes consumed
                 * ((pos1+4)-pos), not the full (pos1+4): the retained
                 * "name=" marker (0..pos-1) stays physically in the buffer
                 * for case 7/8 to use later, so it must NOT also be counted
                 * as "consumed" here - doing so double-counts those bytes
                 * (once via readlen here, again via ptr since they're still
                 * buffered).
                 */
                client->readlen += ((pos1+4)-pos);
                client->substep = 7;  // skip case 5 entirely, go straight to data
            } else {
                // Single \r\n - if more headers follow, go to case 5
                ptr1 = strncasestr(&client->buffer[pos], "\r\n", client->ptr-pos);
                if(ptr1 != NULL) {
                    if (((ptr1-client->buffer) + 4) >= client->ptr) {
                        //CRLF on end of buffer, wait for two more to make sure we are not at end of header
					    loop = 0;
                    } else {
                        client->buffer[pos++] = '=';
                        uint16_t pos1 = (ptr1-client->buffer);
                        uint16_t newlen = client->ptr-((pos1+2)-pos);
                        // See the double-CRLF branch above for why the
                        // memmove length and readlen credit both exclude pos.
                        memmove(&client->buffer[pos], &client->buffer[pos1+2], client->ptr-(pos1+2));
                        client->ptr = newlen;
                       client->readlen += ((pos1+2)-pos);
                        client->substep = 5;
                    }
                  } else {
                      client->substep = 6;
                }
            }
          } else {
            loop = 0;
          }
        } break;
        // Content-type
        case 5: {
          unsigned char *ptr = (unsigned char *)memchr(client->buffer, '=', client->ptr);
          if(ptr != NULL) {
            uint16_t pos = (ptr-client->buffer)+1;

            if((client->ptr - pos) >= 4) {
              unsigned char *ptr1 = strnstr(&client->buffer[pos], "\r\n\r\n", client->ptr-pos);
              if(ptr1 != NULL) {
                uint16_t pos1 = (ptr1-client->buffer)+4;
                uint16_t newlen = client->ptr-(pos1-pos);
                memmove(&client->buffer[pos], &client->buffer[pos1], newlen);
                client->ptr = newlen;
                client->readlen += (pos1-pos);
                client->substep = 7;
              } else {
                loop = 0;
              }
            } else {
              loop = 0;
            }
          } else {
            loop = 0;
          }
        } break;
        // Name
        case 6: {
          if(client->ptr >= 2) {
            unsigned char *ptr = strnstr(client->buffer, "\";", client->ptr);
            if(ptr != NULL) {
              unsigned char *ptr1 = strnstr(client->buffer, "\r\n", client->ptr);
              if(ptr1 != NULL) {
                client->substep = 4;
              } else {
                loop = 0;
              }
            } else {
              unsigned char *ptr1 = strnstr(client->buffer, "\"\r\n", client->ptr);
              if(ptr1 != NULL) {
                uint16_t pos = (ptr1-client->buffer);
                /*
                 * Since we're increasing the pos
                 * right after, check for 5 positions
                 */
                if((client->ptr - pos) >= 5) {
                  client->buffer[pos++] = '=';;
                  unsigned char *ptr2 = strnstr(&client->buffer[pos], "\r\n\r\n", client->ptr-pos);
                  if(ptr2 != NULL) {
                    uint16_t pos1 = (ptr2-client->buffer)+4;

                    // See the case 4 double-CRLF branch for why pos is
                    // excluded from the readlen credit here too.
                    memmove(&client->buffer[pos], &client->buffer[pos1], client->ptr-pos1);
                    client->ptr -= (pos1-pos);
                    client->readlen += (pos1-pos);
                    client->substep = 7;
                  } else {
                    loop = 0;
                  }
                } else {
                  loop = 0;
                }
              } else {
                loop = 0;
              }
            }
          } else {
            loop = 0;
          }
        } break;
        // Value
        case 8:
        case 7: {
          unsigned char *ptr = strnstr(client->buffer, "\r\n--", client->ptr);
          if(ptr != NULL && client->substep != 8) {
            uint16_t pos = (ptr-client->buffer);
            ptr = (unsigned char *)memchr(client->buffer, '=', client->ptr);
            uint16_t vlen = 0;

            if(ptr != NULL) {
              vlen = (ptr-client->buffer);
            } else {
              // error
              return -1; /*LCOV_EXCL_LINE*/
            }
			struct arguments_t args;
			client->buffer[vlen] = 0;

			args.name = &client->buffer[0];
			args.value = &client->buffer[vlen+1];
			args.len = pos-(vlen+1);

			if(client->callback != NULL) {
				uint8_t ret = client->callback(client, &args);
				if(ret == -1) {
					return -1;
				}
			}

			client->buffer[vlen] = '=';
			memmove(&client->buffer[vlen+1], &client->buffer[pos], client->ptr-pos);
			client->readlen += (pos-(vlen+1));
			client->ptr -= (pos-(vlen+1));
            client->substep = 0;
		  } else if(client->ptr == WEBSERVER_BUFFER_SIZE) {
            uint8_t ending = 0;
            uint8_t dash = 0;
            /*
             * Double check that the CR / LN don't belong
             * to the boundary delimiter.
             */
            if(strncmp((char *)&client->buffer[client->ptr-1], "\r", 1) == 0) {
              ending = 1;
            }
            if(strncmp((char *)&client->buffer[client->ptr-2], "\r\n", 2) == 0) {
              ending = 2;
            }
            if(strncmp((char *)&client->buffer[client->ptr-3], "\r\n\r", 3) == 0) {
              ending = 3;
            }
            if(strncmp((char *)&client->buffer[client->ptr-3], "\r\n-", 3) == 0) {
              ending = 3;
              dash = 1;
            }
            if(client->substep == 8) {
              client->substep = 7;
            }
            unsigned char *ptr = (unsigned char *)memchr(client->buffer, '=', client->ptr);
            if(ptr != NULL) {
              uint16_t pos = (ptr-client->buffer);

              struct arguments_t args;
              client->buffer[pos] = 0;

              args.name = &client->buffer[0];
              args.value = &client->buffer[pos+1];
              args.len = (client->ptr-ending)-(pos+1);
		  
              if(client->callback != NULL) {
                uint8_t ret = client->callback(client, &args);
                if(ret == -1) {
                  return -1;
                }
              }
              client->buffer[pos] = '=';
              if(ending == 1) {
                client->buffer[pos+1] = '\r';
              }
              if(ending == 2) {
                client->buffer[pos+1] = '\r';
                client->buffer[pos+2] = '\n';
              }
              if(ending == 3) {
                client->buffer[pos+1] = '\r';
                client->buffer[pos+2] = '\n';
                client->buffer[pos+3] = '\r';
                if(dash) {
                  client->buffer[pos+3] = '-';
                }
              }
              client->readlen += ((client->ptr-(pos+1))-ending);
              client->ptr = pos+1+ending;
              loop = 0;
            } else {
              // error
              return -1;
            }
          } else {
			loop = 0;
		  }
        } break;
      }
    }
  }

  return 0;
}

#ifdef __linux__
static char *code_to_text(uint16_t code) {
#else
static PGM_P code_to_text(uint16_t code) {
#endif
  /* LCOV_EXCL_START*/
  switch(code) {
    case 100:
      return PSTR("Continue");
    case 101:
      return PSTR("Switching Protocols");
    /* LCOV_EXCL_STOP*/
    case 200:
      return PSTR("OK");
    /* LCOV_EXCL_START*/
    case 201:
      return PSTR("Created");
    case 202:
      return PSTR("Accepted");
    case 203:
      return PSTR("Non-Authoritative Information");
    case 204:
      return PSTR("No Content");
    case 205:
      return PSTR("Reset Content");
    case 206:
      return PSTR("Partial Content");
    case 300:
      return PSTR("Multiple Choices");
    /* LCOV_EXCL_STOP*/
    case 301:
      return PSTR("Moved Permanently");
    /* LCOV_EXCL_START*/
    case 302:
      return PSTR("Found");
    case 303:
      return PSTR("See Other");
    case 304:
      return PSTR("Not Modified");
    case 305:
      return PSTR("Use Proxy");
    case 307:
      return PSTR("Temporary Redirect");
    case 400:
      return PSTR("Bad Request");
    case 401:
      return PSTR("Unauthorized");
    case 402:
      return PSTR("Payment Required");
    case 403:
      return PSTR("Forbidden");
    case 404:
      return PSTR("Not Found");
    case 405:
      return PSTR("Method Not Allowed");
    case 406:
      return PSTR("Not Acceptable");
    case 407:
      return PSTR("Proxy Authentication Required");
    case 408:
      return PSTR("Request Timeout");
    case 409:
      return PSTR("Conflict");
    case 410:
      return PSTR("Gone");
    case 411:
      return PSTR("Length Required");
    case 412:
      return PSTR("Precondition Failed");
    case 413:
      return PSTR("Request Entity Too Large");
    case 414:
      return PSTR("URI Too Long");
    case 415:
      return PSTR("Unsupported Media Type");
    case 416:
      return PSTR("Range not satisfiable");
    case 417:
      return PSTR("Expectation Failed");
    case 500:
      return PSTR("Internal Server Error");
    case 501:
      return PSTR("Not Implemented");
    case 502:
      return PSTR("Bad Gateway");
    case 503:
      return PSTR("Service Unavailable");
    case 504:
      return PSTR("Gateway Timeout");
    case 505:
      return PSTR("HTTP Version not supported");
    default:
      return PSTR("");
  }
  /* LCOV_EXCL_STOP*/
}

static uint16_t webserver_create_header(struct webserver_t *client, uint16_t code, char *mimetype, uint16_t len) {
  uint16_t i = 0;
  unsigned char buffer[512], *p = buffer;
  memset(buffer, '\0', sizeof(buffer));

  i += snprintf_P((char *)&p[i], sizeof(buffer), PSTR("HTTP/1.1 %d %s\r\n"), code, code_to_text(code));
  if(client->callback != NULL) {
    client->step = WEBSERVER_CLIENT_CREATE_HEADER;
    struct header_t header;
    header.buffer = &p[i];
    header.ptr = i;

    if(client->callback(client, &header) == -1) {
      if(strstr_P((char *)&p[i], PSTR("\r\n\r\n")) == NULL) {
        if(strstr((char *)&p[i], PSTR("\r\n")) != NULL) {
          header.ptr += snprintf_P((char *)&p[header.ptr], sizeof(buffer)-header.ptr, PSTR("\r\n"));
        } else {
          header.ptr += snprintf_P((char *)&p[header.ptr], sizeof(buffer)-header.ptr, PSTR("\r\n\r\n"));
        }
      }
      client->step = WEBSERVER_CLIENT_WRITE;
      i = header.ptr;
      return i;
    }

    if(header.ptr > i && strstr_P((char *)&p[i], PSTR("\r\n")) == NULL) {
      header.ptr += snprintf((char *)&p[header.ptr], sizeof(buffer)-header.ptr, PSTR("\r\n"));
    }
    i = header.ptr;
    client->step = WEBSERVER_CLIENT_WRITE;
  }
  i += snprintf_P((char *)&p[i], sizeof(buffer) - i, PSTR("Server: HeishaMon\r\n"));
  i += snprintf_P((char *)&p[i], sizeof(buffer) - i, PSTR("Keep-Alive: timeout=15, max=100\r\n"));
  i += snprintf_P((char *)&p[i], sizeof(buffer) - i, PSTR("Content-Type: %s\r\n"), mimetype);
  i += snprintf_P((char *)&p[i], sizeof(buffer) - i, PSTR("Content-Length: %d\r\n\r\n"), len);


  if(client->async == 1) {
    tcp_write(client->pcb, &buffer, i, TCP_WRITE_FLAG_COPY);
    tcp_output(client->pcb);
  } else {
    if(safe_write(client,buffer, i) > 0) {
      if(client->is_websocket == 0) {
        client->lastseen = millis();
      }
    }
  }

  return i;
}

static int webserver_process_send(struct webserver_t *client) {
  struct sendlist_t *tmp = NULL;
  uint16_t cpylen = client->totallen, i = 0, cpyptr = client->ptr;
  unsigned char cpy[client->totallen+1];
  memset(&cpy, 0, client->totallen+1);

#if WEBSERVER_MAX_SENDLIST == 0
  tmp = client->sendlist;
#else
  uint8_t x = 0, y = 0;
  for(x=0;x<WEBSERVER_MAX_SENDLIST;x++) {
    if(client->sendlist[x].data.ptr != NULL) {
      tmp = &client->sendlist[x];
      break;
    }
  }
#endif

  if(client->chunked == 1) {
    while(tmp != NULL && cpylen > 0) {
      if(cpyptr == 0) {
        if(cpylen >= tmp->size) {
          cpyptr += tmp->size;
          cpylen -= tmp->size;
#if WEBSERVER_MAX_SENDLIST == 0
          tmp = tmp->next;
#else
          tmp = NULL;
          for(y=x+1;y<WEBSERVER_MAX_SENDLIST;y++) {
            if(client->sendlist[y].data.ptr != NULL) {
              tmp = &client->sendlist[y];
              x = y;
              break;
            }
          }
#endif
          cpyptr = 0;
        } else {
          cpyptr += cpylen;
          cpylen = 0;
        }
      } else if(cpyptr+cpylen >= tmp->size) {
        cpylen -= (tmp->size-cpyptr);
#if WEBSERVER_MAX_SENDLIST == 0
          tmp = tmp->next;
#else
          tmp = NULL;
          for(y=x+1;y<WEBSERVER_MAX_SENDLIST;y++) {
            if(client->sendlist[y].data.ptr != NULL) {
              tmp = &client->sendlist[y];
              x = y;
              break;
            }
          }
#endif
        cpyptr = 0;
      } else {
        cpyptr += cpylen;
        cpylen = 0;
      }
    }

    unsigned char chunk_size[12];
    size_t n = snprintf_P((char *)chunk_size, sizeof(chunk_size), PSTR("%X\r\n"), client->totallen - cpylen);

    if(client->async == 1) {
      tcp_write(client->pcb, chunk_size, n, TCP_WRITE_FLAG_COPY);
    } else {
      if(safe_write(client,chunk_size, n) > 0) {
        if(client->is_websocket == 0) {
          client->lastseen = millis();
        }
      }
    }
    i += n;
  }

#if WEBSERVER_MAX_SENDLIST == 0
  tmp = client->sendlist;
#else
  x = 0, y = 0;
  for(x=0;x<WEBSERVER_MAX_SENDLIST;x++) {
    if(client->sendlist[x].data.ptr != NULL) {
      tmp = &client->sendlist[x];
      break;
    }
  }
#endif
  if(tmp != NULL) {
    while(tmp != NULL && client->totallen > 0) {
      if(client->ptr == 0) {
        if(client->totallen >= tmp->size) {
          if(tmp->type == 1) {
#if (!defined(NON32XFER_HANDLER) && defined(MMU_SEC_HEAP))
            uint16_t x = 0;
            for(x=0;x<tmp->size;x++) {
              cpy[x] = pgm_read_byte(&((PGM_P)tmp->data.ptr)[client->ptr+x]);
            }
#else
             memcpy_P(cpy, &((PGM_P)tmp->data.ptr)[client->ptr], tmp->size);
#endif
            if(client->async == 1) {
              tcp_write(client->pcb, cpy, tmp->size, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
            } else {
              if(safe_write(client,cpy, tmp->size) > 0) {
                if(client->is_websocket == 0) {
                  client->lastseen = millis();
                }
              }
            }
          } else {
            if(client->async == 1) {
              tcp_write(client->pcb, &((unsigned char *)tmp->data.ptr)[client->ptr], tmp->size, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
            } else {
              if(safe_write(client,&((unsigned char *)tmp->data.ptr)[client->ptr], tmp->size) > 0) {
                if(client->is_websocket == 0) {
                  client->lastseen = millis();
                }
              }
            }
          }
          i += tmp->size;
          client->ptr += tmp->size;
          client->totallen -= tmp->size;

          if(tmp->type == 0) {
            free(tmp->data.ptr);
          }

          tmp->data.ptr = NULL;
#if WEBSERVER_MAX_SENDLIST == 0
          client->sendlist = client->sendlist->next;
          free(tmp);
          tmp = client->sendlist;
#else
          tmp = NULL;
          for(y=x+1;y<WEBSERVER_MAX_SENDLIST;y++) {
            if(client->sendlist[y].data.ptr != NULL) {
              tmp = &client->sendlist[y];
              x = y;
              break;
            }
          }
#endif
          client->ptr = 0;
        } else {
          if(tmp->type == 1) {
#if (!defined(NON32XFER_HANDLER) && defined(MMU_SEC_HEAP))
            uint16_t x = 0;
            for(x=0;x<client->totallen;x++) {
              cpy[x] = pgm_read_byte(&((PGM_P)tmp->data.ptr)[client->ptr+x]);
            }
#else
             memcpy_P(cpy, &((PGM_P)tmp->data.ptr)[client->ptr], client->totallen);
#endif
            if(client->async == 1) {
              tcp_write(client->pcb, cpy, client->totallen, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
            } else {
              if(safe_write(client,cpy, client->totallen) > 0) {
                if(client->is_websocket == 0) {
                  client->lastseen = millis();
                }
              }
            }
          } else {
            if(client->async == 1) {
              tcp_write(client->pcb, &((unsigned char *)tmp->data.ptr)[client->ptr], client->totallen, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
            } else {
              if(safe_write(client,&((unsigned char *)tmp->data.ptr)[client->ptr], client->totallen) > 0) {
                if(client->is_websocket == 0) {
                  client->lastseen = millis();
                }
              }
            }
          }
          i += client->totallen;
          client->ptr += client->totallen;
          client->totallen = 0;
        }
      } else if(client->ptr+client->totallen >= tmp->size) {
        if(tmp->type == 1) {
#if (!defined(NON32XFER_HANDLER) && defined(MMU_SEC_HEAP))
          uint16_t x = 0;
          for(x=0;x<(tmp->size-client->ptr);x++) {
            cpy[x] = pgm_read_byte(&((PGM_P)tmp->data.ptr)[client->ptr+x]);
          }
#else
           memcpy_P(cpy, &((PGM_P)tmp->data.ptr)[client->ptr], (tmp->size-client->ptr));
#endif
          if(client->async == 1) {
            tcp_write(client->pcb, cpy, (tmp->size-client->ptr), TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
          } else {
            if(safe_write(client,cpy, (tmp->size-client->ptr)) > 0) {
              if(client->is_websocket == 0) {
                client->lastseen = millis();
              }
            }
          }
        } else {
          if(client->async == 1) {
            tcp_write(client->pcb, &((unsigned char *)tmp->data.ptr)[client->ptr], (tmp->size-client->ptr), TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
          } else {
            if(safe_write(client,&((unsigned char *)tmp->data.ptr)[client->ptr], (tmp->size-client->ptr)) > 0) {
              if(client->is_websocket == 0) {
                client->lastseen = millis();
              }
            }
          }
        }
        i += (tmp->size-client->ptr);
        client->totallen -= (tmp->size-client->ptr);

        if(tmp->type == 0) {
          free(tmp->data.ptr);
        }

        tmp->data.ptr = NULL;
#if WEBSERVER_MAX_SENDLIST == 0
        client->sendlist = client->sendlist->next;
        free(tmp);
        tmp = client->sendlist;
#else
        tmp = NULL;
        for(y=x+1;y<WEBSERVER_MAX_SENDLIST;y++) {
          if(client->sendlist[y].data.ptr != NULL) {
            tmp = &client->sendlist[y];
            x = y;
            break;
          }
        }
#endif
        client->ptr = 0;
      } else {
        if(tmp->type == 1) {
#if (!defined(NON32XFER_HANDLER) && defined(MMU_SEC_HEAP))
          uint16_t x = 0;
          for(x=0;x<client->totallen;x++) {
            cpy[x] = pgm_read_byte(&((PGM_P)tmp->data.ptr)[client->ptr+x]);
          }
#else
           memcpy_P(cpy, &((PGM_P)tmp->data.ptr)[client->ptr], client->totallen);
#endif
          if(client->async == 1) {
            tcp_write(client->pcb, cpy, client->totallen, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
          } else {
            if(safe_write(client,cpy, client->totallen) > 0) {
              if(client->is_websocket == 0) {
                client->lastseen = millis();
              }
            }
          }
        } else {
          if(client->async == 1) {
            tcp_write(client->pcb, &((unsigned char *)tmp->data.ptr)[client->ptr], client->totallen, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
          } else {
            if(safe_write(client,&((unsigned char *)tmp->data.ptr)[client->ptr], client->totallen) > 0) {
              if(client->is_websocket == 0) {
                client->lastseen = millis();
              }
            }
          }
        }
        client->ptr += client->totallen;
        client->totallen = 0;
      }
    }
    if(client->chunked == 1) {
      if(client->async == 1) {
        tcp_write_P(client->pcb, PSTR("\r\n"), 2, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
      } else {
        if(safe_write_P(client,(char *)PSTR("\r\n"), 2) > 0) {
          if(client->is_websocket == 0) {
            client->lastseen = millis();
          }
        }
      }
    }
  }

  if(tmp == NULL) {
#if WEBSERVER_MAX_SENDLIST > 0
    uint8_t x = 0;
    for(x=0;x<WEBSERVER_MAX_SENDLIST;x++) {
      tmp = &client->sendlist[x];
      tmp->data.ptr = NULL;
      memset(tmp, 0, sizeof(struct sendlist_t));
    }
#endif
    tmp = NULL;

    client->content++;
    if(client->is_websocket == 1) {
      client->step = WEBSERVER_CLIENT_WEBSOCKET;
    } else {
      client->step = WEBSERVER_CLIENT_WRITE;
      if(client->callback(client, NULL) == -1) {
        client->step = WEBSERVER_CLIENT_CLOSE;
      } else {
        client->step = WEBSERVER_CLIENT_SENDING;
      }

#if WEBSERVER_MAX_SENDLIST == 0
      tmp = client->sendlist;
#else
      for(x=0;x<WEBSERVER_MAX_SENDLIST;x++) {
        if(client->sendlist[x].data.ptr != NULL) {
          tmp = &client->sendlist[x];
          break;
        }
      }
#endif
      if(tmp == NULL) {
        if(client->chunked == 1) {
          if(client->async == 1) {
            tcp_write_P(client->pcb, PSTR("0\r\n\r\n"), 5, TCP_WRITE_FLAG_COPY);
          } else {
            if(safe_write_P(client,(char *)PSTR("0\r\n\r\n"), 5) > 0) {
              if(client->is_websocket == 0) {
                client->lastseen = millis();
              }
            }
          }
          i += 5;
        } else {
          if(client->async == 1) {
            tcp_write_P(client->pcb, PSTR("\r\n\r\n"), 4, TCP_WRITE_FLAG_COPY);
          } else {
            if(safe_write_P(client,(char *)PSTR("\r\n\r\n"), 4) > 0) {
              if(client->is_websocket == 0) {
                client->lastseen = millis();
              }
            }
          }
          i += 4;
        }
        client->step = WEBSERVER_CLIENT_CLOSE;
        client->userdata = NULL;
        client->ptr = 0;
        client->content = 0;
      }
    }
  }
  if(client->async == 1) {
    tcp_output(client->pcb);
  }

  return i;
}

void webserver_send_content_P(struct webserver_t *client, PGM_P buf, uint16_t size) {
  struct sendlist_t *node = NULL;

#if WEBSERVER_MAX_SENDLIST == 0
  node = (struct sendlist_t *)malloc(sizeof(struct sendlist_t));
  /*LCOV_EXCL_START*/
  if(node == NULL) {
  #if defined(ESP8266) || defined(ESP32)
    loggingSerial.printf("Out of memory %s:#%d\n", __FUNCTION__, __LINE__);
    ESP.restart();
    exit(-1);
  #endif
  }
#else
  uint8_t i = 0;
  for(i=0;i<WEBSERVER_MAX_SENDLIST;i++) {
    if(client->sendlist[i].data.ptr == NULL) {
      node = &client->sendlist[i];
      break;
    }
  }
  if(node == NULL) {
  #if defined(ESP8266) || defined(ESP32)
    loggingSerial.printf("Sendlist queue is full\n");
  #else
    printf("Sendlist queue is full\n");
  #endif
    return;
  }
#endif

  /*LCOV_EXCL_STOP*/
  memset(node, 0, sizeof(struct sendlist_t));
  node->data.ptr = (void *)buf;
  node->size = size;
  node->type = 1;

#if WEBSERVER_MAX_SENDLIST == 0
  if(client->sendlist == NULL) {
    client->sendlist = node;
    client->sendlist_head = node;
  } else {
    client->sendlist_head->next = node;
    client->sendlist_head = node;
  }
#endif
}

void webserver_send_content(struct webserver_t *client, char *buf, uint16_t size) {
  struct sendlist_t *node = NULL;

#if WEBSERVER_MAX_SENDLIST == 0
  node = (struct sendlist_t *)malloc(sizeof(struct sendlist_t));
  /*LCOV_EXCL_START*/
  if(node == NULL) {
  #if defined(ESP8266) || defined(ESP32)
    loggingSerial.printf("Out of memory %s:#%d\n", __FUNCTION__, __LINE__);
    ESP.restart();
    exit(-1);
  #endif
  }
#else
  uint8_t i = 0;
  for(i=0;i<WEBSERVER_MAX_SENDLIST;i++) {
    if(client->sendlist[i].data.ptr == NULL) {
      node = &client->sendlist[i];
      break;
    }
  }
  if(node == NULL) {
  #if defined(ESP8266) || defined(ESP32)
    loggingSerial.printf("Sendlist queue is full\n");
  #else
    printf("Sendlist queue is full\n");
  #endif
    return;
  }
#endif
  memset(node, 0, sizeof(struct sendlist_t));
  if((node->data.ptr = malloc(size+1)) == NULL) {
  #if defined(ESP8266) || defined(ESP32)
    loggingSerial.printf("Out of memory %s:#%d\n", __FUNCTION__, __LINE__);
    ESP.restart();
    exit(-1);
  #endif
  }
  memcpy(node->data.ptr, buf, size);

  node->size = size;
  node->type = 0;

#if WEBSERVER_MAX_SENDLIST == 0
  if(client->sendlist == NULL) {
    client->sendlist = node;
    client->sendlist_head = node;
  } else {
    client->sendlist_head->next = node;
    client->sendlist_head = node;
  }
#endif
}

int8_t webserver_send(struct webserver_t *client, uint16_t code, char *mimetype, uint16_t data_len) {
  uint16_t i = 0;
  if(data_len == 0) {
    unsigned char buffer[512], *p = buffer;
    memset(buffer, '\0', sizeof(buffer));

    client->chunked = 1;
    i = snprintf_P((char *)p, sizeof(buffer), PSTR("HTTP/1.1 %d %s\r\n"), code, code_to_text(code));
    if(client->callback != NULL) {
      client->step = WEBSERVER_CLIENT_CREATE_HEADER;
      struct header_t header;
      header.buffer = &p[i];
      header.ptr = i;

      if(client->callback(client, &header) == -1) {
        if(strstr_P((char *)&p[i], PSTR("\r\n\r\n")) == NULL) {
          if(strstr_P((char *)&p[i], PSTR("\r\n")) != NULL) {
            header.ptr += snprintf((char *)&p[header.ptr], sizeof(buffer)-header.ptr, PSTR("\r\n"));
          } else {
            header.ptr += snprintf((char *)&p[header.ptr], sizeof(buffer)-header.ptr, PSTR("\r\n\r\n"));
          }
        }
        client->step = WEBSERVER_CLIENT_WRITE;
        i = header.ptr;
        goto done;
      }
      if(header.ptr > i && strstr_P((char *)&p[i], PSTR("\r\n")) == NULL) {
        header.ptr += snprintf((char *)&p[header.ptr], sizeof(buffer)-header.ptr, PSTR("\r\n"));
      }
      i = header.ptr;
      client->step = WEBSERVER_CLIENT_WRITE;
    }

    i += snprintf((char *)&p[i], sizeof(buffer)-i, PSTR("Keep-Alive: timeout=15, max=100\r\n"));
    i += snprintf((char *)&p[i], sizeof(buffer)-i, PSTR("Content-Type: %s\r\n"), mimetype);
    i += snprintf((char *)&p[i], sizeof(buffer)-i, PSTR("Transfer-Encoding: chunked\r\n\r\n"));

done:
    if(client->async == 1) {
      tcp_write(client->pcb, &buffer, i, TCP_WRITE_FLAG_COPY);
      tcp_output(client->pcb);
    } else{
      if(safe_write(client,(unsigned char *)&buffer, i) > 0) {
        if(client->is_websocket == 0) {
          client->lastseen = millis();
        }
      }
    }
  } else {
    client->chunked = 0;
    i = webserver_create_header(client, code, mimetype, data_len);
  }

  if(i > 0) {
    return 0;
  } else {
    return -1;
  }
}

/* LCOV_EXCL_START*/
static void webserver_client_close(struct webserver_t *client) {
  if(client->callback != NULL) {
    client->callback(client, NULL);
  }
#if defined(ESP8266) || defined(ESP32)
  loggingSerial.print(F("Closing webserver client: "));
  if(client->async == 1) {
    loggingSerial.print(ipaddr_ntoa(&client->pcb->remote_ip));
    loggingSerial.print(F(":"));
    loggingSerial.println(client->pcb->remote_port);
  } else {
    loggingSerial.print(client->client->remoteIP().toString().c_str());
    loggingSerial.print(F(":"));
    loggingSerial.println(client->client->remotePort());
  }

  client->step = 0;

  tcp_recv(client->pcb, NULL);
  tcp_sent(client->pcb, NULL);
  tcp_poll(client->pcb, NULL, 0);

  tcp_close(client->pcb);
  client->pcb = NULL;

  webserver_reset_client(client);
#endif
}
/* LCOV_EXCL_STOP*/

#if defined(ESP8266) || defined(ESP32)
err_t webserver_sent(void *arg, tcp_pcb *pcb, uint16_t len) {
  uint16_t i = 0;
  for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
    if(clients[i].data.pcb == pcb) {
      if(clients[i].data.step == WEBSERVER_CLIENT_WRITE) {
        if(clients[i].data.callback(&clients[i].data, NULL) == -1) {
          clients[i].data.step = WEBSERVER_CLIENT_CLOSE;
        } else {
          clients[i].data.step = WEBSERVER_CLIENT_SENDING;
        }
      }
      if(clients[i].data.step == WEBSERVER_CLIENT_SENDING) {
        if((clients[i].data.totallen = tcp_sndbuf(clients[i].data.pcb)) > 0) {
          /*
           * Leave room for chunk overhead
           */
          clients[i].data.totallen -= 16;
          webserver_process_send(&clients[i].data);
        }
      }
      if(clients[i].data.step == WEBSERVER_CLIENT_CLOSE) {
        webserver_client_close(&clients[i].data);
      }
      break;
    }
  }
  return ERR_OK;
}
#endif


static void send_websocket_handshake(struct webserver_t *client, const char *key) {
  char cpy[61] = { 0 }; // key length = 24, magic = 36, + 1
  unsigned char input[SHA1_DIGEST_SIZE] = { 0 };
  char encoded[Base64encode_len(SHA1_DIGEST_SIZE)] = { 0 };

  const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  if(strlen(key) + strlen(magic) >= sizeof(cpy)) {
    return; // of error
  }

  snprintf(cpy, sizeof(cpy), "%s%s", key, magic);

  sha1digest(input, NULL, (uint8_t *)cpy, strlen(cpy));

  if(Base64encode(encoded, input, SHA1_DIGEST_SIZE) <= 0) {
    return;
  }

  unsigned char buf[256];
  int len = snprintf((char *)buf, sizeof(buf),
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Connection: Upgrade\r\n"
    "Upgrade: websocket\r\n"
    "Sec-WebSocket-Accept: %s\r\n\r\n", encoded);

  if(len < 0 || len >= (int)sizeof(buf)) {
    return;
  }

  if(client->async == 1) {
    tcp_write(client->pcb, buf, len, TCP_WRITE_FLAG_COPY);
    tcp_output(client->pcb);
  } else {
    if(safe_write(client,buf, len) > 0 && client->is_websocket == 0) {
      client->lastseen = millis();
    }
  }
}

void websocket_write_P(struct webserver_t *client, PGM_P data, uint16_t data_len) {
  websocket_send_header(client, WEBSOCKET_OPCODE_TEXT, data_len);
  webserver_send_content_P(client, data, data_len);
  client->step = WEBSERVER_CLIENT_SENDING;
}

void websocket_write(struct webserver_t *client, char *data, uint16_t data_len) {
  websocket_send_header(client, WEBSOCKET_OPCODE_TEXT, data_len);
  webserver_send_content(client, (char *)data, data_len);
  client->step = WEBSERVER_CLIENT_SENDING;
}

void websocket_write_all(char *data, uint16_t data_len) {
  uint8_t i = 0;
  for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
    if(clients[i].data.is_websocket == 1 && clients[i].data.step != WEBSERVER_CLIENT_CLOSE) {
      websocket_write(&clients[i].data, data, data_len);
    }
  }
}

void websocket_write_all_P(PGM_P data, uint16_t data_len) {
  uint8_t i = 0;
  for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
    if(clients[i].data.is_websocket == 1 && clients[i].data.step != WEBSERVER_CLIENT_CLOSE) {
      websocket_write_P(&clients[i].data, data, data_len);
    }
  }
}

void websocket_send_header(struct webserver_t *client, uint8_t opcode, uint16_t data_len) {
  unsigned char copy[10];
  int index = 2;
  memset(&copy, 0, 10);

  copy[0] = 0x80 + (opcode & 0x0f);
  if(data_len <= 125) {
    copy[1] = data_len;
  } else if(data_len < 65535) {
    copy[1] = 126;
    copy[2] = (data_len >> 8) & 255;
    copy[3] = (data_len) & 255;
    index = 4;
  } else {
    /**
     * Size too big for ESP8266
     */
    /*
      copy[1] = 127;
      copy[2] = (data_len >> 56) & 255;
      copy[3] = (data_len >> 48) & 255;
      copy[4] = (data_len >> 40) & 255;
      copy[5] = (data_len >> 32) & 255;
      copy[6] = (data_len >> 24) & 255;
      copy[7] = (data_len >> 16) & 255;
      copy[8] = (data_len >> 8) & 255;
      copy[9] = (data_len) & 255;
      index = 10;
     */
  }
  webserver_send_content(client, (char *)copy, index);
}

int websocket_read(struct webserver_t *client, unsigned char *buf, ssize_t buf_len) {
  unsigned int i = 0, j = 0;
  unsigned char mask[4];
  uint32_t packet_length = 0;
  int index_first_mask = 0;
  int index_first_data_byte = 0;
  int opcode = buf[0] & 0xF;

  memset(&mask, '\0', 4);
  packet_length = ((unsigned char)buf[1]) & 0x7F;

  index_first_mask = 2;
  if(packet_length == 126) {
    index_first_mask = 4;
    packet_length = buf[2] << 8 | buf[3];
  } else if(packet_length == 127) {
    if(buf[2] != 0 || buf[3] != 0 || buf[4] != 0 || buf[5] != 0) {
      /*
       * Packet length too big
       */
      return -1;
    } else {
      packet_length = buf[6] << 24 | buf[7] << 16 | buf[8] << 8 | buf[9];
    }
    index_first_mask = 10;
  }
  memcpy(mask, &buf[index_first_mask], 4);
  index_first_data_byte = index_first_mask + 4;

  for(i = index_first_data_byte, j = 0; i < buf_len && i < packet_length + index_first_data_byte; i++, j++) {
    buf[j] = buf[i] ^ mask[j % 4];
  }
  buf[packet_length] = '\0';

  switch(opcode) {
    case WEBSOCKET_OPCODE_PONG: {
      client->lastseen = millis();
    } break;
    case WEBSOCKET_OPCODE_PING: {
      websocket_send_header(client, WEBSOCKET_OPCODE_PONG, 0);
    } break;
    case WEBSOCKET_OPCODE_CONNECTION_CLOSE:
      websocket_send_header(client, WEBSOCKET_OPCODE_CONNECTION_CLOSE, 0);
      client->step = WEBSERVER_CLIENT_CLOSE;
      return -1;
    break;
    case WEBSOCKET_OPCODE_TEXT:
      if(client->callback != NULL) {
        client->step = WEBSERVER_CLIENT_WEBSOCKET_TEXT;
        if(client->callback(client, buf) == -1) {
          client->step = WEBSERVER_CLIENT_CLOSE;
          return -1;
        }
        client->step = WEBSERVER_CLIENT_WEBSOCKET;
      } else {
        client->step = WEBSERVER_CLIENT_CLOSE;
        return -1;
      }
    break;
  }

  return packet_length;
}

uint8_t webserver_sync_receive(struct webserver_t *client, uint8_t *rbuffer, uint16_t size) {
  if(client->step == WEBSERVER_CLIENT_READ_HEADER) {
    int8_t ret = http_parse_request(client, &rbuffer, &size);
    if(ret == -1) {
      client->step = WEBSERVER_CLIENT_CLOSE;
      return 0;
    }
    if(ret == 0) {
      if(client->is_websocket == 1 && client->data.websockkey != NULL) {
        client->is_websocket = 1;
        client->lastping = millis();
        send_websocket_handshake(client, (char *)client->data.websockkey);
        free(client->data.websockkey);
        client->data.websockkey = NULL;
        client->step = WEBSERVER_CLIENT_WEBSOCKET;
      }
      if(client->method == 1) {
         client->step = WEBSERVER_CLIENT_ARGS;
         if(client->reqtype == 0) {
          client->readlen = 0;
          if(http_parse_body(client, (char *)rbuffer, size) == -1) {
            client->step = WEBSERVER_CLIENT_CLOSE;
          }
        } else if(client->reqtype == 1) {
          client->substep = 0;
          if(http_parse_multipart_body(client, (unsigned char *)rbuffer, size) == -1) {
            client->step = WEBSERVER_CLIENT_CLOSE;
          }
        }
        if(client->readlen == client->totallen) {
          client->step = WEBSERVER_CLIENT_WRITE;
        }

        if(client->step == WEBSERVER_CLIENT_ARGS) {
          return 1;
        }
      } else if(client->step != WEBSERVER_CLIENT_WEBSOCKET) {
        client->step = WEBSERVER_CLIENT_WRITE;
      }
    }
  }

  if(client->step == WEBSERVER_CLIENT_WEBSOCKET && size > 0) {
    websocket_read(client, rbuffer, size);
  }

  if(client->step == WEBSERVER_CLIENT_ARGS) {
    if(client->reqtype == 0) {
      if(http_parse_body(client, (char *)rbuffer, size) == -1) {
        client->step = WEBSERVER_CLIENT_CLOSE;
      }
    } else if(client->reqtype == 1) {
      if(http_parse_multipart_body(client, (unsigned char *)rbuffer, size) == -1) {
        client->step = WEBSERVER_CLIENT_CLOSE;
      }
    }

    if(client->readlen == client->totallen) {
      client->step = WEBSERVER_CLIENT_WRITE;
    }
  }

  return 0;
}

err_t webserver_async_receive(void *arg, tcp_pcb *pcb, struct pbuf *data, err_t err) {
  uint16_t size = 0;
  uint8_t i = 0;

  if(data == NULL) {
    for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
      if(clients[i].data.pcb == pcb) {
        webserver_client_close(&clients[i].data);
      }
    }
    return ERR_OK;
  }

  struct pbuf *b = data;

  while(b != NULL) {
    rbuffer = (uint8_t *)b->payload;
    size = b->len;

    for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
      if(clients[i].data.pcb == pcb) {
        struct webserver_t *client = &clients[i].data;

        if(webserver_sync_receive(client, rbuffer, size) == 1) {
          continue;
        }

        if(client->step == WEBSERVER_CLIENT_WRITE) {
          if((client->totallen = tcp_sndbuf(client->pcb)) > 0) {
            client->totallen -= 16;
            if(client->callback != NULL) {
              if(client->callback(client, NULL) == -1) {
                client->step = WEBSERVER_CLIENT_CLOSE;
                return -1;
              }
              client->content++;
              client->ptr = 0;
            } else {
              client->step = WEBSERVER_CLIENT_CLOSE;
              return -1;
            }
          }
        }
        break;
      }
    }
    tcp_recved(pcb, b->len);
    b = b->next;
  }
  pbuf_free(data);

  return ERR_OK;
}

#if defined(ESP8266) || defined(ESP32)
err_t webserver_poll(void *arg, struct tcp_pcb *pcb) {
  uint8_t i = 0;
  for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
    if(clients[i].data.pcb == pcb) {
      if(clients[i].data.is_websocket == 1) {
        if((unsigned long)(millis() - clients[i].data.lastping) > WEBSERVER_CLIENT_PING_INTERVAL) {
          websocket_send_header(&clients[i].data, WEBSOCKET_OPCODE_PING, 0);
          clients[i].data.lastping = millis();
        }
      }
      if((unsigned long)(millis() - clients[i].data.lastseen) > WEBSERVER_CLIENT_TIMEOUT) {
  #if defined(ESP8266) || defined(ESP32)
		loggingSerial.printf("Timeout webserver client: %s:%d", ipaddr_ntoa(&clients[i].data.pcb->remote_ip), clients[i].data.pcb->remote_port);
  #endif
        clients[i].data.step = WEBSERVER_CLIENT_CLOSE;
        webserver_client_close(&clients[i].data);
      }
      break;
    }
  }
  return ERR_OK;
}
#endif

void webserver_reset_client(struct webserver_t *client) {
#if defined(ESP8266) || defined(ESP32)
  if(client->pcb != NULL) {
    tcp_close(client->pcb);
    client->pcb = NULL;
  }
  if(client->client != NULL) {
    client->client->stop();
    delete client->client;
    client->client = NULL;
  }
#endif

  client->readlen = 0;
  client->reqtype = 0;
  client->method = 0;
  client->async = 0;
  client->totallen = 0;
  client->step = 0;
  client->substep = 0;
  client->chunked = 0;
  client->ptr = 0;
  client->route = 0;
  client->lastseen = 0;
  client->lastping = 0;
  client->content = 0;
  client->is_websocket = 0;
  client->userdata = NULL;

  struct sendlist_t *tmp = NULL;
#if WEBSERVER_MAX_SENDLIST == 0
  while(client->sendlist) {
    tmp = client->sendlist;
    client->sendlist = client->sendlist->next;
    if(tmp->type == 0) {
      free(tmp->data.ptr);
    }
    tmp->data.ptr = NULL;
    free(tmp);
  }
#else
  uint8_t i = 0;
  for(i=0;i<WEBSERVER_MAX_SENDLIST;i++) {
    tmp = &client->sendlist[i];
    if(tmp->type == 0) {
      free(tmp->data.ptr);
    }
    tmp->data.ptr = NULL;
    memset(tmp, 0, sizeof(struct sendlist_t));
  }
#endif
  if(client->data.boundary != NULL) {
    free(client->data.boundary);
    client->data.boundary = NULL;
  }
  if(client->data.websockkey != NULL) {
    free(client->data.websockkey);
    client->data.websockkey = NULL;
  }

#if WEBSERVER_MAX_SENDLIST == 0
  client->sendlist = NULL;
  client->sendlist_head = NULL;
#endif
  client->data.boundary = NULL;
  memset(&client->buffer, 0, WEBSERVER_BUFFER_SIZE);
}

#if defined(ESP8266) || defined(ESP32)
err_t webserver_client(void *arg, tcp_pcb *pcb, err_t err) {
  uint8_t i = 0;
  for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
    if(clients[i].data.pcb == NULL) {
      webserver_reset_client(&clients[i].data);
      clients[i].data.pcb = pcb;
      clients[i].data.async = 1;
      clients[i].data.step = WEBSERVER_CLIENT_READ_HEADER;

      loggingSerial.print(F("New webserver client: "));
	  loggingSerial.print(ipaddr_ntoa(&clients[i].data.pcb->remote_ip));
	  loggingSerial.print(F(":"));
	  loggingSerial.println(clients[i].data.pcb->remote_port);

      //tcp_nagle_disable(pcb);
      tcp_recv(pcb, &webserver_async_receive);
      tcp_sent(pcb, &webserver_sent);
      tcp_poll(pcb, &webserver_poll, 1);
      break;
    }
  }
  return ERR_OK;
}
#endif

void webserver_loop(void) {
  uint16_t size = 0;
  uint8_t i = 0;

  for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
    /*
     * size (and the shared rbuffer it indexes into) must be reset for every
     * client, not just once per call: if a client at an earlier index reads
     * real data this pass and sets size > 0, and a later client in this same
     * pass is connected() but not yet available() (the common case for a
     * mostly-idle websocket), the read below is skipped and size/rbuffer
     * would otherwise still hold the EARLIER client's leftover bytes - which
     * then get fed into webserver_sync_receive() below as if they were
     * freshly received data for this client. In the reverse ordering, this
     * silently injects a few stray bytes from another connection (e.g. a
     * websocket ping/pong) into an in-progress firmware upload's multipart
     * stream, corrupting the byte count against the declared Content-Length.
     */
    size = 0;
    if(clients[i].data.step == 0 || clients[i].data.async == 1) {
      continue;
    }

    if(clients[i].data.is_websocket == 1) {
      if((unsigned long)(millis() - clients[i].data.lastping) > WEBSERVER_CLIENT_PING_INTERVAL) {
        websocket_send_header(&clients[i].data, WEBSOCKET_OPCODE_PING, 0);
        clients[i].data.lastping = millis();
      }
    }
    if((unsigned long)(millis() - clients[i].data.lastseen) > WEBSERVER_CLIENT_TIMEOUT) {
#if defined(ESP8266) || defined(ESP32)
        loggingSerial.print("Timeout webserver client: ");
        loggingSerial.print(clients[i].data.client->remoteIP()); //ok to use, no async call
        loggingSerial.print(":");
        loggingSerial.println(clients[i].data.client->remotePort());
#endif
      clients[i].data.step = WEBSERVER_CLIENT_CLOSE;
    }
    if(!clients[i].data.client->connected()) {
      clients[i].data.step = WEBSERVER_CLIENT_CLOSE;
    }
    switch(clients[i].data.step) {
      case WEBSERVER_CLIENT_CONNECTING: {
        if(clients[i].data.client->available()) {
          clients[i].data.step = WEBSERVER_CLIENT_READ_HEADER;
        }
        clients[i].data.ptr = 0;
        memset(&clients[i].data.buffer, 0, WEBSERVER_BUFFER_SIZE);
      } break;
      case WEBSERVER_CLIENT_ARGS:
      case WEBSERVER_CLIENT_WEBSOCKET:
      case WEBSERVER_CLIENT_READ_HEADER: {
        if(clients[i].data.client->connected() || clients[i].data.client->available()) {
          if(clients[i].data.client->available()) {
            uint8_t *p = (uint8_t *)rbuffer;
            size = clients[i].data.client->read(
              p,
              WEBSERVER_READ_SIZE
            );
          }
        } else if(!clients[i].data.client->connected()) {
          clients[i].data.step = WEBSERVER_CLIENT_CLOSE;
        } else {
          continue;
        }
        if(size > 0) {
          clients[i].data.lastseen = millis();
          webserver_sync_receive(&clients[i].data, rbuffer, size);
        }
      } break;
      case WEBSERVER_CLIENT_WRITE: {
        if(clients[i].data.callback != NULL) {
          if(clients[i].data.step == WEBSERVER_CLIENT_WRITE) {
            if(clients[i].data.callback(&clients[i].data, NULL) == -1) {
              clients[i].data.step = WEBSERVER_CLIENT_CLOSE;
            } else if(clients[i].data.content > 0) {
              clients[i].data.step = WEBSERVER_CLIENT_SENDING;
            } else {
              clients[i].data.step = WEBSERVER_CLIENT_WRITE;
              clients[i].data.content++;
            }
          }
          clients[i].data.ptr = 0;
        } else {
          clients[i].data.step = WEBSERVER_CLIENT_CLOSE;
          continue;
        }
      } break;
      case WEBSERVER_CLIENT_SENDING: {
        clients[i].data.totallen = MTU_SIZE;
        /*
         * Leave room for chunk overhead
         */
        clients[i].data.totallen -= 16;
        webserver_process_send(&clients[i].data);
      } break;
      case WEBSERVER_CLIENT_CLOSE: {
#if defined(ESP8266) || defined(ESP32)
        loggingSerial.print("Closing webserver client: ");
        loggingSerial.print(clients[i].data.client->remoteIP()); //ok to use, no async call
        loggingSerial.print(":");
        loggingSerial.println(clients[i].data.client->remotePort());
#endif
        if(clients[i].data.callback != NULL) {
          clients[i].data.callback(&clients[i].data, NULL);
        }

        clients[i].data.client->stop();
        webserver_reset_client(&clients[i].data);
      } break;
    }
  }

#if defined(ESP8266) || defined(ESP32)
  if(sync_server.hasClient()) {
    for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
      if(clients[i].data.client == NULL) {
        webserver_reset_client(&clients[i].data);
        clients[i].data.client = new WiFiClient(sync_server.accept());
        if(clients[i].data.client) {

          clients[i].data.async = 0;
          clients[i].data.lastseen = millis();
          clients[i].data.step = WEBSERVER_CLIENT_CONNECTING;

          clients[i].data.client->setNoDelay(true);
          clients[i].data.client->setTimeout(5000);

          loggingSerial.print("New webserver client: ");
          loggingSerial.print(clients[i].data.client->remoteIP());
          loggingSerial.print(":");
          loggingSerial.println(clients[i].data.client->remotePort());
          break;
        }
      }
    }
  }
#endif
}

int8_t webserver_start(int port, webserver_cb_t *callback, uint8_t async) {
  uint8_t i = 0;

  if(async == 1) {
    for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
      webserver_reset_client(&clients[i].data);
      clients[i].data.callback = callback;
      clients[i].data.async = 1;
    }

#if defined(ESP8266) || defined(ESP32)
    async_server = tcp_new();
    if(async_server == NULL) {
      return -1;
    }

    tcp_setprio(async_server, TCP_PRIO_MIN);

    ip_addr_t local_addr;
#ifdef ESP8266
    local_addr.addr = (uint32_t)IPADDR_ANY;
#endif
#ifdef ESP32
    local_addr.u_addr.ip4.addr= (uint32_t)IPADDR_ANY;
#endif
    uint8_t err = tcp_bind(async_server, &local_addr, port);
    if(err != ERR_OK) {
      tcp_close(async_server);
      return -1;
    }

    tcp_pcb *listen_pcb = tcp_listen_with_backlog(async_server, WEBSERVER_MAX_CLIENTS);
    if(listen_pcb == NULL) {
      tcp_close(async_server);
      async_server = NULL;
      return -1;
    }
    async_server = listen_pcb;
    tcp_nagle_disable(async_server);
    tcp_setprio(async_server, TCP_PRIO_MIN);
    tcp_accept(async_server, &webserver_client);
    tcp_arg(async_server, (void *)callback);
#endif
  } else {
    rbuffer = (uint8_t *)malloc(WEBSERVER_READ_SIZE);
    if(rbuffer == NULL) {
#if defined(ESP8266) || defined(ESP32)
      loggingSerial.printf("Out of memory %s:#%d\n", __FUNCTION__, __LINE__);
      ESP.restart();
      exit(-1);
#endif
    }

    for(i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
      webserver_reset_client(&clients[i].data);
      clients[i].data.callback = callback;
      clients[i].data.async = 0;
    }
#if defined(ESP8266) || defined(ESP32)
    sync_server.begin(port);
#endif
  }

#if defined(ESP8266) || defined(ESP32)
  if(async == 1) {
    loggingSerial.print("A-sync ");
  } else {
    loggingSerial.print("Sync ");
  }
  loggingSerial.print("webserver server started at port: ");
  loggingSerial.println(port);
#endif
  return 0;
}
