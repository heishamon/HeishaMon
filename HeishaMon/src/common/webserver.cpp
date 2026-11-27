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
#if defined(ESP32) && !defined(strncasecmp_P)
  #define strncasecmp_P strncasecmp
#endif
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
static uint8_t webserver_busy = 0; // set while webserver_loop() runs
static uint8_t *rbuffer = NULL;

/*
  ESP32 synchronous HTTP TX staging.

  Socket sends may accept only part of a response. Stage HTTP fragments in
  wire order and resume delivery across webserver_loop() iterations, without
  blocking the main loop on a slow receiver.
*/
#ifdef ESP32
#include "lwip/sockets.h"
#include <errno.h>
// One fixed staging buffer for each possible client (no per-request malloc/free).
static const uint16_t WEB_TX_CAPACITY = MTU_SIZE + 64;
static const uint16_t WEB_TX_FLUSH_LIMIT = 1500;
static const uint32_t WEB_TX_STALL_TIMEOUT_MS = 5000;
static uint8_t web_tx_pool[WEBSERVER_MAX_CLIENTS][WEB_TX_CAPACITY];
static uint8_t web_tx_busy[WEBSERVER_MAX_CLIENTS] = {0};

typedef enum {
  WEB_TX_DONE = 0,
  WEB_TX_PENDING = 1,
  WEB_TX_FAILED = -1
} web_tx_status_t;

static web_tx_status_t web_tx_fail(struct webserver_t *client) {
  client->tx_failed = 1;
  client->step = WEBSERVER_CLIENT_CLOSE;
  client->tx_used = 0;
  return WEB_TX_FAILED;
}

static int8_t web_tx_acquire(struct webserver_t *client) {
  if(client->tx_data != NULL) {
    return 0;
  }
  for(uint8_t i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
    if(web_tx_busy[i] == 0) {
      web_tx_busy[i] = 1;
      client->tx_data = web_tx_pool[i];
      client->tx_used = 0;
      client->tx_last_progress = millis();
      return 0;
    }
  }
  return -1;
}

static void web_tx_release(struct webserver_t *client) {
  if(client->tx_data == NULL) {
    return;
  }
  for(uint8_t i=0;i<WEBSERVER_MAX_CLIENTS;i++) {
    if(client->tx_data == &web_tx_pool[i][0]) {
      web_tx_busy[i] = 0;
      break;
    }
  }
  client->tx_data = NULL;
  client->tx_used = 0;
}

static int32_t web_tx_enqueue(struct webserver_t *client, const uint8_t *buf, uint16_t len) {
  if(len == 0) {
    return 0;
  }
  if(web_tx_acquire(client) == -1) {
    Serial.println("WEB TX buffer unavailable; closing client");
    web_tx_fail(client);
    return -1;
  }
  if(client->tx_used > WEB_TX_CAPACITY || len > WEB_TX_CAPACITY - client->tx_used) {
    Serial.printf("WEB TX queue overflow queued=%u new=%u; closing client\n",
      (unsigned int)client->tx_used, (unsigned int)len);
    web_tx_fail(client);
    return -1;
  }
  if(client->tx_used == 0) {
    client->tx_last_progress = millis();
  }
  memcpy(client->tx_data + client->tx_used, buf, len);
  client->tx_used += len;
  return (int32_t)len;
}

/*
 * Flush pending TX data.
 * WEB_TX_DONE:    all data has been sent.
 * WEB_TX_PENDING: more data remains; retry on next loop iteration.
 * WEB_TX_FAILED:  transmission failed; client will be closed.
 */
static web_tx_status_t web_tx_flush(struct webserver_t *client) {
  if(client->tx_used == 0) {
    return WEB_TX_DONE;
  }

  if(client->tx_data == NULL || client->tx_used > WEB_TX_CAPACITY) {
    Serial.println("WEB TX invalid buffer state; closing client");
    return web_tx_fail(client);
  }

  if(client->client == NULL) {
    Serial.println("WEB TX client unavailable; closing client");
    return web_tx_fail(client);
  }

  int fd = client->client->fd();
  if(fd < 0) {
    Serial.println("WEB TX invalid socket; closing client");
    return web_tx_fail(client);
  }

  uint16_t attempt = client->tx_used < WEB_TX_FLUSH_LIMIT ? client->tx_used : WEB_TX_FLUSH_LIMIT;
  int n = lwip_send(fd, client->tx_data, attempt, MSG_DONTWAIT);

  if(n > 0) {
    if((uint16_t)n > attempt) {
      Serial.printf("WEB TX invalid send length=%d; closing client\n", n);
      return web_tx_fail(client);
    }

    client->tx_used -= (uint16_t)n;

    if(client->tx_used > 0) {
      memmove(client->tx_data, client->tx_data + n, client->tx_used);
    }

    client->tx_last_progress = millis();

    if(client->is_websocket == 0) {
      client->lastseen = millis();
    }
  } else if(n < 0) {
    // Save errno before doing anything that might overwrite it.
    int err = errno;

    if(err != EAGAIN && err != EWOULDBLOCK && err != EINTR && err != ENOMEM) {
      Serial.printf("WEB TX socket error errno=%d; closing client\n", err);
      return web_tx_fail(client);
    }
  } else {
    // No progress is possible with pending data.
    Serial.println("WEB TX socket returned zero; closing client");
    return web_tx_fail(client);
  }

  if(client->tx_used == 0) {
    return WEB_TX_DONE;
  }

  if((uint32_t)(millis() - client->tx_last_progress) > WEB_TX_STALL_TIMEOUT_MS) {
    Serial.printf("WEB TX stalled for %lu ms remaining=%u; closing client\n",
      (unsigned long)(millis() - client->tx_last_progress),
      (unsigned int)client->tx_used);
    return web_tx_fail(client);
  }

  return WEB_TX_PENDING;
}
#endif

