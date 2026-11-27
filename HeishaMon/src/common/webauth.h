/*
  Optional login for the web interface and the HTTP API.

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Everything that decides about a request lives here, so the host tests
  (tests/webauth) run exactly this code:

  - webauth_routes[]: the one table of every route number webserver_cb()
    uses, with its policy (public or login, state changing, HTML page or
    API). A route that is not in the table is treated as "login, state
    changing" (deny by default).
  - webauth_request(): called by webserver_cb() for WEBSERVER_CLIENT_REQUEST.
    Finds the route of the path, checks Host, Origin, the session cookie
    or the API key, and sets client->route to the route or to an answer
    route (login redirect, 401, 403, 429).
  - the login, logout and /security form handling, the sessions, the
    per-address lockout, the password hash and the API key.

  The login is on as soon as a web password is set. Without a password
  everything works as before, except that state changing requests must
  come with an allowed Host and, when the browser sends one, a matching
  Origin.
*/
#ifndef _WEBAUTH_H_
#define _WEBAUTH_H_

#include <stdint.h>
#include <stddef.h>
#include "webserver.h"

/* ---- limits ---- */
#define WEBAUTH_SESSIONS 4                    // logged in browsers at the same time
#define WEBAUTH_SESSION_IDLE_MS (30UL * 60UL * 1000UL)        // a session ends after 30 min without a request
#define WEBAUTH_SESSION_MAX_MS (12UL * 60UL * 60UL * 1000UL)  // and at the latest 12 h after the login
#define WEBAUTH_TOKEN_BYTES 16                // 128 bit session token and API key
#define WEBAUTH_SALT_BYTES 16
#define WEBAUTH_HASH_BYTES 32
#define WEBAUTH_ITERATIONS 2000               // PBKDF2-HMAC-SHA256 iterations of a new password hash
#define WEBAUTH_PASSWORD_MIN 8
#define WEBAUTH_PASSWORD_MAX 64
#define WEBAUTH_HOSTS_MAX 96                  // extra allowed host names (reverse proxy), comma separated
#define WEBAUTH_LOCK_ENTRIES 16               // addresses with failed attempts that are remembered (12 bytes each)
#define WEBAUTH_FAILED_BITS 2048              // Bloom filter of all addresses that failed lately (256 bytes, two generations: 512)
#define WEBAUTH_LOCK_FREE 3                   // failed attempts of one address before the backoff starts
#define WEBAUTH_LOCK_MAX_MS (5UL * 60UL * 1000UL)     // longest backoff of one address
#define WEBAUTH_LOCK_FORGET_MS (15UL * 60UL * 1000UL) // failures of an address are forgotten after this
#define WEBAUTH_BUCKET_SIZE 10                // password checks in a burst, all addresses together
#define WEBAUTH_BUCKET_REFILL_MS 3000UL       // one more password check every 3 s
#define WEBAUTH_KNOWN_ENTRIES 4               // addresses that logged in lately: only their own backoff, never the global budget
#define WEBAUTH_KNOWN_MS (7UL * 24UL * 60UL * 60UL * 1000UL)
#define WEBAUTH_BUCKET_RESERVE 4              // the last ones only for addresses without recent failures
#define WEBAUTH_ITERATIONS_MIN 1000           // a stored hash outside these limits is not valid
#define WEBAUTH_ITERATIONS_MAX 100000
#define WEBAUTH_SWEEP_MS 60000UL              // expired sessions and lockouts are dropped at least this often
#define WEBAUTH_HOSTNAME_MAX 32
#define WEBAUTH_REJECT_DRAIN_MAX 4096         // a rejected POST with a longer body is closed, not drained

#define WEBAUTH_COOKIE_NAME "hmsession"

/* ---- route policy ---- */
#define WEBAUTH_PUBLIC   0x01  // no login needed, also when the login is on
#define WEBAUTH_STATE    0x02  // changes something: Host allowlist and Origin check (CSRF)
#define WEBAUTH_PAGE     0x04  // an HTML page: without login a GET is sent to /login (else 401)
#define WEBAUTH_ANYHOST  0x08  // captive portal check of a phone on the hotspot: any Host header
#define WEBAUTH_WS       0x10  // the websocket: Origin must be sent and match for a cookie login
#define WEBAUTH_INTERNAL 0x20  // only set by the code, no path leads to it

