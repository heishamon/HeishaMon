/*
  Host test of HeishaMon/src/common/webserver.cpp.

  The real webserver is compiled with g++ (see run_tests.py) and driven
  through webserver_loop() with a fake client that hands it a raw HTTP
  request in pieces. The test callback records every event it gets; the
  tests assert the events, their order and the answer that was sent.
  Every request is also fed split in two at every offset and one byte at
  a time, which must not change anything.
*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "webserver.h"

static unsigned long now_ms = 1000;
unsigned long millis(void) { return ++now_ms; }
void log_message(char *) {}

/* ---- fake client ---- */

static std::vector<std::string> pieces;
static size_t piece = 0;
static std::string output;

static int f_write(const unsigned char *b, int n) { output.append((const char *)b, n); return n; }
static int f_write_P(const char *b, int n) { output.append(b, n); return n; }
static int f_available() { return piece < pieces.size(); }
static int f_connected() { return 1; }
static void f_stop() {}
static size_t piece_at = 0;  // bytes of the current piece already read
static int f_read(uint8_t *b, int n) {
  std::string &p = pieces[piece];
  int l = (int)(p.size() - piece_at) < n ? (int)(p.size() - piece_at) : n;
  memcpy(b, p.data() + piece_at, l);
  piece_at += l;
  if(piece_at >= p.size()) {
    piece++;
    piece_at = 0;
  }
  return l;
}
static WiFiClient fake = { f_write, f_write_P, f_available, f_connected, f_stop, f_read };

/* ---- test callback ---- */

enum { DECIDE_ACCEPT, DECIDE_REJECT, DECIDE_CLOSE };
static int decide = DECIDE_ACCEPT;
static std::string veto;  // an argument with this name returns -1
static std::string events;
static std::string cookie;  // Cookie header as the callback saw it
static uint8_t was_websocket = 0;

static std::string shorten(const unsigned char *v, uint16_t len) {
  if(len <= 40) {
    return std::string((const char *)v, len);
  }
  uint32_t sum = 0;
  for(uint16_t i = 0; i < len; i++) {
    sum = sum * 31 + v[i];
  }
  char tmp[48];
  snprintf(tmp, sizeof(tmp), "<%u bytes %08x>", len, sum);
  return tmp;
}

static int8_t callback(struct webserver_t *client, void *data);
static webserver_cb_t *current_cb = callback;

static int8_t callback(struct webserver_t *client, void *data) {
  switch(client->step) {
    case WEBSERVER_CLIENT_REQUEST: {
      struct webserver_request_t *r = (struct webserver_request_t *)data;
      char tmp[256];
      snprintf(tmp, sizeof(tmp), "REQ %s %s q=%s ws=%d cl=%u t=%d", r->method == WEBSERVER_METHOD_POST ? "POST" : "GET",
               r->path, r->query, r->websocket, (unsigned)r->content_length, r->truncated);
      events += tmp;
      static const char *names[WEBSERVER_HEADER_COUNT] = { "host", "origin", "cookie", "auth", "ctype" };
      for(int i = 0; i < WEBSERVER_HEADER_COUNT; i++) {
        if(r->header[i] != NULL && i != WEBSERVER_HEADER_COOKIE) {
          events += std::string(" ") + names[i] + "=" + r->header[i];
        }
      }
      cookie = r->header[WEBSERVER_HEADER_COOKIE] ? r->header[WEBSERVER_HEADER_COOKIE] : "(none)";
      events += ";";
      if(client->is_websocket) {
        events += "BUG:websocket-before-decision;";
      }
      if(decide == DECIDE_CLOSE) {
        return -1;
      }
      if(decide == DECIDE_REJECT) {
        client->route = 2;
        return WEBSERVER_REJECT;
      }
      client->route = 1;
      return WEBSERVER_ACCEPT;
    }
    case WEBSERVER_CLIENT_ARGS: {
      struct arguments_t *a = (struct arguments_t *)data;
      events += "ARG " + std::string((const char *)a->name) + "=";
      if(a->value == NULL) {
        events += "(null)";
      } else {
        events += shorten(a->value, a->len);
        if(a->value[a->len] != 0) {
          events += "(not terminated)";
        }
      }
      events += ";";
      if(veto.size() > 0 && veto == (const char *)a->name) {
        return -1;
      }
      return 0;
    }
    case WEBSERVER_CLIENT_WRITE: {
      if(client->content == 0) {
        events += "WRITE;";
        if(client->route == 2) {
          webserver_send(client, 401, (char *)"text/plain", 12);
          webserver_send_content(client, (char *)"unauthorized", 12);
        } else {
          webserver_send(client, 200, (char *)"text/plain", 2);
          webserver_send_content(client, (char *)"ok", 2);
        }
      }
      return 0;
    }
    case WEBSERVER_CLIENT_WEBSOCKET_TEXT: {
      events += "WS " + std::string((const char *)data) + ";";
      return 0;
    }
    case WEBSERVER_CLIENT_CLOSE: {
      events += "CLOSE;";
      return 0;
    }
  }
  return 0;
}

