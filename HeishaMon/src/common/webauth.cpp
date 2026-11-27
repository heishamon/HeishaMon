/*
  Optional login for the web interface and the HTTP API (see webauth.h).

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifdef __linux__
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <strings.h>
  #include <ctype.h>
  #include "webauth.h"
  #include "unittest.h"
#else
  #define LWIP_INTERNAL  // as webserver.cpp: webserver.h pulls in lwIP, which needs this before Arduino.h
  #include <Arduino.h>
  #include <WiFiClient.h>
  #include <WiFiServer.h>
  #include "webauth.h"
#endif

webauth_store_t webauth_store;

/* ------------------------------------------------------------------ */
/* Route table                                                         */
/* ------------------------------------------------------------------ */

/*
 * Every route number of webserver_cb() (HeishaMon.ino) is in this table;
 * tests/webauth checks that. The path is matched by webauth_find_route()
 * in this order. A route only reached by the code has no path.
 *
 * Policy: "0" means the login is needed (when it is on). Only the login
 * itself, the 404 answer and the captive portal checks of a phone on the
 * setup hotspot are public.
 */
#if defined(ESP32) || defined(WEBAUTH_ALL_ROUTES)
  #define WEBAUTH_HAS_MODBUS 1
#endif
#if defined(ESP32) || defined(TLS_SUPPORT) || defined(WEBAUTH_ALL_ROUTES)
  #define WEBAUTH_HAS_CACERT 1
#endif

static const char p_root[] PROGMEM = "/";
static const char p_modbus[] PROGMEM = "/modbus";
static const char p_json[] PROGMEM = "/json";
static const char p_reboot[] PROGMEM = "/reboot";
static const char p_debug[] PROGMEM = "/debug";
static const char p_wifiscan[] PROGMEM = "/wifiscan";
static const char p_dallasalias[] PROGMEM = "/dallasalias";
static const char p_removedallas[] PROGMEM = "/removedallas";
static const char p_dallasoffset[] PROGMEM = "/dallasoffset";
static const char p_togglelog[] PROGMEM = "/togglelog";
static const char p_togglehexdump[] PROGMEM = "/togglehexdump";
static const char p_connecttest[] PROGMEM = "/connecttest.txt";
static const char p_ncsi[] PROGMEM = "/ncsi.txt";
static const char p_redirect[] PROGMEM = "/redirect";
static const char p_fwlink[] PROGMEM = "/fwlink";
static const char p_generate204[] PROGMEM = "/generate_204";
static const char p_gen204[] PROGMEM = "/gen_204";
static const char p_popup[] PROGMEM = "/popup";
static const char p_hotspotdetect[] PROGMEM = "/hotspot-detect.html";
static const char p_factoryreset[] PROGMEM = "/factoryreset";
static const char p_gpio[] PROGMEM = "/gpio";
static const char p_command[] PROGMEM = "/command";
static const char p_login[] PROGMEM = "/login";
static const char p_logout[] PROGMEM = "/logout";
static const char p_security[] PROGMEM = "/security";
static const char p_savesettings[] PROGMEM = "/savesettings";
static const char p_saverules[] PROGMEM = "/saverules";
static const char p_cacert[] PROGMEM = "/cacert";
static const char p_firmware[] PROGMEM = "/firmware";
static const char p_settings[] PROGMEM = "/settings";
static const char p_getsettings[] PROGMEM = "/getsettings";
static const char p_rules[] PROGMEM = "/rules";
static const char p_scandallas[] PROGMEM = "/scandallas";

#define WA_ANY 0
#define WA_GET 1
#define WA_POST 2
#define WA_LOGIN 0
#define WA_STATE WEBAUTH_STATE
#define WA_PAGE WEBAUTH_PAGE

const webauth_route_t webauth_routes[] PROGMEM = {
  { 210, WEBAUTH_WS, WA_ANY, NULL },                    // websocket upgrade on WEBSERVER_WEBSOCKET_PATH (live log)
  { 1, WA_PAGE, WA_ANY, p_root },                          // start page
  { 200, WA_PAGE, WA_ANY, p_modbus },                      // Modbus register list (ESP32)
  { 20, WA_LOGIN, WA_ANY, p_json },                        // heat pump data as JSON
  { 30, WA_STATE, WA_ANY, p_reboot },                      // reboot (Home Assistant button)
  { 40, WA_LOGIN, WA_ANY, p_debug },                       // raw data hex dump
  { 50, WA_LOGIN, WA_ANY, p_wifiscan },                    // WiFi scan results
  { 60, WA_STATE, WA_ANY, p_dallasalias },                 // set 1-wire aliases
  { 190, WA_STATE, WA_ANY, p_removedallas },               // forget a 1-wire sensor
  { 195, WA_STATE, WA_ANY, p_dallasoffset },               // 1-wire calibration offset
  { 2, WA_PAGE | WA_STATE, WA_ANY, p_togglelog },             // toggle the MQTT log, then the start page
  { 3, WA_PAGE | WA_STATE, WA_ANY, p_togglehexdump },         // toggle the hexdump log, then the start page
  { 80, WEBAUTH_PUBLIC | WEBAUTH_ANYHOST, WA_ANY, p_connecttest },   // captive portal checks: 302 to the setup page
  { 80, WEBAUTH_PUBLIC | WEBAUTH_ANYHOST, WA_ANY, p_ncsi },
  { 80, WEBAUTH_PUBLIC | WEBAUTH_ANYHOST, WA_ANY, p_redirect },
  { 80, WEBAUTH_PUBLIC | WEBAUTH_ANYHOST, WA_ANY, p_fwlink },
  { 80, WEBAUTH_PUBLIC | WEBAUTH_ANYHOST, WA_ANY, p_generate204 },
  { 80, WEBAUTH_PUBLIC | WEBAUTH_ANYHOST, WA_ANY, p_gen204 },
  { 80, WEBAUTH_PUBLIC | WEBAUTH_ANYHOST, WA_ANY, p_popup },
  { 81, WEBAUTH_PUBLIC | WEBAUTH_ANYHOST, WA_ANY, p_hotspotdetect }, // Apple captive portal check: static link page
  { 90, WA_PAGE | WA_STATE, WA_ANY, p_factoryreset },         // factory reset
  { 191, WA_STATE, WA_ANY, p_gpio },                       // GPIO read and switch
  { 100, WA_STATE, WA_ANY, p_command },                    // heat pump commands
  { WEBAUTH_ROUTE_LOGIN_PAGE, WEBAUTH_PUBLIC | WA_PAGE, WA_GET, p_login },
  { WEBAUTH_ROUTE_LOGIN_POST, WEBAUTH_PUBLIC | WA_STATE, WA_POST, p_login },
  { WEBAUTH_ROUTE_LOGOUT, WA_PAGE | WA_STATE, WA_ANY, p_logout },
  { WEBAUTH_ROUTE_SECURITY_PAGE, WA_PAGE, WA_GET, p_security },   // access page: web password, API key, extra host names
  { WEBAUTH_ROUTE_SECURITY_POST, WA_STATE, WA_POST, p_security },
  { 110, WA_STATE, WA_POST, p_savesettings },              // save settings
  { 111, WA_STATE | WEBAUTH_INTERNAL, WA_ANY, NULL },      // save settings: answer "new password" (webfunctions.cpp)
  { 112, WA_STATE | WEBAUTH_INTERNAL, WA_ANY, NULL },      // save settings: answer "reconnect WiFi"
  { 113, WA_STATE | WEBAUTH_INTERNAL, WA_ANY, NULL },      // save settings: redirect to /settings
  { 170, WA_STATE, WA_POST, p_saverules },                 // save rules
  { 165, WA_STATE, WA_POST, p_cacert },                    // upload a CA certificate (TLS)
  { 150, WA_STATE, WA_POST, p_firmware },                  // firmware upload
  { 120, WA_PAGE, WA_GET, p_settings },                    // settings page
  { 130, WA_LOGIN, WA_GET, p_getsettings },                // settings as JSON (no secrets)
  { 140, WA_PAGE, WA_GET, p_firmware },                    // firmware page
  { 160, WA_PAGE, WA_GET, p_rules },                       // rules page (also the answer of a failed /saverules)
  { 166, WA_PAGE, WA_GET, p_cacert },                      // CA certificate page
  { 180, WA_STATE, WA_GET, p_scandallas },                 // rescan 1-wire
  { 0, WEBAUTH_PUBLIC | WEBAUTH_INTERNAL, WA_ANY, NULL },          // 404
  { WEBAUTH_ROUTE_TO_LOGIN, WEBAUTH_PUBLIC | WEBAUTH_INTERNAL, WA_ANY, NULL },
  { WEBAUTH_ROUTE_UNAUTHORIZED, WEBAUTH_PUBLIC | WEBAUTH_INTERNAL, WA_ANY, NULL },
  { WEBAUTH_ROUTE_FORBIDDEN, WEBAUTH_PUBLIC | WEBAUTH_INTERNAL, WA_ANY, NULL },
  { WEBAUTH_ROUTE_TOO_MANY, WEBAUTH_PUBLIC | WEBAUTH_INTERNAL, WA_ANY, NULL },
  { WEBAUTH_ROUTE_BAD_HOST, WEBAUTH_PUBLIC | WEBAUTH_INTERNAL, WA_ANY, NULL },
};
const uint8_t webauth_routes_count = sizeof(webauth_routes) / sizeof(webauth_routes[0]);