/* Route numbers of the login itself and of the answers of a rejected request */
#define WEBAUTH_ROUTE_LOGIN_PAGE 4
#define WEBAUTH_ROUTE_LOGIN_POST 5
#define WEBAUTH_ROUTE_LOGOUT 6
#define WEBAUTH_ROUTE_SECURITY_PAGE 7
#define WEBAUTH_ROUTE_SECURITY_POST 8
#define WEBAUTH_ROUTE_TO_LOGIN 220     // 302 to /login
#define WEBAUTH_ROUTE_UNAUTHORIZED 221 // 401
#define WEBAUTH_ROUTE_FORBIDDEN 222    // 403 (Host or Origin)
#define WEBAUTH_ROUTE_TOO_MANY 223     // 429 (lockout)
#define WEBAUTH_ROUTE_BAD_HOST 224     // 403 with a page: which host name to add

typedef struct webauth_route_t {
  uint8_t route;
  uint8_t flags;
  uint8_t method;      // 0: GET and POST, 1: only GET, 2: only POST
  const char *path;    // NULL: only set by the code
} webauth_route_t;

extern const webauth_route_t webauth_routes[];
extern const uint8_t webauth_routes_count;

uint8_t webauth_route_flags(uint8_t route);   // policy of a route, unknown: login + state changing
int16_t webauth_find_route(const struct webserver_request_t *req);  // -1: close (unknown POST)

/* ---- stored credentials (the device keeps them in /webauth.bin) ---- */
/*
 * Format of /webauth.bin. Every layout change gets a new version and a
 * migration in webauth_load(); a file of a newer, unknown version keeps the
 * login on (no password matches) instead of switching it off.
 *   version 1: magic "HAW1", no version field (alpha builds of the web login)
 *   version 2: magic "HAWS" + version byte (this layout)
 */
#define WEBAUTH_STORE_MAGIC 0x53574148UL    /* "HAWS" in the file */
#define WEBAUTH_STORE_VERSION 2
#define WEBAUTH_STORE_MAGIC_V1 0x48415731UL /* "HAW1" */
#define WEBAUTH_STORE_MAX_SIZE 256          // the largest file of any known version
typedef struct webauth_store_t {
  uint32_t magic;
  uint8_t version;
  uint8_t password_set;
  uint8_t apikey_set;
  uint8_t reserved;
  uint32_t iterations;
  uint8_t check[8];                           // first bytes of the SHA-256 of everything else (bit rot, cut writes)
  uint8_t salt[WEBAUTH_SALT_BYTES];
  uint8_t password_hash[WEBAUTH_HASH_BYTES];  // PBKDF2-HMAC-SHA256(password, salt, iterations)
  uint8_t apikey_hash[WEBAUTH_HASH_BYTES];    // SHA-256 of the key as sent (the key itself is never stored)
  char hosts[WEBAUTH_HOSTS_MAX];              // extra allowed Host names
} webauth_store_t;

extern webauth_store_t webauth_store;

void webauth_reset(void);            // forget everything (RAM): no password, no sessions, no lockouts
enum {
  WEBAUTH_LOAD_INVALID = 0,   // damaged: call webauth_load_failed()
  WEBAUTH_LOAD_OK,
  WEBAUTH_LOAD_MIGRATED,      // an older version, taken over: save it again in the current format
  WEBAUTH_LOAD_NEWER,         // made by a newer firmware: call webauth_load_failed() (login stays on)
};
uint8_t webauth_load(const void *data, size_t len);
bool webauth_save(void);             // write the current credentials (after a migration)
void webauth_load_failed(void);      // the stored file exists but is not valid: login on, no password matches (until a factory reset)
bool webauth_broken(void);           // webauth_load_failed() was called
bool webauth_login_on(void);         // a web password is set (or the stored one is damaged)
void webauth_tick(void);             // call from loop(): drops expired sessions and lockouts (millis() wrap)
bool webauth_ota_allowed(const char *ota_password); // ArduinoOTA may run: login off and not the default password
bool webauth_hostname_valid(const char *name);       // [A-Za-z0-9-], 1..32, no '-' at either end
void webauth_device_hosts(char *out, size_t size, const char *const *ips, uint8_t nips, const char *hostname);
size_t webauth_html_escape(const char *in, char *out, size_t size); // returns the length written

/* Implemented by the firmware (and the tests) */
bool webauth_platform_save(const webauth_store_t *store);   // write /webauth.bin, false: failed
void webauth_platform_random(uint8_t *buf, size_t len);     // hardware random numbers
void webauth_platform_sha256(const uint8_t *a, size_t alen, const uint8_t *b, size_t blen, uint8_t out[32]);
void webauth_platform_yield(void);
unsigned long webauth_platform_millis(void);
void webauth_platform_close_others(struct webserver_t *keep); // stop every other web client