/* ---- running a request ---- */

struct result_t {
  std::string events;
  std::string output;
  std::string cookie;
  uint8_t websocket;
};

static result_t run(const std::vector<std::string> &p) {
  pieces = p;
  piece = 0;
  piece_at = 0;
  output.clear();
  events.clear();
  cookie.clear();
  was_websocket = 0;
  struct webserver_t *c = &clients[0].data;
  webserver_reset_client(c);
  c->client = &fake;
  c->callback = current_cb;
  c->step = WEBSERVER_CLIENT_CONNECTING;
  c->lastseen = millis();
  int idle = 0;
  for(int i = 0; i < 100000 && c->step != 0 && idle < 20; i++) {
    webserver_loop();
    if(c->is_websocket) {
      was_websocket = 1;
    }
    if(piece >= pieces.size()) {
      idle++;
    }
  }
  result_t r = { events, output, cookie, was_websocket };
  return r;
}

static result_t run(const std::string &req) { return run(std::vector<std::string>{ req }); }

/* ---- checks ---- */

static int checks = 0, failed = 0;

#define CHECK(cond, ...) do { \
  checks++; \
  if(!(cond)) { \
    failed++; \
    printf("FAIL %s:%d: %s\n  ", __FILE__, __LINE__, #cond); \
    printf(__VA_ARGS__); \
    printf("\n"); \
  } \
} while(0)

static std::string replaced(std::string s, const std::string &from, const std::string &to) {
  size_t at = s.find(from);
  if(at != std::string::npos) {
    s.replace(at, from.size(), to);
  }
  return s;
}

static bool contains(const std::string &s, const std::string &part) { return s.find(part) != std::string::npos; }

/*
 * Feed the request whole, split in two at every offset and one byte at a
 * time: the events and the answer must be the same every time.
 */
static result_t run_split(const std::string &name, const std::string &req) {
  result_t whole = run(req);
  int bad = 0;
  for(size_t s = 1; s < req.size(); s++) {
    result_t r = run(std::vector<std::string>{ req.substr(0, s), req.substr(s) });
    if(r.events != whole.events || r.output != whole.output) {
      if(bad++ == 0) {
        printf("  %s split at %zu:\n    %s\n  whole:\n    %s\n", name.c_str(), s, r.events.c_str(), whole.events.c_str());
      }
    }
  }
  std::vector<std::string> bytes;
  for(char ch : req) {
    bytes.push_back(std::string(1, ch));
  }
  result_t r = run(bytes);
  if(r.events != whole.events || r.output != whole.output) {
    bad++;
    printf("  %s byte by byte:\n    %s\n", name.c_str(), r.events.c_str());
  }
  CHECK(bad == 0, "%s: %d splits differ", name.c_str(), bad);
  return whole;
}

