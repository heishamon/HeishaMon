/*
  Host stubs for compiling HeishaMon/src/common/webserver.cpp with g++
  (tests/webserver). Only what webserver.cpp needs on __linux__: the
  PROGMEM helpers map to the plain C functions, time is a fake clock and
  the lwIP calls of the (unused) async mode do nothing.
*/
#ifndef WEBSERVER_TEST_UNITTEST_H
#define WEBSERVER_TEST_UNITTEST_H

#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#define PROGMEM
#define PSTR(s) (s)
#define memcmp_P memcmp
#define memcpy_P memcpy
#define strcmp_P strcmp
#define strncmp_P strncmp
#define strcasecmp_P strcasecmp
#define strncasecmp_P strncasecmp
#define strstr_P strstr
#define strncpy_P strncpy
#define strlen_P strlen
#define snprintf_P snprintf
#define pgm_read_byte(p) (*(const uint8_t *)(p))

unsigned long millis(void);
static inline void yield(void) {}

#define TCP_WRITE_FLAG_COPY 0x01
#define TCP_WRITE_FLAG_MORE 0x02
static inline int tcp_write(tcp_pcb *, const void *, uint16_t, uint8_t) { return 0; }
static inline uint16_t tcp_write_P(tcp_pcb *, const char *, uint16_t, uint8_t) { return 0; }
static inline int tcp_output(tcp_pcb *) { return 0; }
static inline uint16_t tcp_sndbuf(tcp_pcb *) { return 0; }
static inline void tcp_recved(tcp_pcb *, uint16_t) {}
static inline void pbuf_free(struct pbuf *) {}

#endif