#undef WA_ANY
#undef WA_GET
#undef WA_POST
#undef WA_LOGIN
#undef WA_STATE
#undef WA_PAGE

static void route_at(uint8_t i, webauth_route_t *out) {
  memcpy_P(out, &webauth_routes[i], sizeof(*out));
}

uint8_t webauth_route_flags(uint8_t route) {
  webauth_route_t r;
  for(uint8_t i = 0; i < webauth_routes_count; i++) {
    route_at(i, &r);
    if(r.route == route) {
      return r.flags;
    }
  }
  return WEBAUTH_STATE; // not in the table: login needed, state changing
}

int16_t webauth_find_route(const struct webserver_request_t *req) {
  if(req->websocket) {
    return 210;
  }
  uint8_t post = (req->method == WEBSERVER_METHOD_POST);
  webauth_route_t r;
  for(uint8_t i = 0; i < webauth_routes_count; i++) {
    route_at(i, &r);
    if(r.path == NULL) {
      continue;
    }
#ifndef WEBAUTH_HAS_MODBUS
    if(r.route == 200) {
      continue;
    }
#endif
#ifndef WEBAUTH_HAS_CACERT
    if(r.route == 165 || r.route == 166) {
      continue;
    }
#endif
    if((r.method == 1 && post) || (r.method == 2 && !post)) {
      continue;
    }
    if(strcmp_P(req->path, r.path) == 0) {
      return r.route;
    }
  }
  return post ? -1 : 0; // an unknown POST is closed, an unknown GET gets the 404
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static const char hexdigits[] PROGMEM = "0123456789abcdef";

static void to_hex(const uint8_t *in, size_t len, char *out) {
  for(size_t i = 0; i < len; i++) {
    out[2 * i] = pgm_read_byte(&hexdigits[in[i] >> 4]);
    out[2 * i + 1] = pgm_read_byte(&hexdigits[in[i] & 0x0f]);
  }
  out[2 * len] = 0;
}

static int hexval(char c) {
  if(c >= '0' && c <= '9') return c - '0';
  if(c >= 'a' && c <= 'f') return c - 'a' + 10;
  if(c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool from_hex(const char *in, size_t len, uint8_t *out, size_t outlen) {
  if(len != 2 * outlen) {
    return false;
  }
  for(size_t i = 0; i < outlen; i++) {
    int h = hexval(in[2 * i]), l = hexval(in[2 * i + 1]);
    if(h < 0 || l < 0) {
      return false;
    }
    out[i] = (uint8_t)((h << 4) | l);
  }
  return true;
}

static char lower(char c) {
  return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* case insensitive compare of a with the first blen bytes of b */
static bool equal_nocase(const char *a, size_t alen, const char *b, size_t blen) {
  if(alen != blen) {
    return false;
  }
  for(size_t i = 0; i < alen; i++) {
    if(lower(a[i]) != lower(b[i])) {
      return false;
    }
  }
  return true;
}

static void wipe(void *p, size_t len) {
  volatile uint8_t *v = (volatile uint8_t *)p;
  while(len--) {
    *v++ = 0;
  }
}

bool webauth_equal(const uint8_t *a, const uint8_t *b, size_t len) {
  uint8_t diff = 0;
  for(size_t i = 0; i < len; i++) {
    diff |= (uint8_t)(a[i] ^ b[i]);
  }
  return diff == 0;
}

static unsigned long now_ms(void) {
  return webauth_platform_millis();
}

/* ------------------------------------------------------------------ */
/* Password hash: PBKDF2-HMAC-SHA256 (RFC 8018), one 32 byte block     */
/* ------------------------------------------------------------------ */

static void hmac_sha256(const uint8_t key[64], const uint8_t *msg, size_t mlen, uint8_t out[32]) {
  uint8_t pad[64], inner[32];
  for(uint8_t i = 0; i < 64; i++) {
    pad[i] = key[i] ^ 0x36;
  }
  webauth_platform_sha256(pad, 64, msg, mlen, inner);
  for(uint8_t i = 0; i < 64; i++) {
    pad[i] = key[i] ^ 0x5c;
  }
  webauth_platform_sha256(pad, 64, inner, 32, out);
  wipe(pad, sizeof(pad));
  wipe(inner, sizeof(inner));
}

void webauth_pbkdf2_sha256(const char *password, size_t plen, const uint8_t *salt, size_t slen, uint32_t iterations, uint8_t out[32]) {
  uint8_t key[64], u[32], msg[64 + 4];
  memset(key, 0, sizeof(key));
  if(plen > 64) {
    webauth_platform_sha256((const uint8_t *)password, plen, NULL, 0, key);
  } else {
    memcpy(key, password, plen);
  }
  if(slen > 64) {
    slen = 64;
  }
  memcpy(msg, salt, slen);
  msg[slen] = 0;
  msg[slen + 1] = 0;
  msg[slen + 2] = 0;
  msg[slen + 3] = 1;  // block index 1
  hmac_sha256(key, msg, slen + 4, u);
  memcpy(out, u, 32);
  for(uint32_t i = 1; i < iterations; i++) {
    hmac_sha256(key, u, 32, u);
    for(uint8_t j = 0; j < 32; j++) {
      out[j] ^= u[j];
    }
    if((i & 31) == 0) {
      webauth_platform_yield(); // keeps WiFi and the watchdog of the ESP8266 happy
    }
  }
  wipe(key, sizeof(key));
  wipe(u, sizeof(u));
}

/* ------------------------------------------------------------------ */
/* Stored credentials                                                  */
/* ------------------------------------------------------------------ */

struct webauth_session_t {
  uint8_t token[WEBAUTH_TOKEN_BYTES];
  unsigned long created;
  unsigned long last;
  uint8_t used;
};

struct webauth_lock_t {
  uint32_t ip;
  unsigned long last;  // time of the last failure
  uint8_t fails;
  uint8_t used;
};

static struct webauth_session_t sessions[WEBAUTH_SESSIONS];
static struct webauth_lock_t locks[WEBAUTH_LOCK_ENTRIES];
// every address that failed lately, also after its entry made room for another one (Bloom filter):
// such an address never counts as "without failures" and cannot use the reserve of the budget.
// Two generations: a failure is written to the current one, every WEBAUTH_LOCK_FORGET_MS the older
// one is cleared and becomes the current one. So a failure is forgotten 15 to 30 minutes later,
// also while other addresses keep failing (one filter alone would never forget during a flood).
static uint8_t failed_seen[2][WEBAUTH_FAILED_BITS / 8];
static uint8_t failed_gen = 0;        // the generation that is written
static unsigned long failed_time = 0; // when it became the current one
struct webauth_known_t {
  uint32_t ip;
  unsigned long last;           // last login
  uint8_t used;
  struct webauth_lock_t lock;   // its own failures (never in the table or the shared entry)
};
static struct webauth_known_t known[WEBAUTH_KNOWN_ENTRIES];  // addresses that logged in lately (the owner's devices)
static uint8_t bucket = WEBAUTH_BUCKET_SIZE;
static unsigned long bucket_time = 0;
static unsigned long sweep_time = 0;
static uint8_t broken = 0;
static uint8_t save_ok = 1;

/* layout of version 1, only to take it over */
typedef struct webauth_store_v1_t {
  uint32_t magic;
  uint32_t iterations;
  uint8_t password_set;
  uint8_t apikey_set;
  uint8_t reserved[2];
  uint8_t check[8];
  uint8_t salt[WEBAUTH_SALT_BYTES];
  uint8_t password_hash[WEBAUTH_HASH_BYTES];
  uint8_t apikey_hash[WEBAUTH_HASH_BYTES];
  char hosts[WEBAUTH_HOSTS_MAX];
} webauth_store_v1_t;

static_assert(sizeof(webauth_store_t) <= WEBAUTH_STORE_MAX_SIZE && sizeof(webauth_store_v1_t) <= WEBAUTH_STORE_MAX_SIZE, "WEBAUTH_STORE_MAX_SIZE");

/* SHA-256 (first 8 bytes) of a record without its check field */
static void check_of(const void *rec, size_t len, size_t at, uint8_t out[8]) {
  uint8_t h[32];
  const uint8_t *p = (const uint8_t *)rec;
  webauth_platform_sha256(p, at, p + at + 8, len - at - 8, h);
  memcpy(out, h, 8);
}

static void store_check(const webauth_store_t *st, uint8_t out[8]) {
  check_of(st, sizeof(*st), offsetof(webauth_store_t, check), out);
}

void webauth_reset(void) {
  memset(&webauth_store, 0, sizeof(webauth_store));
  webauth_store.magic = WEBAUTH_STORE_MAGIC;
  webauth_store.version = WEBAUTH_STORE_VERSION;
  memset(sessions, 0, sizeof(sessions));
  memset(locks, 0, sizeof(locks));
  memset(known, 0, sizeof(known));
  memset(failed_seen, 0, sizeof(failed_seen));
  failed_gen = 0;
  failed_time = now_ms();
  bucket = WEBAUTH_BUCKET_SIZE;
  bucket_time = now_ms();
  sweep_time = bucket_time;
  broken = 0;
  save_ok = 1;
}

static bool hosts_chars_ok(const char *hosts) {
  for(const char *p = hosts; *p; p++) {
    char c = *p;
    if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ',' || c == ' ' || c == ':')) {
      return false;
    }
  }
  return true;
}

static bool store_valid(const webauth_store_t *st) {
  bool ok = st->password_set <= 1 && st->apikey_set <= 1 && memchr(st->hosts, 0, sizeof(st->hosts)) != NULL && hosts_chars_ok(st->hosts);
  if(ok && st->password_set) {
    ok = st->iterations >= WEBAUTH_ITERATIONS_MIN && st->iterations <= WEBAUTH_ITERATIONS_MAX;
  } else if(ok) {
    ok = st->iterations == 0 && !st->apikey_set;  // no key without a password
  }
  return ok;
}

/* version 1 -> current: the same fields */
static bool migrate_v1(const uint8_t *data, size_t len, webauth_store_t *out) {
  webauth_store_v1_t v1;
  if(len != sizeof(v1)) {
    return false;
  }
  memcpy(&v1, data, sizeof(v1));
  uint8_t check[8];
  check_of(&v1, sizeof(v1), offsetof(webauth_store_v1_t, check), check);
  bool ok = webauth_equal(check, v1.check, 8);
  if(ok) {
    memset(out, 0, sizeof(*out));
    out->magic = WEBAUTH_STORE_MAGIC;
    out->version = WEBAUTH_STORE_VERSION;
    out->password_set = v1.password_set;
    out->apikey_set = v1.apikey_set;
    out->iterations = v1.iterations;
    memcpy(out->salt, v1.salt, sizeof(out->salt));
    memcpy(out->password_hash, v1.password_hash, sizeof(out->password_hash));
    memcpy(out->apikey_hash, v1.apikey_hash, sizeof(out->apikey_hash));
    memcpy(out->hosts, v1.hosts, sizeof(out->hosts));
  }
  wipe(&v1, sizeof(v1));
  return ok;
}

uint8_t webauth_load(const void *data, size_t len) {
  if(data == NULL || len < 8) {
    return WEBAUTH_LOAD_INVALID;
  }
  const uint8_t *d = (const uint8_t *)data;
  uint32_t magic;
  memcpy(&magic, d, 4);
  webauth_store_t tmp;
  uint8_t result = WEBAUTH_LOAD_INVALID;
  if(magic == WEBAUTH_STORE_MAGIC_V1) {
    result = migrate_v1(d, len, &tmp) && store_valid(&tmp) ? WEBAUTH_LOAD_MIGRATED : WEBAUTH_LOAD_INVALID;
  } else if(magic == WEBAUTH_STORE_MAGIC) {
    uint8_t version = d[4];
    if(version > WEBAUTH_STORE_VERSION) {
      result = WEBAUTH_LOAD_NEWER;  // unknown layout: never read as "no password"
    } else if(version == WEBAUTH_STORE_VERSION && len == sizeof(tmp)) {
      memcpy(&tmp, d, sizeof(tmp));
      uint8_t check[8];
      store_check(&tmp, check);
      if(webauth_equal(check, tmp.check, 8) && store_valid(&tmp)) {
        result = WEBAUTH_LOAD_OK;
      }
    }
    // an older version with this magic: add its migration here (none yet)
  }
  if(result == WEBAUTH_LOAD_OK || result == WEBAUTH_LOAD_MIGRATED) {
    memcpy(&webauth_store, &tmp, sizeof(tmp));
    broken = 0;
  }
  wipe(&tmp, sizeof(tmp));
  return result;
}

/*
 * A stored file that is not valid is not "no password": the login stays
 * on and no password matches, until the physical factory reset.
 */
void webauth_load_failed(void) {
  memset(&webauth_store, 0, sizeof(webauth_store));
  webauth_store.magic = WEBAUTH_STORE_MAGIC;
  webauth_store.password_set = 1;  // iterations 0: webauth_check_password() never matches
  webauth_sessions_clear();
  broken = 1;
}

bool webauth_broken(void) {
  return broken != 0;
}

bool webauth_login_on(void) {
  return webauth_store.password_set != 0;
}

bool webauth_last_save_ok(void) {
  return save_ok != 0;
}

static bool save(void) {
  webauth_store.magic = WEBAUTH_STORE_MAGIC;
  webauth_store.version = WEBAUTH_STORE_VERSION;
  store_check(&webauth_store, webauth_store.check);
  save_ok = webauth_platform_save(&webauth_store) ? 1 : 0;
  return save_ok;
}

bool webauth_save(void) {
  return save();
}

bool webauth_ota_allowed(const char *ota_password) {
  return !webauth_login_on() && ota_password != NULL && ota_password[0] != 0 && strcmp_P(ota_password, PSTR("heisha")) != 0;
}

bool webauth_hostname_valid(const char *name) {
  size_t len = name ? strlen(name) : 0;
  if(len == 0 || len > WEBAUTH_HOSTNAME_MAX || name[0] == '-' || name[len - 1] == '-') {
    return false;
  }
  for(size_t i = 0; i < len; i++) {
    char c = name[i];
    if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) {
      return false;
    }
  }
  return true;
}

static bool ipv4_text(const char *ip) {
  if(ip == NULL || *ip == 0 || strcmp(ip, "0.0.0.0") == 0) {
    return false;
  }
  for(const char *p = ip; *p; p++) {
    if(!((*p >= '0' && *p <= '9') || *p == '.')) {
      return false;
    }
  }
  return true;
}

/* The Host names of this device: its IP addresses, hostname and hostname.local (only valid ones) */
void webauth_device_hosts(char *out, size_t size, const char *const *ips, uint8_t nips, const char *hostname) {
  size_t n = 0;
  out[0] = 0;
  char local[WEBAUTH_HOSTNAME_MAX + 8];
  const char *names[8];
  uint8_t count = 0;
  for(uint8_t i = 0; i < nips && count < 5; i++) {
    if(ipv4_text(ips[i])) {
      names[count++] = ips[i];
    }
  }
  if(webauth_hostname_valid(hostname)) {
    names[count++] = hostname;
    snprintf_P(local, sizeof(local), PSTR("%s.local"), hostname);
    names[count++] = local;
  }
  for(uint8_t i = 0; i < count; i++) {
    size_t l = strlen(names[i]);
    if(n + l + 2 > size) {
      break;
    }
    if(n > 0) {
      out[n++] = ',';
    }
    memcpy(out + n, names[i], l);
    n += l;
    out[n] = 0;
  }
}

size_t webauth_html_escape(const char *in, char *out, size_t size) {
  size_t n = 0;
  if(size == 0) {
    return 0;
  }
  for(const char *p = in; p && *p; p++) {
    const char *rep = NULL;
    char one[2] = { *p, 0 };
    switch(*p) {
      case '&': rep = "&amp;"; break;
      case '<': rep = "&lt;"; break;
      case '>': rep = "&gt;"; break;
      case '"': rep = "&quot;"; break;
      case '\'': rep = "&#39;"; break;
      default: rep = one;
    }
    size_t l = strlen(rep);
    if(n + l >= size) {
      break;
    }
    memcpy(out + n, rep, l);
    n += l;
  }
  out[n] = 0;
  return n;
}

bool webauth_set_password(const char *password) {
  size_t len = strlen(password);
  if(len < WEBAUTH_PASSWORD_MIN || len > WEBAUTH_PASSWORD_MAX) {
    return false;
  }
  webauth_platform_random(webauth_store.salt, sizeof(webauth_store.salt));
  webauth_store.iterations = WEBAUTH_ITERATIONS;
  webauth_pbkdf2_sha256(password, len, webauth_store.salt, sizeof(webauth_store.salt), webauth_store.iterations, webauth_store.password_hash);
  webauth_store.password_set = 1;
  broken = 0;
  webauth_sessions_clear();
  save();
  return true;
}

bool webauth_check_password(const char *password) {
  if(!webauth_store.password_set || webauth_store.iterations == 0) {
    return false;
  }
  uint8_t hash[WEBAUTH_HASH_BYTES];
  webauth_pbkdf2_sha256(password, strlen(password), webauth_store.salt, sizeof(webauth_store.salt), webauth_store.iterations, hash);
  bool ok = webauth_equal(hash, webauth_store.password_hash, sizeof(hash));
  wipe(hash, sizeof(hash));
  return ok;
}

void webauth_remove_password(void) {
  webauth_store.password_set = 0;
  webauth_store.iterations = 0;
  wipe(webauth_store.salt, sizeof(webauth_store.salt));
  wipe(webauth_store.password_hash, sizeof(webauth_store.password_hash));
  // the API key and the extra host names go with it: nothing set while the login was on outlives it
  webauth_store.apikey_set = 0;
  wipe(webauth_store.apikey_hash, sizeof(webauth_store.apikey_hash));
  wipe(webauth_store.hosts, sizeof(webauth_store.hosts));
  webauth_sessions_clear();
  save();
}

void webauth_new_apikey(char out[2 * WEBAUTH_TOKEN_BYTES + 1]) {
  uint8_t key[WEBAUTH_TOKEN_BYTES];
  webauth_platform_random(key, sizeof(key));
  to_hex(key, sizeof(key), out);
  wipe(key, sizeof(key));
  webauth_platform_sha256((const uint8_t *)out, 2 * WEBAUTH_TOKEN_BYTES, NULL, 0, webauth_store.apikey_hash);
  webauth_store.apikey_set = 1;
  save();
}

void webauth_delete_apikey(void) {
  webauth_store.apikey_set = 0;
  wipe(webauth_store.apikey_hash, sizeof(webauth_store.apikey_hash));
  save();
}

bool webauth_check_apikey(const char *key, size_t len) {
  if(!webauth_store.apikey_set || len != 2 * WEBAUTH_TOKEN_BYTES) {
    return false;
  }
  uint8_t hash[WEBAUTH_HASH_BYTES];
  webauth_platform_sha256((const uint8_t *)key, len, NULL, 0, hash);
  bool ok = webauth_equal(hash, webauth_store.apikey_hash, sizeof(hash));
  wipe(hash, sizeof(hash));
  return ok;
}

bool webauth_set_hosts(const char *hosts) {
  size_t len = strlen(hosts);
  if(len >= sizeof(webauth_store.hosts) || !hosts_chars_ok(hosts)) {
    return false;
  }
  strcpy(webauth_store.hosts, hosts);
  save();
  return true;
}

/* ------------------------------------------------------------------ */
/* Sessions                                                            */
/* ------------------------------------------------------------------ */

static bool session_valid(const struct webauth_session_t *s, unsigned long now) {
  return s->used && (unsigned long)(now - s->last) < WEBAUTH_SESSION_IDLE_MS && (unsigned long)(now - s->created) < WEBAUTH_SESSION_MAX_MS;
}

int8_t webauth_session_new(uint8_t token[WEBAUTH_TOKEN_BYTES]) {
  unsigned long now = now_ms();
  int8_t pick = -1;
  for(int8_t i = 0; i < WEBAUTH_SESSIONS; i++) {
    if(!session_valid(&sessions[i], now)) {
      pick = i;
      break;
    }
  }
  if(pick < 0) {  // all in use: the one unused the longest goes
    pick = 0;
    for(int8_t i = 1; i < WEBAUTH_SESSIONS; i++) {
      if((unsigned long)(now - sessions[i].last) > (unsigned long)(now - sessions[pick].last)) {
        pick = i;
      }
    }
  }
  webauth_platform_random(sessions[pick].token, WEBAUTH_TOKEN_BYTES);
  sessions[pick].created = now;
  sessions[pick].last = now;
  sessions[pick].used = 1;
  memcpy(token, sessions[pick].token, WEBAUTH_TOKEN_BYTES);
  return pick;
}

int8_t webauth_session_find(const uint8_t token[WEBAUTH_TOKEN_BYTES]) {
  unsigned long now = now_ms();
  int8_t found = -1;
  for(int8_t i = 0; i < WEBAUTH_SESSIONS; i++) {
    if(sessions[i].used && !session_valid(&sessions[i], now)) {
      wipe(&sessions[i], sizeof(sessions[i]));
    }
    if(sessions[i].used && webauth_equal(sessions[i].token, token, WEBAUTH_TOKEN_BYTES)) {
      found = i;
    }
  }
  return found;
}

void webauth_session_end(int8_t index) {
  if(index >= 0 && index < WEBAUTH_SESSIONS) {
    wipe(&sessions[index], sizeof(sessions[index]));
  }
}

void webauth_sessions_clear(void) {
  wipe(sessions, sizeof(sessions));
}

/*
 * The session of a Cookie header: the cookie is found by its name, in any
 * position; a cookie whose name only ends or starts like it does not count.
 */
int8_t webauth_cookie_session(const char *cookie) {
  static const char name[] PROGMEM = WEBAUTH_COOKIE_NAME;
  const size_t nlen = sizeof(name) - 1;
  const char *p = cookie;
  while(p != NULL && *p) {
    while(*p == ' ' || *p == ';' || *p == '\t') {
      p++;
    }
    const char *eq = p;
    while(*eq && *eq != '=' && *eq != ';') {
      eq++;
    }
    const char *end = eq;
    while(*end && *end != ';') {
      end++;
    }
    if(*eq == '=' && (size_t)(eq - p) == nlen && strncmp_P(p, name, nlen) == 0) {
      const char *v = eq + 1;
      const char *ve = end;
      while(ve > v && (ve[-1] == ' ' || ve[-1] == '\t')) {
        ve--;
      }
      if(ve - v >= 2 && v[0] == '"' && ve[-1] == '"') {
        v++;
        ve--;
      }
      uint8_t token[WEBAUTH_TOKEN_BYTES];
      if(from_hex(v, (size_t)(ve - v), token, sizeof(token))) {
        int8_t s = webauth_session_find(token);
        wipe(token, sizeof(token));
        if(s >= 0) {
          return s;
        }
      }
    }
    p = end;
  }
  return -1;
}

/* ------------------------------------------------------------------ */
/* Host and Origin                                                     */
/* ------------------------------------------------------------------ */

static bool in_list(const char *name, size_t nlen, const char *list) {
  const char *p = list;
  while(p != NULL && *p) {
    while(*p == ',' || *p == ' ') {
      p++;
    }
    const char *e = p;
    while(*e && *e != ',' && *e != ' ') {
      e++;
    }
    size_t elen = (size_t)(e - p);
    if(elen > 0 && !(elen == 7 && strncmp(p, "0.0.0.0", 7) == 0) && equal_nocase(name, nlen, p, elen)) {
      return true;
    }
    p = e;
  }
  return false;
}

/*
 * The Host header (without a port) must be one of the names of this
 * device or one of the extra host names of the settings. This stops DNS
 * rebinding: a page of another site that has its name pointed to this
 * device still sends its own name as Host.
 */
bool webauth_host_allowed(const char *host, const char *device_hosts) {
  if(host == NULL || *host == 0) {
    return false;
  }
  size_t len = strlen(host);
  const char *colon = strchr(host, ':');
  if(colon != NULL) {
    if(strchr(colon + 1, ':') != NULL || colon[1] == 0) {
      return false;  // IPv6 literal or an empty port
    }
    for(const char *d = colon + 1; *d; d++) {
      if(*d < '0' || *d > '9') {
        return false;
      }
    }
    len = (size_t)(colon - host);
  }
  if(len == 0) {
    return false;
  }
  return in_list(host, len, device_hosts) || in_list(host, len, webauth_store.hosts);
}

/* Origin "http://<host>" or "https://<host>" with exactly the Host header */
bool webauth_origin_matches(const char *origin, const char *host) {
  if(origin == NULL || host == NULL || *host == 0) {
    return false;
  }
  const char *rest = NULL;
  if(strncasecmp_P(origin, PSTR("http://"), 7) == 0) {
    rest = origin + 7;
  } else if(strncasecmp_P(origin, PSTR("https://"), 8) == 0) {
    rest = origin + 8;
  } else {
    return false;
  }
  return equal_nocase(rest, strlen(rest), host, strlen(host));
}

/* ------------------------------------------------------------------ */
/* Lockout                                                             */
/* ------------------------------------------------------------------ */

static void bucket_refill(unsigned long now) {
  if(bucket >= WEBAUTH_BUCKET_SIZE) {
    bucket_time = now;
    return;
  }
  unsigned long elapsed = (unsigned long)(now - bucket_time);
  unsigned long add = elapsed / WEBAUTH_BUCKET_REFILL_MS;
  if(add > 0) {
    if(add >= (unsigned long)(WEBAUTH_BUCKET_SIZE - bucket)) {
      bucket = WEBAUTH_BUCKET_SIZE;
      bucket_time = now;
    } else {
      bucket += (uint8_t)add;
      bucket_time += add * WEBAUTH_BUCKET_REFILL_MS;
    }
  }
}

static void lock_expire(struct webauth_lock_t *l, unsigned long now) {
  if(l->used && (unsigned long)(now - l->last) >= WEBAUTH_LOCK_FORGET_MS) {
    memset(l, 0, sizeof(*l));
  }
}

static struct webauth_lock_t *lock_find(uint32_t ip, unsigned long now) {
  for(uint8_t i = 0; i < WEBAUTH_LOCK_ENTRIES; i++) {
    lock_expire(&locks[i], now);
    if(locks[i].used && locks[i].ip == ip) {
      return &locks[i];
    }
  }
  return NULL;
}

static struct webauth_known_t *known_find(uint32_t ip, unsigned long now) {
  struct webauth_known_t *found = NULL;
  for(uint8_t i = 0; i < WEBAUTH_KNOWN_ENTRIES; i++) {
    if(known[i].used && (unsigned long)(now - known[i].last) >= WEBAUTH_KNOWN_MS) {
      memset(&known[i], 0, sizeof(known[i]));
    }
    if(known[i].used) {
      lock_expire(&known[i].lock, now);
      if(known[i].ip == ip) {
        found = &known[i];
      }
    }
  }
  return found;
}

static void remember(uint32_t ip, unsigned long now) {
  struct webauth_known_t *k = known_find(ip, now);
  if(k == NULL) {
    k = &known[0];
    for(uint8_t i = 0; i < WEBAUTH_KNOWN_ENTRIES; i++) {
      if(!known[i].used || (unsigned long)(now - known[i].last) > (unsigned long)(now - k->last)) {
        k = &known[i];
        if(!known[i].used) {
          break;
        }
      }
    }
    memset(k, 0, sizeof(*k));
  }
  k->ip = ip;
  k->last = now;
  k->used = 1;
  memset(&k->lock, 0, sizeof(k->lock));
}

static struct webauth_lock_t *lock_free(void) {
  for(uint8_t i = 0; i < WEBAUTH_LOCK_ENTRIES; i++) {
    if(!locks[i].used) {
      return &locks[i];
    }
  }
  return NULL;
}



/*
 * The entry that counts for an address: its own (in the table, or with
 * its known-address record). NULL: no failures of its own. Addresses are
 * never pooled, so a new address (the owner's new phone) is never held
 * back by the failures of others.
 */
static struct webauth_lock_t *lock_of(uint32_t ip, unsigned long now) {
  struct webauth_lock_t *l = lock_find(ip, now);
  if(l != NULL) {
    return l;
  }
  struct webauth_known_t *k = known_find(ip, now);
  if(k != NULL) {
    return &k->lock;
  }
  return NULL;
}

static unsigned long lock_delay(const struct webauth_lock_t *l) {
  if(l == NULL || l->fails < WEBAUTH_LOCK_FREE) {
    return 0;
  }
  uint8_t shift = (uint8_t)(l->fails - WEBAUTH_LOCK_FREE);
  if(shift > 16) {
    shift = 16;
  }
  unsigned long d = 1000UL << shift;  // 1 s, 2 s, 4 s, ...
  return d > WEBAUTH_LOCK_MAX_MS ? WEBAUTH_LOCK_MAX_MS : d;
}

/* remaining backoff of this address, 0: none */
static unsigned long ip_wait(uint32_t ip, unsigned long now) {
  struct webauth_lock_t *l = lock_of(ip, now);
  unsigned long d = lock_delay(l);
  unsigned long since = l ? (unsigned long)(now - l->last) : 0;
  return since < d ? d - since : 0;
}

static void seen_bits(uint32_t ip, uint16_t *a, uint16_t *b) {
  // murmur3 finalizer: every bit of the address moves every bit of the hash
  uint32_t h = ip;
  h ^= h >> 16;
  h *= 0x85ebca6bUL;
  h ^= h >> 13;
  h *= 0xc2b2ae35UL;
  h ^= h >> 16;
  *a = (uint16_t)(h % WEBAUTH_FAILED_BITS);
  *b = (uint16_t)((h >> 16) % WEBAUTH_FAILED_BITS);
}

/* the older generation is dropped every WEBAUTH_LOCK_FORGET_MS */
static void seen_rotate(unsigned long now) {
  unsigned long age = (unsigned long)(now - failed_time);
  if(age < WEBAUTH_LOCK_FORGET_MS) {
    return;
  }
  if(age >= 2 * WEBAUTH_LOCK_FORGET_MS) {
    memset(failed_seen, 0, sizeof(failed_seen));  // nobody looked for two periods: both are old
    failed_time = now;
    return;
  }
  failed_gen ^= 1;
  memset(failed_seen[failed_gen], 0, sizeof(failed_seen[failed_gen]));
  failed_time += WEBAUTH_LOCK_FORGET_MS;
}

static void seen_add(uint32_t ip, unsigned long now) {
  uint16_t a, b;
  seen_rotate(now);
  seen_bits(ip, &a, &b);
  failed_seen[failed_gen][a >> 3] |= (uint8_t)(1 << (a & 7));
  failed_seen[failed_gen][b >> 3] |= (uint8_t)(1 << (b & 7));
}

static bool seen_has(uint32_t ip, unsigned long now) {
  uint16_t a, b;
  seen_rotate(now);
  seen_bits(ip, &a, &b);
  for(uint8_t g = 0; g < 2; g++) {
    if((failed_seen[g][a >> 3] & (1 << (a & 7))) && (failed_seen[g][b >> 3] & (1 << (b & 7)))) {
      return true;
    }
  }
  return false;
}

/* an address without recent failures (may use the reserve of the budget) */
static bool ip_clean(uint32_t ip, unsigned long now) {
  struct webauth_lock_t *l = lock_of(ip, now);
  if(l != NULL) {
    return l->fails == 0;  // its own entry decides (a success cleared it)
  }
  return !seen_has(ip, now);
}

bool webauth_lock_clean(uint32_t ip) {
  return ip_clean(ip, now_ms());
}

/*
 * An address that logged in lately (the owner's devices) does not use the
 * global budget at all: only its own backoff holds it back. Otherwise every
 * new address of an attacker (each one is "without failures" once) takes a
 * try out of the reserve and the owner waits for minutes. Nobody else can
 * get onto that list: it needs the right password.
 */
static bool ip_known(uint32_t ip, unsigned long now) {
  return known_find(ip, now) != NULL;
}

unsigned long webauth_lock_wait_ms(uint32_t ip) {
  unsigned long now = now_ms();
  unsigned long w = ip_wait(ip, now);
  if(ip_known(ip, now)) {
    return w;
  }
  bucket_refill(now);
  uint8_t need = ip_clean(ip, now) ? 1 : WEBAUTH_BUCKET_RESERVE + 1;
  if(w == 0 && bucket < need) {
    unsigned long since = (unsigned long)(now - bucket_time);
    w = (unsigned long)(need - bucket - 1) * WEBAUTH_BUCKET_REFILL_MS + (since < WEBAUTH_BUCKET_REFILL_MS ? WEBAUTH_BUCKET_REFILL_MS - since : 1);
  }
  return w;
}

bool webauth_lock_check(uint32_t ip) {
  unsigned long now = now_ms();
  if(ip_wait(ip, now) > 0) {
    return false;  // refused tries do not count as failures
  }
  if(!ip_known(ip, now)) {
    bucket_refill(now);
    // the last tokens are kept for addresses without recent failures (the owner on a new address)
    if(bucket < (ip_clean(ip, now) ? 1 : WEBAUTH_BUCKET_RESERVE + 1)) {
      return false;
    }
    bucket--;
    if(bucket == WEBAUTH_BUCKET_SIZE - 1) {
      bucket_time = now;
    }
  }
  // counted as a failure right away: a try whose connection ends before the
  // password came still counts; a success clears it, webauth_lock_refund() gives it back
  webauth_lock_failed(ip);
  return true;
}

void webauth_lock_refund(uint32_t ip) {
  struct webauth_lock_t *l = lock_of(ip, now_ms());
  if(l != NULL && l->fails > 0) {
    l->fails--;
  }
}

void webauth_lock_failed(uint32_t ip) {
  unsigned long now = now_ms();
  struct webauth_lock_t *l = lock_of(ip, now);
  if(l == NULL) {
    l = lock_free();
    if(l == NULL) {
      // table full: the entry whose last failure is oldest among those not backing off
      // now goes; if all back off, the one with the oldest failure (it waits the longest
      // anyway). Its address only gets its free tries back, and the global budget still
      // keeps its reserve for addresses without failures.
      struct webauth_lock_t *idle = NULL, *oldest = &locks[0];
      for(uint8_t i = 0; i < WEBAUTH_LOCK_ENTRIES; i++) {
        struct webauth_lock_t *c = &locks[i];
        unsigned long since = (unsigned long)(now - c->last);
        if(since >= lock_delay(c) && (idle == NULL || since > (unsigned long)(now - idle->last))) {
          idle = c;
        }
        if(since > (unsigned long)(now - oldest->last)) {
          oldest = c;
        }
      }
      l = idle != NULL ? idle : oldest;
    }
    memset(l, 0, sizeof(*l));
    l->ip = ip;
    l->used = 1;
  } else {
    l->used = 1;  // the one of a known address
  }
  if(l->fails < 255) {
    l->fails++;
  }
  l->last = now;
  seen_add(ip, now);
}

void webauth_lock_success(uint32_t ip) {
  unsigned long now = now_ms();
  struct webauth_lock_t *l = lock_of(ip, now);
  if(l != NULL) {
    memset(l, 0, sizeof(*l));
  }
  remember(ip, now);  // from now on its failures are its own and it does not need the global budget
}

/*
 * Drops expired sessions and lockouts; called from loop() (and with every
 * request), so nothing can look valid again after millis() wrapped.
 */
void webauth_tick(void) {
  unsigned long now = now_ms();
  if((unsigned long)(now - sweep_time) < WEBAUTH_SWEEP_MS) {
    return;
  }
  sweep_time = now;
  for(int8_t i = 0; i < WEBAUTH_SESSIONS; i++) {
    if(sessions[i].used && !session_valid(&sessions[i], now)) {
      wipe(&sessions[i], sizeof(sessions[i]));
    }
  }
  for(uint8_t i = 0; i < WEBAUTH_LOCK_ENTRIES; i++) {
    lock_expire(&locks[i], now);
  }
  known_find(0, now);  // drops the expired ones
  seen_rotate(now);
  bucket_refill(now);
}

/* ------------------------------------------------------------------ */
/* The decision                                                        */
/* ------------------------------------------------------------------ */

#define FORM_FIELD 65
struct webauth_form_t {
  uint32_t ip;
  int8_t session;               // session of the request, -1: none (API key or login off)
  uint8_t result;
  uint8_t overflow;             // a field was longer than its buffer
  uint8_t counted;              // webauth_lock_check() counted this request as a try
  uint8_t token[WEBAUTH_TOKEN_BYTES];  // new session for the Set-Cookie header
  uint8_t new_session;
  uint8_t keep_apikey;          // password change: keep the API key (default: it is deleted)
  char action[16];
  char current[FORM_FIELD];
  char password[FORM_FIELD];
  char repeat[FORM_FIELD];
  char hosts[WEBAUTH_HOSTS_MAX];
  char apikey[2 * WEBAUTH_TOKEN_BYTES + 1];
};

static bool is_form_route(uint8_t route) {
  return route == WEBAUTH_ROUTE_LOGIN_POST || route == WEBAUTH_ROUTE_SECURITY_POST;
}

bool webauth_is_answer_route(uint8_t route) {
  return route == WEBAUTH_ROUTE_TO_LOGIN || route == WEBAUTH_ROUTE_UNAUTHORIZED ||
         route == WEBAUTH_ROUTE_FORBIDDEN || route == WEBAUTH_ROUTE_TOO_MANY || route == WEBAUTH_ROUTE_BAD_HOST;
}

/* what the 403 page of an unknown Host shows */
struct webauth_bad_host_t {
  char host[WEBSERVER_MAX_HOST + 1];
  char ip[16];
};

static int8_t reject(struct webserver_t *client, const struct webserver_request_t *req, uint8_t answer, unsigned long wait) {
  client->route = answer;
  client->userdata = (answer == WEBAUTH_ROUTE_TOO_MANY) ? (void *)(uintptr_t)((wait + 999) / 1000) : NULL;
  if(req->method == WEBSERVER_METHOD_POST && req->content_length > WEBAUTH_REJECT_DRAIN_MAX) {
    return -1;  // a firmware upload is not read to the end only to be thrown away
  }
  return WEBSERVER_REJECT;
}

/* 403 for a Host name this device does not know: a page that says which name to add */
static int8_t reject_host(struct webserver_t *client, const struct webserver_request_t *req, const char *host, const char *device_hosts) {
  int8_t r = reject(client, req, WEBAUTH_ROUTE_BAD_HOST, 0);
  if(r != WEBSERVER_REJECT) {
    return r;
  }
  struct webauth_bad_host_t *b = (struct webauth_bad_host_t *)malloc(sizeof(struct webauth_bad_host_t));
  if(b == NULL) {
    client->route = WEBAUTH_ROUTE_FORBIDDEN;
    return r;
  }
  memset(b, 0, sizeof(*b));
  // only the characters of a host name (the page shows it), the port is cut
  size_t n = 0;
  for(const char *p = host ? host : ""; *p && *p != ':' && n < sizeof(b->host) - 1; p++) {
    char c = *p;
    b->host[n++] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-') ? c : '?';
  }
  n = 0;
  for(const char *p = device_hosts ? device_hosts : ""; *p && *p != ',' && n < sizeof(b->ip) - 1; p++) {
    b->ip[n++] = *p;  // the first entry: the IP address (webauth_device_hosts: only digits and dots)
  }
  client->userdata = b;
  return r;
}

/*
 * Sec-Fetch-Site, when the browser sends it: only "same-origin" (a page of
 * this device) and "none" (typed address, bookmark) may change something.
 * "same-site" is refused too: evil.fritz.box is the same site as
 * heishamon.fritz.box. A cut value is refused. No header (programs, old
 * browsers): decided by Host and Origin only.
 */
static bool foreign_site(const struct webserver_request_t *req) {
  const char *sfs = req->header[WEBSERVER_HEADER_SEC_FETCH_SITE];
  if(sfs == NULL) {
    return false;
  }
  if(req->truncated & (1 << WEBSERVER_HEADER_SEC_FETCH_SITE)) {
    return true;
  }
  return strcasecmp_P(sfs, PSTR("same-origin")) != 0 && strcasecmp_P(sfs, PSTR("none")) != 0;
}

int8_t webauth_request(struct webserver_t *client, const struct webserver_request_t *req, const webauth_env_t *env) {
  webauth_tick();
  int16_t found = webauth_find_route(req);
  if(found < 0) {
    return -1;
  }
  uint8_t route = (uint8_t)found;
  uint8_t flags = webauth_route_flags(route);
  bool post = (req->method == WEBSERVER_METHOD_POST);
  client->route = route;
  client->userdata = NULL;

  // a cut value is never trusted
  const char *host = (req->truncated & (1 << WEBSERVER_HEADER_HOST)) ? NULL : req->header[WEBSERVER_HEADER_HOST];
  const char *origin = req->header[WEBSERVER_HEADER_ORIGIN];
  bool origin_ok = origin == NULL || (!(req->truncated & (1 << WEBSERVER_HEADER_ORIGIN)) && webauth_origin_matches(origin, host));
  bool host_ok = webauth_host_allowed(host, env->hosts);
  int8_t session = -1;

  // a page of another site (browsers send Sec-Fetch-Site): never changes anything, in both modes;
  // without it an <img src="http://<device>/reboot"> would reboot a device without a password
  if((flags & (WEBAUTH_STATE | WEBAUTH_WS)) && foreign_site(req)) {
    return reject(client, req, WEBAUTH_ROUTE_FORBIDDEN, 0);
  }

  if(!webauth_login_on()) {
    // no password: reading stays open as before; changing something and the live log need
    // a Host of this device (DNS rebinding) and, when the browser sends one, the same Origin
    if(flags & (WEBAUTH_STATE | WEBAUTH_WS)) {
      if(!host_ok) {
        return reject_host(client, req, host, env->hosts);
      }
      if(!origin_ok) {
        return reject(client, req, WEBAUTH_ROUTE_FORBIDDEN, 0);
      }
    }
  } else {
    if(!(flags & WEBAUTH_ANYHOST) && !host_ok) {
      return reject_host(client, req, host, env->hosts);
    }
    if(flags & WEBAUTH_PUBLIC) {
      if((flags & WEBAUTH_STATE) && !origin_ok) {
        return reject(client, req, WEBAUTH_ROUTE_FORBIDDEN, 0);
      }
      if(route == WEBAUTH_ROUTE_LOGIN_POST && !webauth_lock_check(env->ip)) {
        return reject(client, req, WEBAUTH_ROUTE_TOO_MANY, webauth_lock_wait_ms(env->ip));
      }
    } else {
      const char *authz = req->header[WEBSERVER_HEADER_AUTHORIZATION];
      if(authz != NULL && strncasecmp_P(authz, PSTR("Bearer "), 7) == 0) {
        // API key: no cookie, so no CSRF; the Origin is not checked
        const char *key = authz + 7;
        while(*key == ' ') {
          key++;
        }
        size_t klen = strlen(key);
        while(klen > 0 && key[klen - 1] == ' ') {
          klen--;
        }
        // the right key always works (Home Assistant may share its address with a guesser);
        // wrong keys count, and while the address backs off they get 429
        if((req->truncated & (1 << WEBSERVER_HEADER_AUTHORIZATION)) || !webauth_check_apikey(key, klen)) {
          unsigned long wait = ip_wait(env->ip, now_ms());
          if(wait > 0) {
            return reject(client, req, WEBAUTH_ROUTE_TOO_MANY, wait);
          }
          webauth_lock_failed(env->ip);
          return reject(client, req, WEBAUTH_ROUTE_UNAUTHORIZED, 0);
        }
      } else {
        const char *cookie = req->header[WEBSERVER_HEADER_COOKIE];
        if(cookie != NULL && !(req->truncated & (1 << WEBSERVER_HEADER_COOKIE))) {
          session = webauth_cookie_session(cookie);
        }
        if(session < 0) {
          return reject(client, req, ((flags & WEBAUTH_PAGE) && !post) ? WEBAUTH_ROUTE_TO_LOGIN : WEBAUTH_ROUTE_UNAUTHORIZED, 0);
        }
        if(flags & (WEBAUTH_STATE | WEBAUTH_WS)) {
          // CSRF: a cookie comes with every request to this device, also one a page of another site makes
          if(origin != NULL ? !origin_ok : (post || (flags & WEBAUTH_WS))) {
            return reject(client, req, WEBAUTH_ROUTE_FORBIDDEN, 0);
          }
        }
      }
      // logged in: no global budget; only this address's own wrong current passwords back it off
      if(route == WEBAUTH_ROUTE_SECURITY_POST) {
        unsigned long wait = ip_wait(env->ip, now_ms());
        if(wait > 0) {
          return reject(client, req, WEBAUTH_ROUTE_TOO_MANY, wait);
        }
      }
    }
  }

  // accepted
  if(session >= 0) {
    sessions[session].last = now_ms();
  }
  if(route == WEBAUTH_ROUTE_LOGOUT) {
    webauth_session_end(session);
  }
  if(is_form_route(route)) {
    struct webauth_form_t *f = (struct webauth_form_t *)malloc(sizeof(struct webauth_form_t));
    if(f == NULL) {
      return -1;
    }
    memset(f, 0, sizeof(*f));
    f->ip = env->ip;
    f->session = session;
    f->counted = (route == WEBAUTH_ROUTE_LOGIN_POST) && webauth_login_on();  // went through webauth_lock_check()
    client->userdata = f;
  }
  return WEBSERVER_ACCEPT;
}

/* ------------------------------------------------------------------ */
/* Forms                                                               */
/* ------------------------------------------------------------------ */

static void append(char *dst, size_t size, const unsigned char *src, uint16_t len, uint8_t *overflow) {
  size_t have = strlen(dst);
  if(have + len >= size) {
    *overflow = 1;
    return;
  }
  memcpy(dst + have, src, len);
  dst[have + len] = 0;
}

int webauth_args(struct webserver_t *client, struct arguments_t *args) {
  if(client->route == WEBAUTH_ROUTE_LOGIN_PAGE || client->route == WEBAUTH_ROUTE_SECURITY_PAGE) {
    if(args->value != NULL && strcmp_P((const char *)args->name, PSTR("m")) == 0 && args->len > 0 && args->len < 4) {
      char tmp[4];
      memcpy(tmp, args->value, args->len);
      tmp[args->len] = 0;
      client->userdata = (void *)(uintptr_t)(atoi(tmp) & 0xff);
    }
    return 0;
  }
  if(!is_form_route(client->route) || client->userdata == NULL || args->value == NULL) {
    return 0;
  }
  struct webauth_form_t *f = (struct webauth_form_t *)client->userdata;
  const char *name = (const char *)args->name;
  if(strcmp_P(name, PSTR("password")) == 0) {
    append(f->password, sizeof(f->password), args->value, args->len, &f->overflow);
  } else if(strcmp_P(name, PSTR("current")) == 0) {
    append(f->current, sizeof(f->current), args->value, args->len, &f->overflow);
  } else if(strcmp_P(name, PSTR("repeat")) == 0) {
    append(f->repeat, sizeof(f->repeat), args->value, args->len, &f->overflow);
  } else if(strcmp_P(name, PSTR("hosts")) == 0) {
    append(f->hosts, sizeof(f->hosts), args->value, args->len, &f->overflow);
  } else if(strcmp_P(name, PSTR("action")) == 0) {
    append(f->action, sizeof(f->action), args->value, args->len, &f->overflow);
  } else if(strcmp_P(name, PSTR("keep_apikey")) == 0) {
    // only an explicit yes keeps it ("0", "" or anything else: the key goes)
    f->keep_apikey = (args->len == 1 && args->value[0] == '1') || (args->len == 2 && strncasecmp_P((const char *)args->value, PSTR("on"), 2) == 0);
  }
  return 0;
}

static void new_session(struct webauth_form_t *f) {
  webauth_session_new(f->token);
  f->new_session = 1;
}

uint8_t webauth_process_form(struct webserver_t *client) {
  struct webauth_form_t *f = (struct webauth_form_t *)client->userdata;
  if(f == NULL || !is_form_route(client->route)) {
    return WEBAUTH_RESULT_NONE;
  }
  if(f->result != WEBAUTH_RESULT_NONE) {
    return f->result;
  }
  uint32_t ip = f->ip;
  uint8_t result = WEBAUTH_RESULT_INVALID;
  if(f->overflow) {
    result = WEBAUTH_RESULT_LENGTH;
  } else if(client->route == WEBAUTH_ROUTE_LOGIN_POST) {
    if(!webauth_login_on()) {
      result = WEBAUTH_RESULT_NO_PASSWORD;
    } else if(webauth_check_password(f->password)) {
      webauth_lock_success(ip);
      new_session(f);
      result = WEBAUTH_RESULT_LOGGED_IN;
    } else {
      result = WEBAUTH_RESULT_WRONG;  // counted by webauth_lock_check() already
    }
  } else {
    // /security: with the login on, every action needs the current password
    bool login = webauth_login_on();
    bool is_password = strcmp_P(f->action, PSTR("password")) == 0;
    bool is_remove = strcmp_P(f->action, PSTR("remove")) == 0;
    bool is_apikey = strcmp_P(f->action, PSTR("apikey")) == 0;
    bool is_apikey_delete = strcmp_P(f->action, PSTR("apikey_delete")) == 0;
    bool is_hosts = strcmp_P(f->action, PSTR("hosts")) == 0;
    size_t len = strlen(f->password);
    bool checked = false;  // the current password was right
    if(!(is_password || is_remove || is_apikey || is_apikey_delete || is_hosts)) {
      result = WEBAUTH_RESULT_INVALID;
    } else if(is_password && strcmp(f->password, f->repeat) != 0) {
      result = WEBAUTH_RESULT_MISMATCH;
    } else if(is_password && (len < WEBAUTH_PASSWORD_MIN || len > WEBAUTH_PASSWORD_MAX)) {
      result = WEBAUTH_RESULT_LENGTH;
    } else if(is_hosts && (strlen(f->hosts) >= sizeof(webauth_store.hosts) || !hosts_chars_ok(f->hosts))) {
      result = WEBAUTH_RESULT_INVALID;
    } else if(!login && (is_remove || is_apikey)) {
      result = WEBAUTH_RESULT_NO_PASSWORD;  // no key while everything is open: it would outlive the first password
    } else if(login && !(checked = webauth_check_password(f->current))) {
      webauth_lock_failed(ip);
      result = WEBAUTH_RESULT_WRONG;
    } else {
      if(checked) {
        webauth_lock_success(ip);
      }
      bool saved = true;
      if(is_password) {
        if(!login || !f->keep_apikey) {
          // first password: nothing set while everything was open stays; a change revokes the key by default
          webauth_store.apikey_set = 0;
          wipe(webauth_store.apikey_hash, sizeof(webauth_store.apikey_hash));
          if(!login) {
            wipe(webauth_store.hosts, sizeof(webauth_store.hosts));
          }
        }
        webauth_set_password(f->password);  // also ends all sessions and saves
        saved = webauth_last_save_ok();
        webauth_platform_close_others(client);
        new_session(f);
        result = WEBAUTH_RESULT_PASSWORD_SET;
      } else if(is_remove) {
        webauth_remove_password();  // also the API key and the extra host names
        saved = webauth_last_save_ok();
        webauth_platform_close_others(client);
        result = WEBAUTH_RESULT_REMOVED;
      } else if(is_apikey) {
        webauth_new_apikey(f->apikey);
        saved = webauth_last_save_ok();
        webauth_platform_close_others(client);
        result = WEBAUTH_RESULT_APIKEY;
      } else if(is_apikey_delete) {
        webauth_delete_apikey();
        saved = webauth_last_save_ok();
        webauth_platform_close_others(client);
        result = WEBAUTH_RESULT_APIKEY_DELETED;
      } else {
        webauth_set_hosts(f->hosts);
        saved = webauth_last_save_ok();
        result = WEBAUTH_RESULT_HOSTS;
      }
      if(!saved && result != WEBAUTH_RESULT_APIKEY) {
        result = WEBAUTH_RESULT_SAVE_FAILED;  // the key page says it itself
      }
    }
  }
  if(f->counted && result != WEBAUTH_RESULT_WRONG && result != WEBAUTH_RESULT_LOGGED_IN) {
    webauth_lock_refund(ip);  // no password was checked
  }
  wipe(f->current, sizeof(f->current));
  wipe(f->password, sizeof(f->password));
  wipe(f->repeat, sizeof(f->repeat));
  f->result = result;
  return result;
}

uint8_t webauth_form_result(struct webserver_t *client) {
  if(!is_form_route(client->route) || client->userdata == NULL) {
    return WEBAUTH_RESULT_NONE;
  }
  return ((struct webauth_form_t *)client->userdata)->result;
}

const char *webauth_form_apikey(struct webserver_t *client) {
  if(webauth_form_result(client) != WEBAUTH_RESULT_APIKEY) {
    return NULL;
  }
  return ((struct webauth_form_t *)client->userdata)->apikey;
}

uint8_t webauth_query_message(struct webserver_t *client) {
  if(client->route == WEBAUTH_ROUTE_LOGIN_PAGE || client->route == WEBAUTH_ROUTE_SECURITY_PAGE) {
    return (uint8_t)(uintptr_t)client->userdata;
  }
  return 0;
}

void webauth_close(struct webserver_t *client) {
  if(is_form_route(client->route) && client->userdata != NULL) {
    wipe(client->userdata, sizeof(struct webauth_form_t));
    free(client->userdata);
  }
  if(client->route == WEBAUTH_ROUTE_BAD_HOST && client->userdata != NULL) {
    free(client->userdata);
  }
  if(is_form_route(client->route) || webauth_is_answer_route(client->route) ||
     client->route == WEBAUTH_ROUTE_LOGIN_PAGE || client->route == WEBAUTH_ROUTE_SECURITY_PAGE) {
    client->userdata = NULL;
  }
}

/* ------------------------------------------------------------------ */
/* Answers                                                             */
/* ------------------------------------------------------------------ */

#define COOKIE_ATTRS "; Path=/; HttpOnly; SameSite=Strict"

int webauth_create_header(struct webserver_t *client, struct header_t *header) {
  char *b = (char *)header->buffer;
  const size_t room = 320;  // the webserver header buffer is 512 bytes, the status line is in front
  int n = 0;
  switch(client->route) {
    case WEBAUTH_ROUTE_TO_LOGIN: {
      n = snprintf_P(b, room, PSTR("Location: /login\r\nCache-Control: no-store\r\n"));
      header->ptr += n;
      return -1;
    }
    case WEBAUTH_ROUTE_UNAUTHORIZED: {
      n = snprintf_P(b, room, PSTR("WWW-Authenticate: Bearer realm=\"HeishaMon\"\r\nCache-Control: no-store\r\n"));
      header->ptr += n;
      return 0;
    }
    case WEBAUTH_ROUTE_FORBIDDEN:
    case WEBAUTH_ROUTE_BAD_HOST: {
      n = snprintf_P(b, room, PSTR("Cache-Control: no-store\r\n"));
      header->ptr += n;
      return 0;
    }
    case WEBAUTH_ROUTE_TOO_MANY: {
      unsigned int s = (unsigned int)(uintptr_t)client->userdata;
      n = snprintf_P(b, room, PSTR("Retry-After: %u\r\nCache-Control: no-store\r\n"), s == 0 ? 1 : s);
      header->ptr += n;
      return 0;
    }
    case WEBAUTH_ROUTE_LOGOUT: {
      n = snprintf_P(b, room, PSTR("Location: /login?m=%u\r\nSet-Cookie: " WEBAUTH_COOKIE_NAME "=" COOKIE_ATTRS "; Max-Age=0\r\nCache-Control: no-store\r\n"),
                     (unsigned int)WEBAUTH_RESULT_LOGGED_OUT);
      header->ptr += n;
      return -1;
    }
    case WEBAUTH_ROUTE_LOGIN_POST:
    case WEBAUTH_ROUTE_SECURITY_POST: {
      struct webauth_form_t *f = (struct webauth_form_t *)client->userdata;
      uint8_t result = f ? f->result : WEBAUTH_RESULT_INVALID;
      char cookie[2 * WEBAUTH_TOKEN_BYTES + 1];
      cookie[0] = 0;
      if(f && f->new_session) {
        to_hex(f->token, WEBAUTH_TOKEN_BYTES, cookie);
      }
      if(result == WEBAUTH_RESULT_APIKEY) {  // the page with the key (200), no redirect
        n = snprintf_P(b, room, PSTR("Cache-Control: no-store\r\n"));
        header->ptr += n;
        return 0;
      }
      if(client->route == WEBAUTH_ROUTE_LOGIN_POST) {
        if(result == WEBAUTH_RESULT_LOGGED_IN) {
          n = snprintf_P(b, room, PSTR("Location: /\r\n"));
        } else if(result == WEBAUTH_RESULT_NO_PASSWORD) {
          n = snprintf_P(b, room, PSTR("Location: /security\r\n"));
        } else {
          n = snprintf_P(b, room, PSTR("Location: /login?m=%u\r\n"), (unsigned int)result);
        }
      } else {
        n = snprintf_P(b, room, PSTR("Location: /security?m=%u\r\n"), (unsigned int)result);
      }
      if(cookie[0]) {
        n += snprintf_P(b + n, room - n, PSTR("Set-Cookie: " WEBAUTH_COOKIE_NAME "=%s" COOKIE_ATTRS "\r\n"), cookie);
      } else if(result == WEBAUTH_RESULT_REMOVED) {
        n += snprintf_P(b + n, room - n, PSTR("Set-Cookie: " WEBAUTH_COOKIE_NAME "=" COOKIE_ATTRS "; Max-Age=0\r\n"));
      }
      n += snprintf_P(b + n, room - n, PSTR("Cache-Control: no-store\r\n"));
      wipe(cookie, sizeof(cookie));
      header->ptr += n;
      return -1;
    }
  }
  return 0;
}

/*
 * Write phase of the answer routes, of POST /login, /logout and of
 * POST /security. Returns 1 for POST /security with a new API key: the
 * caller shows the key (once) on a page and then calls webauth_close().
 */
int webauth_write_answer(struct webserver_t *client) {
  if(client->content != 0) {
    return 0;
  }
  switch(client->route) {
    case WEBAUTH_ROUTE_UNAUTHORIZED: {
      static const char msg[] PROGMEM = "401 Login required: log in at /login, or send the API key as \"Authorization: Bearer <key>\"\n";
      webserver_send(client, 401, (char *)"text/plain", 0);
      webserver_send_content_P(client, msg, strlen_P(msg));
    } break;
    case WEBAUTH_ROUTE_BAD_HOST: {
      struct webauth_bad_host_t *bh = (struct webauth_bad_host_t *)client->userdata;
      static const char p1[] PROGMEM = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
                                       "<title>HeishaMon: unknown host name</title></head><body style='font-family:sans-serif;max-width:640px;margin:2em auto'>"
                                       "<h2>Unknown host name</h2><p>This HeishaMon does not answer to the name <b>";
      static const char p2[] PROGMEM = "</b>. It only accepts its IP addresses, its hostname and hostname.local, to protect it from other web sites (DNS rebinding).</p>"
                                       "<p>If this is the name your router or reverse proxy gives it (for example <i>heishamon.fritz.box</i>), open ";
      static const char p3[] PROGMEM = " and add the name under <i>Host names</i> in Settings &rarr; System &rarr; Access &amp; security (/security).</p></body></html>\n";
      webserver_send(client, 403, (char *)"text/html", 0);
      webserver_send_content_P(client, p1, strlen_P(p1));
      if(bh != NULL) {
        webserver_send_content(client, bh->host, strlen(bh->host));  // only host name characters (reject_host)
      }
      webserver_send_content_P(client, p2, strlen_P(p2));
      if(bh != NULL && bh->ip[0]) {
        char link[96];
        snprintf_P(link, sizeof(link), PSTR("<a href='http://%s/security'>http://%s/security</a>"), bh->ip, bh->ip);
        webserver_send_content(client, link, strlen(link));
      } else {
        webserver_send_content_P(client, PSTR("HeishaMon by its IP address"), 27);
      }
      webserver_send_content_P(client, p3, strlen_P(p3));
      if(bh != NULL) {
        free(bh);
      }
      client->userdata = NULL;  // the webserver drops it after the answer
    } break;
    case WEBAUTH_ROUTE_FORBIDDEN: {
      static const char msg[] PROGMEM = "403 Forbidden: a request of another site (Origin or Sec-Fetch-Site)\n";
      webserver_send(client, 403, (char *)"text/plain", 0);
      webserver_send_content_P(client, msg, strlen_P(msg));
    } break;
    case WEBAUTH_ROUTE_TOO_MANY: {
      static const char msg[] PROGMEM = "429 Too many failed attempts, try again later\n";
      webserver_send(client, 429, (char *)"text/plain", 0);
      webserver_send_content_P(client, msg, strlen_P(msg));
    } break;
    case WEBAUTH_ROUTE_TO_LOGIN:
    case WEBAUTH_ROUTE_LOGOUT: {
      webserver_send(client, 302, (char *)"text/html", 0);
    } break;
    case WEBAUTH_ROUTE_LOGIN_POST:
    case WEBAUTH_ROUTE_SECURITY_POST: {
      if(webauth_process_form(client) == WEBAUTH_RESULT_APIKEY) {
        return 1;  // the caller shows the key, then calls webauth_close()
      }
      webserver_send(client, 302, (char *)"text/html", 0);  // the Set-Cookie header is made here
      webauth_close(client);  // the webserver drops userdata after the answer, before WEBSERVER_CLIENT_CLOSE
    } break;
  }
  return 0;
}