static std::string with_length(const std::string &head, const std::string &body) {
  return head + "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

static const std::string B = "----WebKitFormBoundaryX3vY7qP0aBcD";

static std::string part(const std::string &name, const std::string &value, const std::string &extra = "") {
  return "--" + B + "\r\nContent-Disposition: form-data; name=\"" + name + "\"" + extra + "\r\n\r\n" + value + "\r\n";
}

static std::string multipart(const std::string &path, const std::string &parts, const std::string &headers = "") {
  return with_length("POST " + path + " HTTP/1.1\r\nHost: heishamon.local\r\n" + headers +
                     "Content-Type: multipart/form-data; boundary=" + B + "\r\n", parts + "--" + B + "--\r\n");
}

static std::string ws_frame(const std::string &text) {
  std::string f;
  const unsigned char mask[4] = { 0x12, 0x34, 0x56, 0x78 };
  f += (char)0x81;
  f += (char)(0x80 | text.size());
  for(int i = 0; i < 4; i++) {
    f += (char)mask[i];
  }
  for(size_t i = 0; i < text.size(); i++) {
    f += (char)(text[i] ^ mask[i % 4]);
  }
  return f;
}

static const std::string WS_HEADERS =
  "Host: heishamon.local\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n"
  "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n";

int main() {
  webserver_start(80, callback, 0);
  printf("webserver host test, WEBSERVER_BUFFER_SIZE %d\n", WEBSERVER_BUFFER_SIZE);

  /* --- the GET requests of the web page and the HTTP API --- */
  {
    decide = DECIDE_ACCEPT;
    result_t r = run_split("dallasalias", "GET /dallasalias?28-0123456789ab=Living%20room HTTP/1.1\r\nHost: 192.168.4.1\r\n"
                           "User-Agent: Mozilla/5.0\r\nAccept: */*\r\n\r\n");
    CHECK(r.events == "REQ GET /dallasalias q=28-0123456789ab=Living%20room ws=0 cl=0 t=0 host=192.168.4.1;"
                      "ARG 28-0123456789ab=Living room;WRITE;CLOSE;", "%s", r.events.c_str());
    CHECK(r.output.compare(0, 15, "HTTP/1.1 200 OK") == 0, "%s", r.output.c_str());
    // no CORS header (the test callback adds none, the webserver must not either)
    CHECK(!contains(r.output, "Access-Control"), "%s", r.output.c_str());

    r = run_split("gpio", "GET /gpio?pin=5&value=1 HTTP/1.1\r\nHost: x\r\n\r\n");
    CHECK(r.events == "REQ GET /gpio q=pin=5&value=1 ws=0 cl=0 t=0 host=x;ARG pin=5;ARG value=1;WRITE;CLOSE;", "%s", r.events.c_str());

    r = run_split("command", "GET /command?SetZ1HeatRequestTemperature=45&SetCurves=%7B%22a%22%3A1%7D&x+y=a+b HTTP/1.1\r\nHost: x\r\n\r\n");
    CHECK(r.events == "REQ GET /command q=SetZ1HeatRequestTemperature=45&SetCurves=%7B%22a%22%3A1%7D&x+y=a+b ws=0 cl=0 t=0 host=x;"
                      "ARG SetZ1HeatRequestTemperature=45;ARG SetCurves={\"a\":1};ARG x y=a b;WRITE;CLOSE;", "%s", r.events.c_str());

    r = run_split("flags", "GET /command?flag&empty=&&last=1 HTTP/1.1\r\n\r\n");
    CHECK(r.events == "REQ GET /command q=flag&empty=&&last=1 ws=0 cl=0 t=0;ARG flag=(null);ARG empty=;ARG last=1;WRITE;CLOSE;", "%s", r.events.c_str());

    r = run_split("json", "GET /json HTTP/1.1\r\nhost: x\r\n\r\n");
    CHECK(r.events == "REQ GET /json q= ws=0 cl=0 t=0 host=x;WRITE;CLOSE;", "%s", r.events.c_str());

    // bare LF line ends are accepted too
    r = run_split("lf", "GET /json?topic=1 HTTP/1.1\nHost: x\n\n");
    CHECK(r.events == "REQ GET /json q=topic=1 ws=0 cl=0 t=0 host=x;ARG topic=1;WRITE;CLOSE;", "%s", r.events.c_str());
  }

  /* --- the headers kept for the callback --- */
  {
    result_t r = run_split("headers", "GET / HTTP/1.1\r\nHOST:  heishamon.local \r\nOrigin: http://heishamon.local\r\n"
                           "Cookie: session=abc; theme=dark\r\nAuthorization: Basic YWRtaW46c2VjcmV0\r\nX-Other: 1\r\n\r\n");
    CHECK(r.events == "REQ GET / q= ws=0 cl=0 t=0 host=heishamon.local origin=http://heishamon.local auth=Basic YWRtaW46c2VjcmV0;WRITE;CLOSE;",
          "%s", r.events.c_str());
    CHECK(r.cookie == "session=abc; theme=dark", "%s", r.cookie.c_str());

    // only the first of a repeated header counts
    r = run("GET / HTTP/1.1\r\nCookie: a=1\r\nCookie: b=2\r\n\r\n");
    CHECK(r.cookie == "a=1", "%s", r.cookie.c_str());

    // a header name that only starts like a kept one is not that header
    r = run("GET / HTTP/1.1\r\nCookies: a=1\r\nCookie-Extra: b\r\nHost-X: y\r\n\r\n");
    CHECK(r.cookie == "(none)" && contains(r.events, "REQ GET / q= ws=0 cl=0 t=0;"), "%s / %s", r.events.c_str(), r.cookie.c_str());
  }

  /* --- long headers: cut and marked, never an overflow --- */
  {
    std::string big(3000, 'c');
    result_t r = run_split("long cookie", "GET / HTTP/1.1\r\nHost: x\r\nCookie: s=" + big + "\r\nUser-Agent: " + std::string(1000, 'u') + "\r\n\r\n");
    CHECK(contains(r.events, "t=4 host=x;"), "%s", r.events.c_str());
    CHECK(r.cookie == ("s=" + big).substr(0, WEBSERVER_MAX_COOKIE), "cookie of %zu bytes", r.cookie.size());
    CHECK(contains(r.events, "WRITE;CLOSE;"), "%s", r.events.c_str());

    std::string longauth(500, 'a');
    r = run("GET / HTTP/1.1\r\nAuthorization: Bearer " + longauth + "\r\nOrigin: http://" + std::string(400, 'o') + "\r\n\r\n");
    CHECK(contains(r.events, "t=10 "), "%s", r.events.c_str());

    // a long header name, longer than the webserver buffer
    r = run("GET / HTTP/1.1\r\nX-" + std::string(WEBSERVER_BUFFER_SIZE * 2, 'n') + ": v\r\nHost: x\r\n\r\n");
    CHECK(contains(r.events, "REQ GET / q= ws=0 cl=0 t=0 host=x;WRITE;CLOSE;"), "%s", r.events.c_str());

    // more than WEBSERVER_MAX_HEADER_SIZE of headers: closed, the callback sees no request
    std::string many;
    for(int i = 0; i < 100; i++) {
      many += "X-Filler-" + std::to_string(i) + ": " + std::string(60, 'f') + "\r\n";
    }
    r = run("GET /command?SetHeatpump=1 HTTP/1.1\r\n" + many + "\r\n");
    CHECK(r.events == "CLOSE;" && r.output.empty(), "%s", r.events.c_str());

    // a path or a query that is too long is never cut (a cut command could be applied): closed
    r = run("GET /" + std::string(WEBSERVER_MAX_PATH + 1, 'p') + " HTTP/1.1\r\n\r\n");
    CHECK(r.events == "CLOSE;", "%s", r.events.c_str());
    r = run("GET /command?SetZ1HeatRequestTemperature=4" + std::string(WEBSERVER_MAX_QUERY, '5') + " HTTP/1.1\r\n\r\n");
    CHECK(r.events == "CLOSE;", "%s", r.events.c_str());
    // the longest allowed path and query still work
    r = run("GET /" + std::string(WEBSERVER_MAX_PATH - 1, 'p') + "?" + std::string(WEBSERVER_MAX_QUERY - 2, 'q') + "=1 HTTP/1.1\r\n\r\n");
    CHECK(contains(r.events, "=1;WRITE;CLOSE;"), "%s", r.events.c_str());

    // a multipart Content-Type that is cut has lost its boundary: closed, no body
    r = run(replaced(multipart("/firmware", part("md5", "x")), "boundary=" + B, "boundary=" + std::string(WEBSERVER_MAX_CONTENT_TYPE, 'b')));
    CHECK(contains(r.events, " t=16 ") && !contains(r.events, "ARG") && !contains(r.events, "WRITE"), "%s", r.events.c_str());
  }

  /* --- invalid requests --- */
  {
    const char *bad[] = {
      "HEAD / HTTP/1.1\r\n\r\n",
      "OPTIONS / HTTP/1.1\r\n\r\n",
      "\x16\x03\x01\x02\x00\x01\x00\x01\xfc\x03\x03",
      "GET  HTTP/1.1\r\n\r\n",
      "GET / FTP/1.0\r\n\r\n",
      "GET / HTTP/1.1\r\nNoColon\r\n\r\n",
      "POST /saverules HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nrules",
      "POST /saverules HTTP/1.1\r\nContent-Length: 5x\r\n\r\nrules",
      "POST /saverules HTTP/1.1\r\nContent-Length: 99999999999\r\n\r\nrules",
    };
    for(const char *b : bad) {
      result_t r = run(std::string(b));
      CHECK(r.events == "CLOSE;" && r.output.empty(), "%s -> %s", b, r.events.c_str());
    }
  }

  /* --- the decision: accept, reject, close --- */
  {
    decide = DECIDE_REJECT;
    result_t r = run_split("reject get", "GET /command?SetHeatpump=1 HTTP/1.1\r\nHost: x\r\n\r\n");
    CHECK(r.events == "REQ GET /command q=SetHeatpump=1 ws=0 cl=0 t=0 host=x;WRITE;CLOSE;", "%s", r.events.c_str());
    CHECK(r.output.compare(0, 25, "HTTP/1.1 401 Unauthorized") == 0, "%s", r.output.c_str());

    // a rejected upload: the body is read but never parsed, then the answer is sent
    std::string fw = multipart("/firmware", part("md5", "0123456789abcdef0123456789abcdef") +
                               part("firmware", std::string(3000, 'F'), "; filename=\"HeishaMon.bin\""));
    r = run_split("reject multipart", fw);
    CHECK(!contains(r.events, "ARG") && contains(r.events, "WRITE;CLOSE;"), "%s", r.events.c_str());
    CHECK(r.output.compare(0, 12, "HTTP/1.1 401") == 0, "%s", r.output.c_str());
    // the answer comes only after the whole body was read
    {
      std::vector<std::string> p = { fw.substr(0, fw.size() - 100) };
      result_t part1 = run(p);
      CHECK(part1.output.empty() && !contains(part1.events, "WRITE"), "%s", part1.events.c_str());
    }

    r = run_split("reject urlencoded", with_length("POST /saverules HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\n", "rules=a%3D1"));
    CHECK(!contains(r.events, "ARG") && contains(r.events, "WRITE;CLOSE;"), "%s", r.events.c_str());

    // a rejected websocket upgrade is an ordinary answer, no 101
    r = run_split("reject ws", "GET /ws HTTP/1.1\r\n" + WS_HEADERS + "\r\n");
    CHECK(contains(r.events, "ws=1") && contains(r.events, "WRITE;CLOSE;") && r.websocket == 0, "%s", r.events.c_str());
    CHECK(!contains(r.output, "101") && r.output.compare(0, 12, "HTTP/1.1 401") == 0, "%s", r.output.c_str());

    decide = DECIDE_CLOSE;
    r = run_split("close", fw);
    CHECK(contains(r.events, "REQ POST /firmware") && !contains(r.events, "ARG") && !contains(r.events, "WRITE") &&
          r.output.empty(), "%s", r.events.c_str());
    decide = DECIDE_ACCEPT;
  }

  /* --- websocket --- */
  {
    result_t r = run_split("ws", "GET /ws HTTP/1.1\r\n" + WS_HEADERS + "\r\n");
    CHECK(r.events == "REQ GET /ws q= ws=1 cl=0 t=0 host=heishamon.local;", "%s", r.events.c_str());
    CHECK(r.websocket == 1, "no websocket");
    // a frame in the same read as the request and in a later read
    // (websocket_read() needs a whole frame in one read)
    r = run("GET /ws HTTP/1.1\r\n" + WS_HEADERS + "\r\n" + ws_frame("hello"));
    CHECK(r.events == "REQ GET /ws q= ws=1 cl=0 t=0 host=heishamon.local;WS hello;", "%s", r.events.c_str());
    r = run(std::vector<std::string>{ "GET /ws HTTP/1.1\r\n" + WS_HEADERS + "\r\n", ws_frame("hello") });
    CHECK(r.events == "REQ GET /ws q= ws=1 cl=0 t=0 host=heishamon.local;WS hello;", "%s", r.events.c_str());
    // a frame that claims more bytes than were read is dropped, never read or written past the data
    {
      std::string big = ws_frame("x");
      big[1] = (char)(0x80 | 126);
      big.insert(2, std::string("\xff\xff", 2));
      r = run(std::vector<std::string>{ "GET /ws HTTP/1.1\r\n" + WS_HEADERS + "\r\n", big, std::string("\x81", 1), ws_frame("ok") });
      CHECK(r.events == "REQ GET /ws q= ws=1 cl=0 t=0 host=heishamon.local;WS ok;", "%s", r.events.c_str());
    }
    CHECK(r.websocket == 1, "no websocket");
    CHECK(r.output.compare(0, 34, "HTTP/1.1 101 Switching Protocols\r\n") == 0 &&
          contains(r.output, "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"), "%s", r.output.c_str());

    // query arguments of an accepted upgrade come before the upgrade
    r = run("GET /ws?x=1 HTTP/1.1\r\n" + WS_HEADERS + "\r\n");
    CHECK(r.events == "REQ GET /ws q=x=1 ws=1 cl=0 t=0 host=heishamon.local;ARG x=1;", "%s", r.events.c_str());

    // the upgrade headers on any other path: no upgrade
    const char *paths[] = { "/", "/json", "/wss", "/ws/" };
    for(const char *p : paths) {
      r = run(std::string("GET ") + p + " HTTP/1.1\r\n" + WS_HEADERS + "\r\n");
      CHECK(contains(r.events, " ws=0 ") && contains(r.events, "WRITE;CLOSE;") && r.websocket == 0 && !contains(r.output, "101"),
            "%s: %s", p, r.events.c_str());
    }
    // an incomplete upgrade request on /ws: no upgrade
    std::string variants[] = {
      "Host: x\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\n",                                       // no key
      "Host: x\r\nUpgrade: websocket\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",                     // no version
      "Host: x\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",
      "Host: x\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",             // no Upgrade
      "Host: x\r\nUpgrade: h2c\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",
      "Host: x\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: " + std::string(200, 'k') + "\r\n",
    };
    for(const std::string &v : variants) {
      r = run("GET /ws HTTP/1.1\r\n" + v + "\r\n");
      CHECK(contains(r.events, " ws=0 ") && r.websocket == 0 && !contains(r.output, "101"), "%s", r.events.c_str());
    }
    // a POST is never an upgrade
    r = run("POST /ws HTTP/1.1\r\n" + WS_HEADERS + "Content-Length: 0\r\n\r\n");
    CHECK(contains(r.events, " ws=0 ") && r.websocket == 0, "%s", r.events.c_str());
  }

  /* --- POST bodies of the web page --- */
  {
    // settings form (application/x-www-form-urlencoded), query arguments come first
    result_t r = run_split("settings", with_length("POST /savesettings?q=1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/x-www-form-urlencoded\r\n",
                                                   "wifi_hostname=HeishaMon&mqtt_server=192.168.1.2&update_all_time=300"));
    CHECK(r.events == "REQ POST /savesettings q=q=1 ws=0 cl=67 t=0 host=x ctype=application/x-www-form-urlencoded;"
                      "ARG q=1;ARG wifi_hostname=HeishaMon;ARG mqtt_server=192.168.1.2;ARG update_all_time=300;WRITE;CLOSE;",
          "%s", r.events.c_str());

    // rules via fetch, urlencoded
    r = run_split("saverules", with_length("POST /saverules HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\n", "rules=on%20%40Heatpump_State%20then%0A"));
    CHECK(contains(r.events, "ARG rules=on @Heatpump_State then\n") && contains(r.events, "WRITE;CLOSE;"), "%s", r.events.c_str());

    // empty rules via FormData
    r = run_split("clearrules", multipart("/saverules", part("rules", "")));
    CHECK(contains(r.events, ";ARG rules=;WRITE;CLOSE;"), "%s", r.events.c_str());

    // firmware upload: the value with "\r\n" and pieces of the boundary inside
    std::string bin;
    for(int i = 0; i < 2500; i++) {
      bin += (char)(i * 7);
    }
    bin += "\r\n--" + B.substr(0, 20) + "\r\n-\r\r\n--\r\n";
    bin += std::string(300, '\r');
    std::string body = part("md5", "0123456789abcdef0123456789abcdef") +
                       part("firmware", bin, "; filename=\"HeishaMon_ESP8266-Alpha-0123456789abcdef0123456789abcdef.bin\"\r\nContent-Type: application/octet-stream");
    std::string fw = multipart("/firmware", body);
    r = run_split("firmware", fw);
    CHECK(contains(r.events, "ARG md5=0123456789abcdef0123456789abcdef;ARG firmware="), "%s", r.events.c_str());
    CHECK(contains(r.events, "WRITE;CLOSE;"), "%s", r.events.c_str());
    // the firmware pieces together are exactly the file
    {
      std::string all;
      std::string ev = r.events;
      // collect from a run with a callback that keeps the bytes
      struct collect {
        static int8_t cb(struct webserver_t *client, void *data) {
          if(client->step == WEBSERVER_CLIENT_ARGS) {
            struct arguments_t *a = (struct arguments_t *)data;
            if(strcmp((const char *)a->name, "firmware") == 0) {
              output_collect().append((const char *)a->value, a->len);
            }
          }
          return callback(client, data);
        }
        static std::string &output_collect() { static std::string s; return s; }
      };
      collect::output_collect().clear();
      current_cb = collect::cb;
      run(std::vector<std::string>{ fw.substr(0, 700), fw.substr(700, 1), fw.substr(701) });
      CHECK(collect::output_collect() == bin, "firmware of %zu bytes, %zu expected", collect::output_collect().size(), bin.size());
      current_cb = callback;
    }

    // a Firefox boundary (dashes and digits, longer than the WebKit one)
    {
      const std::string fb = "---------------------------3164735861216357893416587031";
      std::string body = "--" + fb + "\r\nContent-Disposition: form-data; name=\"md5\"\r\n\r\nabc\r\n--" + fb +
                         "\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"f.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n" +
                         std::string(400, 'Z') + "\r\n--" + fb + "--\r\n";
      r = run_split("firefox", with_length("POST /firmware HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=" + fb + "\r\n", body));
      CHECK(contains(r.events, "ARG md5=abc;ARG firmware=") && contains(r.events, "WRITE;CLOSE;"), "%s", r.events.c_str());
    }

    // the field name comes from name=, not from filename=
    r = run(replaced(multipart("/cacert", part("cert", "PEM", "; filename=\"ca.pem\"")), "name=\"cert\"; filename=\"ca.pem\"", "filename=\"ca.pem\"; name=\"cert\""));
    CHECK(contains(r.events, "ARG cert=PEM;"), "%s", r.events.c_str());

    // a preamble before the first boundary and a leading CRLF are skipped
    r = run(with_length("POST /saverules HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=\"" + B + "\"\r\n",
                        "preamble\r\n" + part("rules", "x") + "--" + B + "--\r\nepilogue"));
    CHECK(contains(r.events, "ARG rules=x;WRITE;CLOSE;"), "%s", r.events.c_str());

    // a body that ends before the last boundary: closed, no answer
    std::string cut = "POST /saverules HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=" + B + "\r\nContent-Length: 60\r\n\r\n" +
                      (part("rules", "x") + "--" + B + "--\r\n").substr(0, 60);
    r = run(cut);
    CHECK(!contains(r.events, "WRITE") && contains(r.events, "CLOSE;"), "%s", r.events.c_str());

    // no boundary in the Content-Type: closed
    r = run(with_length("POST /firmware HTTP/1.1\r\nContent-Type: multipart/form-data\r\n", part("md5", "1")));
    CHECK(!contains(r.events, "ARG") && !contains(r.events, "WRITE"), "%s", r.events.c_str());

    // bytes after Content-Length are not body
    r = run(with_length("POST /saverules HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\n", "rules=1") + "GARBAGE");
    CHECK(contains(r.events, "ARG rules=1;WRITE;CLOSE;"), "%s", r.events.c_str());
  }

  /* --- a callback veto in a multipart body stops it --- */
  {
    veto = "md5";
    std::string fw = multipart("/firmware", part("md5", "0123") + part("firmware", std::string(1000, 'F')));
    result_t r = run_split("veto", fw);
    CHECK(r.events == "REQ POST /firmware q= ws=0 cl=" + std::to_string(fw.size() - fw.find("\r\n\r\n") - 4) +
                      " t=0 host=heishamon.local ctype=multipart/form-data; boundary=" + B + ";ARG md5=0123;CLOSE;",
          "%s", r.events.c_str());
    CHECK(r.output.empty(), "%s", r.output.c_str());

    // also a veto on a piece of a long value
    veto = "firmware";
    r = run(fw);
    CHECK(contains(r.events, "ARG firmware=") && !contains(r.events, "WRITE") &&
          r.events.find("ARG firmware=") == r.events.rfind("ARG firmware="), "%s", r.events.c_str());

    // and in the query and an urlencoded body
    veto = "a";
    r = run("GET /command?a=1&b=2 HTTP/1.1\r\n\r\n");
    CHECK(r.events == "REQ GET /command q=a=1&b=2 ws=0 cl=0 t=0;ARG a=1;CLOSE;", "%s", r.events.c_str());
    r = run(with_length("POST /savesettings HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\n", "a=1&b=2"));
    CHECK(!contains(r.events, "ARG b") && !contains(r.events, "WRITE"), "%s", r.events.c_str());
    veto.clear();
  }

  /* --- the request buffer is given back --- */
  {
    run("GET /json HTTP/1.1\r\nCookie: " + std::string(1000, 'c') + "\r\n\r\n");
    CHECK(clients[0].data.req == NULL && clients[0].data.data.boundary == NULL, "request buffer left");
    std::vector<std::string> p = { "GET /json HTTP/1.1\r\nCookie: abc" };  // connection ends in the headers
    run(p);
    webserver_reset_client(&clients[0].data);
    CHECK(clients[0].data.req == NULL, "request buffer left after reset");
  }

  printf("%d checks, %d failed\n", checks, failed);
  return failed == 0 ? 0 : 1;
}
