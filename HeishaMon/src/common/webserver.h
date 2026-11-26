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

/*
 * Limits of what the webserver keeps of a request until the callback
 * has decided about it (WEBSERVER_CLIENT_REQUEST). The space is taken
 * from the heap while the request line and the headers come in, only as
 * far as needed, and is given back right after the decision.
 */
#ifndef WEBSERVER_MAX_PATH
  #define WEBSERVER_MAX_PATH 128         // longer request path: connection closed
#endif
#ifndef WEBSERVER_MAX_QUERY
  #define WEBSERVER_MAX_QUERY 1024       // longer query string: connection closed
#endif
#ifndef WEBSERVER_MAX_HEADER_SIZE
  #define WEBSERVER_MAX_HEADER_SIZE 4096 // request line and headers together, more: connection closed
#endif
#ifndef WEBSERVER_MAX_COOKIE
  #define WEBSERVER_MAX_COOKIE 256       // longer values are cut and marked in request->truncated
#endif
#ifndef WEBSERVER_MAX_AUTHORIZATION
  #define WEBSERVER_MAX_AUTHORIZATION 192
#endif
#ifndef WEBSERVER_MAX_ORIGIN
  #define WEBSERVER_MAX_ORIGIN 128
#endif
#ifndef WEBSERVER_MAX_HOST
  #define WEBSERVER_MAX_HOST 64
#endif
#ifndef WEBSERVER_MAX_CONTENT_TYPE
  #define WEBSERVER_MAX_CONTENT_TYPE 128 // a multipart request with a longer one is closed (boundary lost)
#endif

/*
 * The only path on which a websocket upgrade is possible (the one the
 * web page connects to).
 */
#ifndef WEBSERVER_WEBSOCKET_PATH
  #define WEBSERVER_WEBSOCKET_PATH "/ws"
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

/*
 * Return values of the callback for WEBSERVER_CLIENT_REQUEST
 * (-1 closes the connection, as for every other step).
 */
#define WEBSERVER_ACCEPT 0
#define WEBSERVER_REJECT 1

#define WEBSERVER_METHOD_GET 0
#define WEBSERVER_METHOD_POST 1

/*
 * The request headers the webserver keeps for the callback, index into
 * webserver_request_t.header[] and bit in webserver_request_t.truncated.
 * All other headers are skipped.
 */
enum {
  WEBSERVER_HEADER_HOST = 0,
  WEBSERVER_HEADER_ORIGIN,
  WEBSERVER_HEADER_COOKIE,
  WEBSERVER_HEADER_AUTHORIZATION,
  WEBSERVER_HEADER_CONTENT_TYPE,
  WEBSERVER_HEADER_COUNT
};

/*
 * What the callback gets with WEBSERVER_CLIENT_REQUEST: the whole request
 * line and the headers, before any of the query arguments, the body or a
 * websocket upgrade is handled. The pointers are only valid during this
 * call.
 */
typedef struct webserver_request_t {
  uint8_t method;                 // WEBSERVER_METHOD_GET or WEBSERVER_METHOD_POST
  uint8_t websocket;              // 1: a valid websocket upgrade on WEBSERVER_WEBSOCKET_PATH
  uint8_t truncated;              // bit (1 << WEBSERVER_HEADER_x): value longer than its limit, cut
  uint32_t content_length;        // 0 without a Content-Length header
  const char *path;               // as sent, not url decoded, without the query
  const char *query;              // as sent (not url decoded), "" when there is none
  const char *header[WEBSERVER_HEADER_COUNT]; // NULL when the header was not sent
} webserver_request_t;

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
  int (*write)(const unsigned char *, int i);
  int (*write_P)(const char *, int i);
  int (*available)();
  int (*connected)();
  void (*stop)();
  int (*read)(uint8_t *buffer, int size);
};
  #define PGM_P const char *
#endif

typedef struct webserver_t {
  tcp_pcb *pcb;
  WiFiClient *client;
  unsigned long lastseen;
  unsigned long lastping;
#if defined(ESP32)
  // Buffered HTTP TX, flushed incrementally from webserver_loop().
  uint8_t *tx_data;
  uint16_t tx_used;
  unsigned long tx_last_progress;
  uint8_t tx_failed;
#endif
  uint8_t is_websocket:1;
  uint8_t reqtype:1;
  uint8_t async:1;
  uint8_t method:1;
  uint8_t chunked:4;
  uint8_t step:4;
  uint8_t substep:4;
  uint8_t rejected:1;   // the callback rejected the request: body is skipped, not parsed
  uint8_t mpemitted:1;  // multipart: a part of the current value was already given to the callback
  uint8_t mpmatch;      // multipart: bytes of the boundary delimiter matched so far
  uint8_t mpdlen;       // multipart: length of the boundary delimiter
  uint8_t mpname;       // multipart: length of the name of the current field
  struct webserver_reqbuf_t *req; // request line and headers until the callback decided
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
    char *boundary;     // multipart: "\r\n--" followed by the boundary
  } data;
  void *userdata;
} webserver_t;

typedef struct webserver_client_t {
  struct webserver_t data;
} webserver_client_t;

typedef enum {
  WEBSERVER_CLIENT_CONNECTING = 1,
  WEBSERVER_CLIENT_REQUEST,         // data: struct webserver_request_t *, return WEBSERVER_ACCEPT, WEBSERVER_REJECT or -1
  WEBSERVER_CLIENT_READ_HEADER,
  WEBSERVER_CLIENT_CREATE_HEADER,
  WEBSERVER_CLIENT_WRITE,
  WEBSERVER_CLIENT_WEBSOCKET,
  WEBSERVER_CLIENT_WEBSOCKET_TEXT,
  WEBSERVER_CLIENT_SENDING,
  WEBSERVER_CLIENT_ARGS,            // data: struct arguments_t *, query arguments first, then the body
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
