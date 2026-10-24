/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#ifndef MTU_SIZE
  #define MTU_SIZE 1460
#endif

#ifndef WEBSERVER_READ_SIZE
  #define WEBSERVER_READ_SIZE 2*MTU_SIZE
#endif

#ifndef WEBSERVER_BUFFER_SIZE
  #ifdef ESP8266
    #define WEBSERVER_BUFFER_SIZE 128
  #else
	#define WEBSERVER_BUFFER_SIZE 512  
  #endif
#endif

#ifndef WEBSERVER_MAX_CLIENTS
  #define WEBSERVER_MAX_CLIENTS 5
#endif

#ifndef WEBSERVER_MAX_SENDLIST
#define WEBSERVER_MAX_SENDLIST 0
#endif

#ifndef WEBSERVER_CLIENT_TIMEOUT
  #define WEBSERVER_CLIENT_TIMEOUT 30000
#endif

#ifndef WEBSERVER_CLIENT_PING_INTERVAL
  #define WEBSERVER_CLIENT_PING_INTERVAL 3000
#endif

#ifndef __linux__
  #include <Arduino.h>
  #include "lwip/opt.h"
  #include "lwip/tcp.h"
  #include "lwip/inet.h"
  #include "lwip/dns.h"
  #include "lwip/init.h"
  #include "lwip/errno.h"
  #include <errno.h>
  #include <WiFiServer.h>
  #include <WiFiClient.h>
#endif

#if !defined(err_t) && !defined(ESP8266) && !defined(ESP32)
  #define err_t uint8_t
#endif

#if !defined(ESP8266) && !defined(ESP32)
typedef struct tcp_pcb {
} tcp_pcb;

typedef struct pbuf {
  unsigned int len;
  void *payload;
  struct pbuf *next;
} pbuf;
#endif

typedef struct header_t {
  unsigned char *buffer;
  uint16_t ptr;
} header_t;

struct webserver_t;
extern struct webserver_client_t clients[WEBSERVER_MAX_CLIENTS];

typedef int8_t (webserver_cb_t)(struct webserver_t *client, void *data);

typedef struct arguments_t {
  unsigned char *name;
  unsigned char *value;
  uint16_t len;
} arguments_t;

typedef struct sendlist_t {
  union {
    void *ptr;
  } data;
  uint16_t type:1;
  uint16_t size:15;
#if WEBSERVER_MAX_SENDLIST == 0
  struct sendlist_t *next;
#endif
} sendlist_t;

#if !defined(ESP8266) && !defined(ESP32)
struct WiFiClient {
  int (*write)(unsigned char *, int i);
  int (*write_P)(const char *, int i);
  int (*available)();
  int (*connected)();
  void (*stop)();
  int (*read)(uint8_t *buffer, int size);
};
  #define PGM_P unsigned char *
#endif

typedef struct webserver_t {
  tcp_pcb *pcb;
  WiFiClient *client;
  unsigned long lastseen;
  unsigned long lastping;
#if defined(ESP32)
  // Synchronous HTTP TX staging, drained incrementally by webserver_loop().
  uint8_t *tx_data;
  uint16_t tx_used;
  uint16_t tx_sent;
  unsigned long tx_last_progress;
#endif
  uint8_t is_websocket:1;
  uint8_t reqtype:1;
  uint8_t async:1;
  uint8_t method:1;
  uint8_t chunked:4;
  uint8_t step:4;
  uint8_t substep:4;
  uint16_t ptr;
  uint32_t totallen;
  uint32_t readlen;
  uint16_t content;
  uint8_t route;
#if WEBSERVER_MAX_SENDLIST == 0
  struct sendlist_t *sendlist;
  struct sendlist_t *sendlist_head;
#else
  struct sendlist_t sendlist[WEBSERVER_MAX_SENDLIST];
#endif
  webserver_cb_t *callback;
  unsigned char buffer[WEBSERVER_BUFFER_SIZE];
  union {
    char *boundary;
    char *websockkey;
  } data;
  void *userdata;
} webserver_t;

typedef struct webserver_client_t {
  struct webserver_t data;
} webserver_client_t;

typedef enum {
  WEBSERVER_CLIENT_CONNECTING = 1,
  WEBSERVER_CLIENT_REQUEST_METHOD,
  WEBSERVER_CLIENT_REQUEST_URI,
  WEBSERVER_CLIENT_READ_HEADER,
  WEBSERVER_CLIENT_CREATE_HEADER,
  WEBSERVER_CLIENT_WRITE,
  WEBSERVER_CLIENT_WEBSOCKET,
  WEBSERVER_CLIENT_WEBSOCKET_TEXT,
  WEBSERVER_CLIENT_SENDING,
  WEBSERVER_CLIENT_HEADER,
  WEBSERVER_CLIENT_ARGS,
  WEBSERVER_CLIENT_CLOSE,
} webserver_steps;

enum {
	WEBSOCKET_OPCODE_CONTINUATION = 0x0,
	WEBSOCKET_OPCODE_TEXT = 0x1,
	WEBSOCKET_OPCODE_BINARY = 0x2,
	WEBSOCKET_OPCODE_CONNECTION_CLOSE = 0x8,
	WEBSOCKET_OPCODE_PING = 0x9,
	WEBSOCKET_OPCODE_PONG = 0xa
};

int8_t webserver_start(int port, webserver_cb_t *callback, uint8_t async);
void webserver_loop(void);
void websocket_write_all_P(PGM_P data, uint16_t data_len);
void websocket_write_all(char *data, uint16_t data_len);
void websocket_write_P(struct webserver_t *client, PGM_P data, uint16_t data_len);
void websocket_write(struct webserver_t *client, char *data, uint16_t data_len);
void websocket_send_header(struct webserver_t *client, uint8_t opcode, uint16_t data_len);
void webserver_send_content(struct webserver_t *client, char *buf, uint16_t len);
void webserver_send_content_P(struct webserver_t *client, PGM_P buf, uint16_t len);
err_t webserver_async_receive(void *arg, tcp_pcb *pcb, struct pbuf *data, err_t err);
uint8_t webserver_sync_receive(struct webserver_t *client, uint8_t *rbuffer, uint16_t size);
void webserver_loop(void);
int16_t urldecode(const unsigned char *src, int src_len, unsigned char *dst, int dst_len, int is_form_url_encoded);
int8_t webserver_send(struct webserver_t *client, uint16_t code, char *mimetype, uint16_t data_len);
void webserver_client_stop(struct webserver_t *client);
void webserver_reset_client(struct webserver_t *client);

#endif