/* ---- the decision ---- */
typedef struct webauth_env_t {
  uint32_t ip;          // address of the client (for the lockout only)
  const char *hosts;    // the Host names of this device: its IP addresses, hostname, hostname.local (comma separated)
} webauth_env_t;

int8_t webauth_request(struct webserver_t *client, const struct webserver_request_t *req, const webauth_env_t *env);

/* Answer routes and the login routes in the write phase */
bool webauth_is_answer_route(uint8_t route);
int webauth_write_answer(struct webserver_t *client);
int webauth_create_header(struct webserver_t *client, struct header_t *header);
int webauth_args(struct webserver_t *client, struct arguments_t *args);
void webauth_close(struct webserver_t *client);

/* Result of the forms, for the page that answers (webauth_form_result) */
enum {
  WEBAUTH_RESULT_NONE = 0,
  WEBAUTH_RESULT_LOGGED_IN,
  WEBAUTH_RESULT_WRONG,         // wrong (current) password
  WEBAUTH_RESULT_LOCKED,        // too many attempts, wait
  WEBAUTH_RESULT_NO_PASSWORD,   // login while no password is set
  WEBAUTH_RESULT_PASSWORD_SET,  // first password or new password stored
  WEBAUTH_RESULT_MISMATCH,      // new password and repeat differ
  WEBAUTH_RESULT_LENGTH,        // new password too short or too long
  WEBAUTH_RESULT_REMOVED,       // password removed, login off
  WEBAUTH_RESULT_APIKEY,        // new API key made (shown once)
  WEBAUTH_RESULT_APIKEY_DELETED,
  WEBAUTH_RESULT_HOSTS,         // extra host names stored
  WEBAUTH_RESULT_INVALID,       // invalid input
  WEBAUTH_RESULT_LOGGED_OUT,
  WEBAUTH_RESULT_SAVE_FAILED,   // changed, but could not be written to the flash (lost on restart)
};

/* POST /login and POST /security: run once in the write phase (content 0) */
uint8_t webauth_process_form(struct webserver_t *client);
uint8_t webauth_form_result(struct webserver_t *client);
const char *webauth_form_apikey(struct webserver_t *client); // the new key after WEBAUTH_RESULT_APIKEY, else NULL
uint8_t webauth_query_message(struct webserver_t *client);    // ?m=<n> of GET /login and GET /security
unsigned long webauth_lock_wait_ms(uint32_t ip);               // remaining backoff of an address
bool webauth_lock_clean(uint32_t ip);            // no failure of this address is remembered (it may use the reserve of the budget)

/* ---- building blocks (used by the code above, public for the tests) ---- */
void webauth_pbkdf2_sha256(const char *password, size_t plen, const uint8_t *salt, size_t slen, uint32_t iterations, uint8_t out[32]);
bool webauth_equal(const uint8_t *a, const uint8_t *b, size_t len);      // constant time
bool webauth_set_password(const char *password);  // false: invalid length (true even when the save failed, see webauth_last_save_ok)
bool webauth_last_save_ok(void);
bool webauth_check_password(const char *password);
void webauth_remove_password(void);
void webauth_new_apikey(char out[2 * WEBAUTH_TOKEN_BYTES + 1]);
void webauth_delete_apikey(void);
bool webauth_check_apikey(const char *key, size_t len);
bool webauth_set_hosts(const char *hosts);

int8_t webauth_session_new(uint8_t token[WEBAUTH_TOKEN_BYTES]);           // index, evicts the oldest
int8_t webauth_session_find(const uint8_t token[WEBAUTH_TOKEN_BYTES]);    // index of a valid session or -1
void webauth_session_end(int8_t index);
void webauth_sessions_clear(void);
int8_t webauth_cookie_session(const char *cookie);  // valid session of the Cookie header or -1

bool webauth_host_allowed(const char *host, const char *device_hosts);
bool webauth_origin_matches(const char *origin, const char *host);

bool webauth_lock_check(uint32_t ip);     // 1: this address may try a password now (no backoff, budget left); takes one try and counts it as a failure until webauth_lock_success()
void webauth_lock_failed(uint32_t ip);
void webauth_lock_success(uint32_t ip);
void webauth_lock_refund(uint32_t ip);  // a counted try that checked no password

#endif