static int16_t safe_write(struct webserver_t *client, const uint8_t *buf, uint16_t len) {
#if defined(ESP8266)
  uint32_t start = millis();
  while(client->client->availableForWrite() == 0) {
    if((unsigned long)(millis() - start) > 1000) {
      client->step = WEBSERVER_CLIENT_CLOSE;
      return -1;
    }
    yield();
  }
#endif
#if defined(ESP32)
  return web_tx_enqueue(client, buf, len);
#else
  const int16_t written = client->client->write(buf, len);
  if (written != len) client->step = WEBSERVER_CLIENT_CLOSE;
  return written;
#endif
}

static int16_t safe_write_P(struct webserver_t *client, PGM_P buf, uint16_t len) {
#if defined(ESP8266)
  uint32_t start = millis();
  while(client->client->availableForWrite() == 0) {
    if((unsigned long)(millis() - start) > 1000) {
      client->step = WEBSERVER_CLIENT_CLOSE;
      return -1;
    }
    yield();
  }
#endif
#if defined(ESP32)
  return web_tx_enqueue(client, (const uint8_t *)buf, len);
#else
  const int16_t written = client->client->write_P((char *)buf, len);
  if (written != len) client->step = WEBSERVER_CLIENT_CLOSE;
  return written;
#endif
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

    /*
     * An argument never continues past the end of its line. Without
     * this limit, the arguments of a request line were searched in
     * everything that was already buffered, so a '&' in a header or
     * in the body of a small POST was taken as the end of a GET
     * argument and the headers in between were thrown away.
     */
    uint16_t end = size;
    if(ptrD != NULL && posD > pos) {
      end = posD;
    }

    unsigned char *ptr1 = (unsigned char *)memchr(&client->buffer[pos+1], '&', end-(pos+1));
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
          args.len = pos1 - (pos + 1); // the value ends at pos1, not at the end of the buffer
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

/*
 * Request line and headers.
 *
 * They are read byte by byte, so a request may be split anywhere over
 * the TCP reads. What the callback needs to decide about the request
 * (the path, the query and a few headers) is kept in a small buffer on
 * the heap (struct webserver_reqbuf_t), each value with its own limit.
 * Nothing of the request is given to the callback before all headers
 * are in; then it gets one WEBSERVER_CLIENT_REQUEST event and only after
 * it accepted that are the query arguments, the body or a websocket
 * upgrade handled.
 */

/*
 * Values kept in the request buffer: the public headers first (same
 * index as WEBSERVER_HEADER_x), then the ones only the webserver uses.
 */
enum {
  REQ_FIELD_CONTENT_LENGTH = WEBSERVER_HEADER_COUNT,
  REQ_FIELD_UPGRADE,
  REQ_FIELD_WS_KEY,
  REQ_FIELD_WS_VERSION,
  REQ_FIELD_PATH,
  REQ_FIELD_QUERY,
  REQ_FIELD_COUNT
};

static const uint16_t req_field_max[REQ_FIELD_COUNT] = {
  WEBSERVER_MAX_HOST,
  WEBSERVER_MAX_ORIGIN,
  WEBSERVER_MAX_COOKIE,
  WEBSERVER_MAX_AUTHORIZATION,
  WEBSERVER_MAX_CONTENT_TYPE,
  WEBSERVER_MAX_SEC_FETCH_SITE,
  12,  // Content-Length
  32,  // Upgrade
  32,  // Sec-WebSocket-Key (24 characters)
  8,   // Sec-WebSocket-Version
  WEBSERVER_MAX_PATH,
  WEBSERVER_MAX_QUERY
};

struct webserver_reqbuf_t {
  uint16_t cap;                    // bytes allocated for the values (after this struct)
  uint16_t len;                    // bytes in use
  uint16_t hbytes;                 // bytes of the request line and headers so far
  uint16_t truncated;              // bit per field: value was cut
  uint16_t off[REQ_FIELD_COUNT];   // offset + 1 of the value, 0: not sent
  int8_t cur;                      // field the current value goes to, -1: none
};

#define REQ_BUF_STEP 128

static char *reqbuf_data(struct webserver_reqbuf_t *rb) {
  return (char *)(rb + 1);
}

static void reqbuf_free(struct webserver_t *client) {
  if(client->req != NULL) {
    free(client->req);
    client->req = NULL;
  }
}

static int8_t reqbuf_init(struct webserver_t *client) {
  reqbuf_free(client);
  client->req = (struct webserver_reqbuf_t *)malloc(sizeof(struct webserver_reqbuf_t) + REQ_BUF_STEP);
  if(client->req == NULL) {
    return -1;
  }
  memset(client->req, 0, sizeof(struct webserver_reqbuf_t));
  client->req->cap = REQ_BUF_STEP;
  client->req->cur = -1;
  return 0;
}

static int8_t reqbuf_put(struct webserver_t *client, char c) {
  struct webserver_reqbuf_t *rb = client->req;
  if(rb->len >= rb->cap) {
    struct webserver_reqbuf_t *n = (struct webserver_reqbuf_t *)realloc(rb, sizeof(struct webserver_reqbuf_t) + rb->cap + REQ_BUF_STEP);
    if(n == NULL) {
      return -1;
    }
    n->cap += REQ_BUF_STEP;
    client->req = rb = n;
  }
  reqbuf_data(rb)[rb->len++] = c;
  return 0;
}

static int8_t reqbuf_start(struct webserver_t *client, uint8_t field) {
  client->req->off[field] = client->req->len + 1;
  client->req->cur = field;
  return 0;
}

/*
 * Add a byte to the current value: 0 stored, 1 the value is full (the
 * byte is not stored), -1 out of memory.
 */
static int8_t reqbuf_add(struct webserver_t *client, char c) {
  struct webserver_reqbuf_t *rb = client->req;
  if(rb->cur < 0) {
    return 0;
  }
  if((uint16_t)(rb->len - (rb->off[(uint8_t)rb->cur] - 1)) >= req_field_max[(uint8_t)rb->cur]) {
    rb->truncated |= (1 << rb->cur);
    return 1;
  }
  return reqbuf_put(client, c);
}

static int8_t reqbuf_end(struct webserver_t *client) {
  struct webserver_reqbuf_t *rb = client->req;
  if(rb->cur < 0) {
    return 0;
  }
  uint16_t start = rb->off[(uint8_t)rb->cur] - 1;
  char *d = reqbuf_data(rb);
  while(rb->len > start && (d[rb->len-1] == ' ' || d[rb->len-1] == '\t')) {
    rb->len--;
  }
  rb->cur = -1;
  return reqbuf_put(client, '\0');
}

static char *reqbuf_get(struct webserver_reqbuf_t *rb, uint8_t field) {
  if(rb == NULL || rb->off[field] == 0) {
    return NULL;
  }
  return &reqbuf_data(rb)[rb->off[field] - 1];
}

static int8_t req_header_field(const char *name) {
  if(strcasecmp_P(name, PSTR("Host")) == 0) {
    return WEBSERVER_HEADER_HOST;
  } else if(strcasecmp_P(name, PSTR("Origin")) == 0) {
    return WEBSERVER_HEADER_ORIGIN;
  } else if(strcasecmp_P(name, PSTR("Cookie")) == 0) {
    return WEBSERVER_HEADER_COOKIE;
  } else if(strcasecmp_P(name, PSTR("Authorization")) == 0) {
    return WEBSERVER_HEADER_AUTHORIZATION;
  } else if(strcasecmp_P(name, PSTR("Content-Type")) == 0) {
    return WEBSERVER_HEADER_CONTENT_TYPE;
  } else if(strcasecmp_P(name, PSTR("Sec-Fetch-Site")) == 0) {
    return WEBSERVER_HEADER_SEC_FETCH_SITE;
  } else if(strcasecmp_P(name, PSTR("Content-Length")) == 0) {
    return REQ_FIELD_CONTENT_LENGTH;
  } else if(strcasecmp_P(name, PSTR("Upgrade")) == 0) {
    return REQ_FIELD_UPGRADE;
  } else if(strcasecmp_P(name, PSTR("Sec-WebSocket-Key")) == 0) {
    return REQ_FIELD_WS_KEY;
  } else if(strcasecmp_P(name, PSTR("Sec-WebSocket-Version")) == 0) {
    return REQ_FIELD_WS_VERSION;
  }
  return -1;
}

/*
 * States of the request parser (client->substep while the step is
 * WEBSERVER_CLIENT_READ_HEADER).
 */
enum {
  REQ_METHOD = 0,
  REQ_PATH,
  REQ_QUERY,
  REQ_VERSION,
  REQ_HEADER_NAME,
  REQ_HEADER_SPACE,
  REQ_HEADER_VALUE,
  REQ_HEADER_END,  // '\r' of the empty line seen
  REQ_DONE
};

#define REQ_NAME_MAX 32 // longer header names are none of the ones that are kept

/*
 * Returns the number of bytes used from buf (the rest is the start of the
 * body or of the websocket data) or -1 for a request that is not valid or
 * too large. client->substep is REQ_DONE when all headers are in.
 */
static int32_t http_parse_request(struct webserver_t *client, const uint8_t *buf, uint16_t len) {
  uint16_t i = 0;

  if(client->req == NULL) {
    if(reqbuf_init(client) == -1) {
      return -1;
    }
    client->substep = REQ_METHOD;
    client->ptr = 0;
  }

  for(i=0;i<len && client->substep != REQ_DONE;i++) {
    char c = (char)buf[i];

    if(++client->req->hbytes > WEBSERVER_MAX_HEADER_SIZE) {
      return -1;
    }

    switch(client->substep) {
      case REQ_METHOD: {
        if(c == ' ') {
          client->buffer[client->ptr] = 0;
          if(strcmp_P((char *)client->buffer, PSTR("GET")) == 0) {
            client->method = WEBSERVER_METHOD_GET;
          } else if(strcmp_P((char *)client->buffer, PSTR("POST")) == 0) {
            client->method = WEBSERVER_METHOD_POST;
          } else {
            return -1;
          }
          client->ptr = 0;
          reqbuf_start(client, REQ_FIELD_PATH);
          client->substep = REQ_PATH;
        } else if(c < 'A' || c > 'Z' || client->ptr >= 7) {
          /*
           * Unsupported request method (HEAD, OPTIONS, binary
           * probes from network scanners, etc.)
           */
          return -1;
        } else {
          client->buffer[client->ptr++] = c;
        }
      } break;
      case REQ_PATH:
      case REQ_QUERY: {
        if(c == ' ' || (c == '?' && client->substep == REQ_PATH)) {
          if(client->req->len == client->req->off[REQ_FIELD_PATH] - 1) {
            return -1; // empty path
          }
          if(reqbuf_end(client) == -1) {
            return -1;
          }
          if(c == '?') {
            reqbuf_start(client, REQ_FIELD_QUERY);
            client->substep = REQ_QUERY;
          } else {
            client->substep = REQ_VERSION;
          }
        } else if(c == '\r' || c == '\n' || c == '\0') {
          return -1;
        } else if(reqbuf_add(client, c) != 0) {
          return -1; // URI too long (or out of memory): never cut a path or a query
        }
      } break;
      case REQ_VERSION: {
        if(c == '\n') {
          client->buffer[client->ptr] = 0;
          if(client->ptr > 0 && client->buffer[client->ptr-1] == '\r') {
            client->buffer[--client->ptr] = 0;
          }
          if(strncmp_P((char *)client->buffer, PSTR("HTTP/1."), 7) != 0) {
            return -1;
          }
          client->ptr = 0;
          client->substep = REQ_HEADER_NAME;
        } else if(client->ptr >= 15) {
          return -1;
        } else {
          client->buffer[client->ptr++] = c;
        }
      } break;
      case REQ_HEADER_NAME: {
        if(c == ':') {
          int8_t field = -1;
          if(client->ptr > 0 && client->ptr < REQ_NAME_MAX) {
            client->buffer[client->ptr] = 0;
            field = req_header_field((char *)client->buffer);
          }
          if(field >= 0 && client->req->off[field] != 0) {
            if(field == REQ_FIELD_CONTENT_LENGTH) {
              return -1; // two lengths: which one is right?
            }
            field = -1; // only the first of a header is used
          }
          if(field >= 0) {
            reqbuf_start(client, field);
          }
          client->ptr = 0;
          client->substep = REQ_HEADER_SPACE;
        } else if(client->ptr == 0 && c == '\r') {
          client->substep = REQ_HEADER_END;
        } else if(client->ptr == 0 && c == '\n') {
          client->substep = REQ_DONE;
        } else if(c == '\r' || c == '\n') {
          return -1; // header line without a ':'
        } else {
          if(client->ptr < REQ_NAME_MAX) {
            client->buffer[client->ptr] = c;
          }
          if(client->ptr < 0xFFFF) {
            client->ptr++;
          }
        }
      } break;
      case REQ_HEADER_SPACE:
      case REQ_HEADER_VALUE: {
        if(c == '\n') {
          if(reqbuf_end(client) == -1) {
            return -1;
          }
          client->substep = REQ_HEADER_NAME;
        } else if(c == '\r') {
          // the '\n' follows
        } else if(client->substep == REQ_HEADER_SPACE && (c == ' ' || c == '\t')) {
          // leading white space
        } else {
          client->substep = REQ_HEADER_VALUE;
          if(reqbuf_add(client, c) == -1) {
            return -1;
          }
        }
      } break;
      case REQ_HEADER_END: {
        if(c != '\n') {
          return -1;
        }
        client->substep = REQ_DONE;
      } break;
    }
  }

  return i;
}

/*
 * The query arguments, given to the callback after it accepted the
 * request. Like before: '+' is a space, %xx is decoded, the name and the
 * value are terminated with a 0, a name without '=' has value NULL.
 */
static int8_t http_parse_query(struct webserver_t *client, char *query) {
  char *p = query;
  while(p != NULL && *p != '\0') {
    char *end = strchr(p, '&');
    char *next = NULL;
    if(end != NULL) {
      next = end + 1;
    } else {
      end = p + strlen(p);
    }
    if(end > p) {
      struct arguments_t args;
      char *eq = (char *)memchr(p, '=', end - p);
      uint16_t nlen = (eq != NULL) ? (eq - p) : (end - p);
      int16_t n = urldecode((unsigned char *)p, nlen, (unsigned char *)p, nlen + 1, 1);
      args.name = (unsigned char *)p;
      args.value = NULL;
      args.len = 0;
      if(n < 0) {
        return -1;
      }
      if(eq != NULL) {
        uint16_t vlen = end - (eq + 1);
        int16_t v = urldecode((unsigned char *)(eq + 1), vlen, (unsigned char *)(eq + 1), vlen + 1, 1);
        if(v < 0) {
          return -1;
        }
        args.value = (unsigned char *)(eq + 1);
        args.len = v;
      }
      if(client->callback != NULL) {
        if(client->callback(client, &args) == -1) {
          return -1;
        }
      }
    }
    p = next;
  }
  return 0;
}

/*
 * "multipart/form-data; boundary=xyz" -> client->data.boundary =
 * "\r\n--xyz". -1 when there is no valid boundary.
 */
static int8_t http_multipart_boundary(struct webserver_t *client, const char *ctype) {
  const char *p = ctype;
  const char *b = NULL;
  while((p = strchr(p, '=')) != NULL) {
    if(p - ctype >= 8 && strncasecmp_P(p - 8, PSTR("boundary"), 8) == 0) {
      b = p + 1;
      break;
    }
    p++;
  }
  if(b == NULL) {
    return -1;
  }
  uint8_t quoted = (*b == '"');
  if(quoted) {
    b++;
  }
  uint16_t blen = 0;
  while(b[blen] != '\0' && ((quoted && b[blen] != '"') || (!quoted && b[blen] != ';' && b[blen] != ' ' && b[blen] != '\t'))) {
    if(b[blen] == '\r' || b[blen] == '\n') {
      return -1;
    }
    blen++;
  }
  if(blen == 0 || blen > 70 || (quoted && b[blen] != '"')) {
    return -1;
  }
  if(client->data.boundary != NULL) {
    free(client->data.boundary);
  }
  if((client->data.boundary = (char *)malloc(blen + 5)) == NULL) {
    return -1;
  }
  memcpy(client->data.boundary, "\r\n--", 4);
  memcpy(&client->data.boundary[4], b, blen);
  client->data.boundary[blen + 4] = 0;
  client->mpdlen = blen + 4;
  return 0;
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

/*
 * multipart/form-data body.
 *
 * Read as a stream, so a body may be split anywhere over the TCP reads,
 * also inside a boundary. The delimiter is "\r\n--<boundary>" (the first
 * one, at the start of the body, has no "\r\n" before it). Because '\r'
 * is only the first byte of the delimiter, a partial match that fails can
 * simply be given back as data.
 *
 * client->buffer holds the name of the current field (0 terminated,
 * client->mpname long) followed by the part headers or the value being
 * collected. A value is given to the callback (WEBSERVER_CLIENT_ARGS)
 * when the buffer is full and when the field ends, so a value that fits
 * in the buffer always arrives in one piece. A callback that returns -1
 * closes the connection.
 */
enum {
  MP_PREAMBLE = 0,  // before the first delimiter
  MP_AFTER_DELIM,   // a delimiter: "\r\n" (a part follows) or "--" (the end)
  MP_AFTER_DASH,    // "-" after a delimiter
  MP_AFTER_CR,      // "\r" after a delimiter
  MP_HEADERS,       // the headers of a part
  MP_DATA,          // the value of a part
  MP_EPILOGUE       // after the last delimiter
};

#define MP_NAME_MAX 32

static int8_t mp_emit(struct webserver_t *client) {
  struct arguments_t args;
  uint16_t base = client->mpname + 1;

  client->buffer[client->ptr] = 0;
  args.name = &client->buffer[0];
  args.value = &client->buffer[base];
  args.len = client->ptr - base;
  client->ptr = base;
  client->mpemitted = 1;

  if(client->callback != NULL) {
    if(client->callback(client, &args) == -1) {
      return -1;
    }
  }
  return 0;
}

static int8_t mp_data(struct webserver_t *client, const uint8_t *buf, uint16_t len) {
  while(len > 0) {
    if(client->ptr >= WEBSERVER_BUFFER_SIZE - 1) {
      if(mp_emit(client) == -1) {
        return -1;
      }
    }
    uint16_t n = MIN(len, (WEBSERVER_BUFFER_SIZE - 1) - client->ptr);
    memcpy(&client->buffer[client->ptr], buf, n);
    client->ptr += n;
    buf += n;
    len -= n;
  }
  return 0;
}

/*
 * The name of a part from its Content-Disposition header line (0
 * terminated, at client->buffer[client->mpname + 1]).
 */
static void mp_header_line(struct webserver_t *client) {
  char *line = (char *)&client->buffer[client->mpname + 1];
  if(strncasecmp_P(line, PSTR("Content-Disposition:"), 20) != 0) {
    return;
  }
  char *p = line + 20;
  while((p = strchr(p, '=')) != NULL) {
    if(p - line >= 25 && strncasecmp_P(p - 4, PSTR("name"), 4) == 0 &&
      (p[-5] == ';' || p[-5] == ' ' || p[-5] == '\t') && p[1] == '"') {
      char *name = p + 2;
      char *end = strchr(name, '"');
      uint16_t n = (end == NULL) ? 0 : (end - name);
      if(n > MP_NAME_MAX) {
        n = 0; // too long for a field this firmware knows
      }
      memmove(&client->buffer[0], name, n);
      client->buffer[n] = 0;
      client->mpname = n;
      return;
    }
    p++;
  }
}

int http_parse_multipart_body(struct webserver_t *client, unsigned char *buf, uint16_t len) {
  const char *delim = client->data.boundary;
  uint16_t i = 0;

  if(delim == NULL) {
    return -1;
  }
  if(client->totallen - client->readlen < len) {
    len = client->totallen - client->readlen; // bytes after the body are not used
  }

  while(i < len) {
    switch(client->substep) {
      case MP_PREAMBLE:
      case MP_DATA: {
        /*
         * The value up to the next '\r' that is not part of a match
         * can be copied at once.
         */
        if(client->mpmatch == 0 && client->substep == MP_DATA && buf[i] != '\r') {
          uint8_t *cr = (uint8_t *)memchr(&buf[i], '\r', len - i);
          uint16_t n = (cr == NULL) ? (len - i) : (cr - &buf[i]);
          if(mp_data(client, &buf[i], n) == -1) {
            return -1;
          }
          i += n;
          break;
        }
        char c = (char)buf[i++];
        if(c == delim[client->mpmatch]) {
          if(++client->mpmatch == client->mpdlen) {
            client->mpmatch = 0;
            if(client->substep == MP_DATA && (client->ptr > client->mpname + 1 || client->mpemitted == 0)) {
              if(mp_emit(client) == -1) {
                return -1;
              }
            }
            client->substep = MP_AFTER_DELIM;
          }
          break;
        }
        if(client->mpmatch > 0) {
          if(client->substep == MP_DATA) {
            if(mp_data(client, (const uint8_t *)delim, client->mpmatch) == -1) {
              return -1;
            }
          }
          client->mpmatch = 0;
        }
        if(c == delim[0]) {
          client->mpmatch = 1;
        } else if(client->substep == MP_DATA) {
          if(mp_data(client, (const uint8_t *)&c, 1) == -1) {
            return -1;
          }
        }
      } break;
      case MP_AFTER_DELIM: {
        char c = (char)buf[i++];
        if(c == '-') {
          client->substep = MP_AFTER_DASH;
        } else if(c == '\r') {
          client->substep = MP_AFTER_CR;
        } else if(c != ' ' && c != '\t') {
          return -1;
        }
      } break;
      case MP_AFTER_DASH: {
        if(buf[i++] != '-') {
          return -1;
        }
        client->substep = MP_EPILOGUE;
      } break;
      case MP_AFTER_CR: {
        if(buf[i++] != '\n') {
          return -1;
        }
        client->mpname = 0;
        client->buffer[0] = 0;
        client->ptr = 1;
        client->mpemitted = 0;
        client->substep = MP_HEADERS;
      } break;
      case MP_HEADERS: {
        char c = (char)buf[i++];
        if(c == '\n') {
          if(client->ptr > client->mpname + 1 && client->buffer[client->ptr-1] == '\r') {
            client->ptr--;
          }
          if(client->ptr == client->mpname + 1) {
            client->mpmatch = 0;
            client->substep = MP_DATA; // empty line: the value follows
          } else {
            client->buffer[client->ptr] = 0;
            mp_header_line(client);
            client->ptr = client->mpname + 1;
          }
        } else if(client->ptr < WEBSERVER_BUFFER_SIZE - 1) {
          client->buffer[client->ptr++] = c; // a longer line is cut, the name comes first
        }
      } break;
      case MP_EPILOGUE: {
        i = len;
      } break;
    }
  }

  client->readlen += len;
  if(client->readlen == client->totallen && client->substep != MP_EPILOGUE) {
    return -1; // the body ended before the last delimiter
  }
  return 0;
}

static PGM_P code_to_text(uint16_t code) {
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
    case 429:
      return PSTR("Too Many Requests");
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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
      // Abort this response immediately if the TCP client stalled or accepted
      // only part of the requested bytes. Cleanup happens on the next pass.
      if(client->step == WEBSERVER_CLIENT_CLOSE) return -1;
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

/*
 * A websocket message waits in the send list until webserver_loop() sends it, one packet
 * for each pass. A burst of messages (all topics changing at once) is queued in a single
 * pass, and on the ESP8266 that used up the heap. So there a client that already has
 * WEBSOCKET_MAX_QUEUE nodes waiting (two for each message) is first sent what it has, and
 * only misses the new message when that doesn't make room in time.
 */
#ifndef WEBSOCKET_FLUSH_TIMEOUT
  #define WEBSOCKET_FLUSH_TIMEOUT 250 // ms to wait for a slow client before a message is dropped
#endif
#ifndef WEBSOCKET_FLUSH_HOLDOFF
  #define WEBSOCKET_FLUSH_HOLDOFF 2000 // ms without waiting again after a client was too slow
#endif
#ifndef WEBSOCKET_FLUSH_SIZE
  #define WEBSOCKET_FLUSH_SIZE 256 // bytes for each send, small because this runs deep in the stack
#endif
#ifndef WEBSOCKET_MAX_QUEUE
  #if defined(ESP8266)
    #define WEBSOCKET_MAX_QUEUE 40
  #else
    #define WEBSOCKET_MAX_QUEUE 0 // no limit, the ESP32 has the memory for a full burst
  #endif
#endif

static uint8_t websocket_queue_full(struct webserver_t *client) {
#if WEBSERVER_MAX_SENDLIST == 0 && WEBSOCKET_MAX_QUEUE > 0
  uint16_t nr = 0;
  struct sendlist_t *tmp = client->sendlist;
  while(tmp != NULL) {
    if(++nr >= WEBSOCKET_MAX_QUEUE) {
      return 1;
    }
    tmp = tmp->next;
  }
#endif
  return 0;
}

/*
 * Send the waiting messages of one websocket client, like the sending step of
 * webserver_loop() does, until there is room again. Returns 0 when there is room.
 * Not while the webserver is busy with a request itself (an upload for example).
 */
static uint8_t websocket_make_room(struct webserver_t *client) {
  if(websocket_queue_full(client) == 0) {
    return 0;
  }
#if defined(ESP8266) && WEBSERVER_MAX_SENDLIST == 0
  /*
   * A client that didn't take its messages in time is not waited for again for
   * a while, or a burst of messages would wait the timeout for each of them.
   */
  static unsigned long gaveup = 0;
  static uint8_t hasgivenup = 0;
  if(hasgivenup == 1 && (unsigned long)(millis() - gaveup) >= WEBSOCKET_FLUSH_HOLDOFF) {
    hasgivenup = 0;
  }
  if(webserver_busy == 0 && hasgivenup == 0 && client->async == 0) {
    uint32_t start = millis();
    while(websocket_queue_full(client) == 1 && client->step == WEBSERVER_CLIENT_SENDING) {
      if((unsigned long)(millis() - start) >= WEBSOCKET_FLUSH_TIMEOUT) {
        hasgivenup = 1;
        gaveup = millis();
        break;
      }
      /*
       * Only send what the TCP stack takes right now, so the write
       * itself never waits and the timeout above is the real limit.
       */
      if(client->client->availableForWrite() < WEBSOCKET_FLUSH_SIZE) {
        yield();
        continue;
      }
      client->totallen = WEBSOCKET_FLUSH_SIZE;
      if(webserver_process_send(client) <= 0) {
        break;
      }
    }
  }
#endif
  return websocket_queue_full(client);
}

void websocket_write_P(struct webserver_t *client, PGM_P data, uint16_t data_len) {
  if(websocket_make_room(client)) {
    return;
  }
  websocket_send_header(client, WEBSOCKET_OPCODE_TEXT, data_len);
  webserver_send_content_P(client, data, data_len);
  client->step = WEBSERVER_CLIENT_SENDING;
}

void websocket_write(struct webserver_t *client, char *data, uint16_t data_len) {
  if(websocket_make_room(client)) {
    return;
  }
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
  /*
   * Only a whole frame in this read is used: one that is cut off (or
   * claims to be longer than what was read) is dropped, so nothing is
   * read or written past the received data.
   */
  if(buf_len < 6) {
    return -1;
  }
  packet_length = ((unsigned char)buf[1]) & 0x7F;

  index_first_mask = 2;
  if(packet_length == 126) {
    if(buf_len < 8) {
      return -1;
    }
    index_first_mask = 4;
    packet_length = buf[2] << 8 | buf[3];
  } else if(packet_length == 127) {
    if(buf_len < 14 || buf[2] != 0 || buf[3] != 0 || buf[4] != 0 || buf[5] != 0) {
      /*
       * Packet length too big
       */
      return -1;
    } else {
      packet_length = (uint32_t)buf[6] << 24 | (uint32_t)buf[7] << 16 | (uint32_t)buf[8] << 8 | buf[9];
    }
    index_first_mask = 10;
  }
  if(buf_len < index_first_mask + 4) {
    return -1;
  }
  memcpy(mask, &buf[index_first_mask], 4);
  index_first_data_byte = index_first_mask + 4;
  if(packet_length > (uint32_t)(buf_len - index_first_data_byte)) {
    return -1;
  }

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

/*
 * All headers are in: give the request to the callback and, when it is
 * accepted, give it the query arguments and prepare the body or the
 * websocket upgrade. -1 closes the connection.
 */
static int8_t webserver_request_ready(struct webserver_t *client) {
  struct webserver_reqbuf_t *rb = client->req;
  struct webserver_request_t request;
  uint8_t i = 0;
  int8_t ret = WEBSERVER_ACCEPT;

  memset(&request, 0, sizeof(request));
  request.method = client->method;
  request.path = reqbuf_get(rb, REQ_FIELD_PATH);
  request.query = reqbuf_get(rb, REQ_FIELD_QUERY);
  if(request.query == NULL) {
    request.query = "";
  }
  for(i=0;i<WEBSERVER_HEADER_COUNT;i++) {
    request.header[i] = reqbuf_get(rb, i);
  }
  request.truncated = rb->truncated & ((1 << WEBSERVER_HEADER_COUNT) - 1);

  const char *clen = reqbuf_get(rb, REQ_FIELD_CONTENT_LENGTH);
  if(clen != NULL) {
    uint32_t n = 0;
    uint8_t digits = 0;
    for(;*clen != '\0';clen++, digits++) {
      if(*clen < '0' || *clen > '9' || digits >= 9) {
        return -1;
      }
      n = (n * 10) + (*clen - '0');
    }
    if(digits == 0) {
      return -1;
    }
    request.content_length = n;
  }

  /*
   * A websocket upgrade only on its own path, and only a complete one.
   */
  const char *upgrade = reqbuf_get(rb, REQ_FIELD_UPGRADE);
  const char *key = reqbuf_get(rb, REQ_FIELD_WS_KEY);
  const char *version = reqbuf_get(rb, REQ_FIELD_WS_VERSION);
  if(client->method == WEBSERVER_METHOD_GET && strcmp_P(request.path, PSTR(WEBSERVER_WEBSOCKET_PATH)) == 0 &&
     upgrade != NULL && strcasecmp_P(upgrade, PSTR("websocket")) == 0 &&
     key != NULL && strlen(key) == 24 && version != NULL && strcmp_P(version, PSTR("13")) == 0) {
    request.websocket = 1;
  }

  client->totallen = (client->method == WEBSERVER_METHOD_POST) ? request.content_length : 0;
  client->readlen = 0;
  client->ptr = 0;
  client->substep = 0;

  if(client->callback != NULL) {
    client->step = WEBSERVER_CLIENT_REQUEST;
    ret = client->callback(client, &request);
    if(ret == -1) {
      return -1;
    }
  }

  if(ret != WEBSERVER_ACCEPT) {
    /*
     * Rejected: no query arguments, no body processing and no upgrade.
     * A body is read (and thrown away) before the answer is sent, so
     * the client sees the answer and not a reset connection.
     */
    client->rejected = 1;
    reqbuf_free(client);
    client->step = (client->totallen > 0) ? WEBSERVER_CLIENT_ARGS : WEBSERVER_CLIENT_WRITE;
    return 0;
  }

  client->step = WEBSERVER_CLIENT_ARGS;
  if(http_parse_query(client, reqbuf_get(rb, REQ_FIELD_QUERY)) == -1) {
    return -1;
  }

  if(request.websocket == 1) {
    client->is_websocket = 1;
    client->lastping = millis();
    send_websocket_handshake(client, key);
    reqbuf_free(client);
    client->step = WEBSERVER_CLIENT_WEBSOCKET;
    return 0;
  }

  if(client->method == WEBSERVER_METHOD_POST) {
    const char *ctype = request.header[WEBSERVER_HEADER_CONTENT_TYPE];
    client->reqtype = 0;
    if(ctype != NULL && strncasecmp_P(ctype, PSTR("multipart/form-data"), 19) == 0) {
      client->reqtype = 1;
      if((request.truncated & (1 << WEBSERVER_HEADER_CONTENT_TYPE)) != 0 ||
         http_multipart_boundary(client, ctype) == -1) {
        return -1; // no (or a cut) boundary: the body can't be read
      }
      client->substep = MP_PREAMBLE;
      client->mpmatch = 2; // the first delimiter has no "\r\n" before it
    }
  }
  reqbuf_free(client);

  client->step = (client->totallen > 0) ? WEBSERVER_CLIENT_ARGS : WEBSERVER_CLIENT_WRITE;
  return 0;
}

uint8_t webserver_sync_receive(struct webserver_t *client, uint8_t *rbuffer, uint16_t size) {
  if(client->step == WEBSERVER_CLIENT_READ_HEADER) {
    int32_t ret = http_parse_request(client, rbuffer, size);
    if(ret == -1) {
      client->step = WEBSERVER_CLIENT_CLOSE;
      return 0;
    }
    rbuffer += ret;
    size -= ret;
    if(client->substep != REQ_DONE) {
      return 0;
    }
    if(webserver_request_ready(client) == -1) {
      client->step = WEBSERVER_CLIENT_CLOSE;
      return 0;
    }
  }

  if(client->step == WEBSERVER_CLIENT_WEBSOCKET && size > 0) {
    websocket_read(client, rbuffer, size);
  }

  if(client->step == WEBSERVER_CLIENT_ARGS) {
    if(client->totallen - client->readlen < size) {
      size = client->totallen - client->readlen; // bytes after the body are not used
    }
    if(size > 0) {
      if(client->rejected == 1) {
        client->readlen += size;
      } else if(client->reqtype == 0) {
        if(http_parse_body(client, (char *)rbuffer, size) == -1) {
          client->step = WEBSERVER_CLIENT_CLOSE;
          return 0;
        }
      } else {
        if(http_parse_multipart_body(client, (unsigned char *)rbuffer, size) == -1) {
          client->step = WEBSERVER_CLIENT_CLOSE;
          return 0;
        }
      }
    }

    if(client->readlen >= client->totallen) {
      if(client->data.boundary != NULL) {
        free(client->data.boundary);
        client->data.boundary = NULL;
      }
      client->ptr = 0;
      client->substep = 0;
      client->step = WEBSERVER_CLIENT_WRITE;
    } else {
      return 1;
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
#if defined(ESP32)
  web_tx_release(client);
  client->tx_last_progress = 0;
  client->tx_failed = 0;
#endif
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
  reqbuf_free(client);
  client->rejected = 0;
  client->mpemitted = 0;
  client->mpmatch = 0;
  client->mpdlen = 0;
  client->mpname = 0;

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

/*
 * Let every other open connection be closed by webserver_loop() (with its
 * WEBSERVER_CLIENT_CLOSE callback), for example after the web password
 * changed: a request or websocket accepted before must not go on.
 */
void webserver_close_others(struct webserver_t *keep) {
  for(uint8_t i = 0; i < WEBSERVER_MAX_CLIENTS; i++) {
    if(&clients[i].data != keep && clients[i].data.step != 0) {
      clients[i].data.step = WEBSERVER_CLIENT_CLOSE;
    }
  }
}

void webserver_loop(void) {
  uint16_t size = 0;
  uint8_t i = 0;

  webserver_busy = 1;

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

#if defined(ESP32)
    /*
     * A previous staging failure must remain fatal even when callbacks
     * have overwritten the client's step after an unsuccessful enqueue.
     */
    if(clients[i].data.tx_failed) {
      clients[i].data.step = WEBSERVER_CLIENT_CLOSE;
      clients[i].data.tx_used = 0;
    }
    /*
     * Flush pending bytes before generating another HTTP sendlist batch.
     * A CLOSE response must be drained before the client is stopped.
     */
    if(clients[i].data.tx_used > 0) {
      web_tx_status_t flush_status = web_tx_flush(&clients[i].data);
      if(flush_status != WEB_TX_DONE) {
        continue;
      }
      // Once drained, only stage the next batch; do not send it here.
    }
#endif

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
            size = clients[i].data.client->read(p, WEBSERVER_READ_SIZE);
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
  webserver_busy = 0;
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
