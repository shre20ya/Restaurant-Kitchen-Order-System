/*
 * Minimal HTTP/1.1 server written with plain sockets (Winsock on Windows,
 * BSD sockets on Linux/macOS). Serves the web/ files and the /api/ endpoints.
 * One request at a time, which is plenty for a kitchen dashboard.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define CLOSESOCK(s) closesocket(s)
  #define BAD_SOCK     INVALID_SOCKET
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <signal.h>
  typedef int sock_t;
  #define CLOSESOCK(s) close(s)
  #define BAD_SOCK     (-1)
#endif

#ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0
#endif

#include "server.h"
#include "api.h"
#include "strbuf.h"

#define REQ_MAX 16384

/* ---------- low-level helpers ---------- */
static int waitReadable(sock_t s, int ms) {
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(s, &fds);
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return select((int)s + 1, &fds, NULL, NULL, &tv) > 0;
}

static int sendAll(sock_t c, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int r = (int)send(c, data + sent, (int)(len - sent), MSG_NOSIGNAL);
        if (r <= 0) return 0;
        sent += (size_t)r;
    }
    return 1;
}

static const char *statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        default:  return "Error";
    }
}

static void sendResponse(sock_t c, int code, const char *ctype, const char *body, size_t len) {
    char head[256];
    int n = snprintf(head, sizeof head,
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
        code, statusText(code), ctype, (unsigned long)len);
    if (sendAll(c, head, (size_t)n) && len) sendAll(c, body, len);
}

static void sendText(sock_t c, int code, const char *text) {
    sendResponse(c, code, "text/plain; charset=utf-8", text, strlen(text));
}

static void sendFile(sock_t c, const char *path, const char *ctype) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        sendText(c, 404, "File not found. Start the program from the project folder (the one that contains the 'web' folder).");
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); sendText(c, 400, "Out of memory."); return; }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    sendResponse(c, 200, ctype, buf, got);
    free(buf);
}

/* ---------- request parsing ---------- */
static int startsNoCase(const char *s, const char *prefix) {
    for (; *prefix; s++, prefix++)
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
    return 1;
}

static long contentLength(const char *h, size_t hlen) {
    const char *p = h, *end = h + hlen;
    while (p < end) {
        if (startsNoCase(p, "content-length:")) return strtol(p + 15, NULL, 10);
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        if (!nl) break;
        p = nl + 1;
    }
    return 0;
}

static int readRequest(sock_t c, char *buf, size_t cap, size_t *hdrLen, size_t *bodyLen) {
    size_t len = 0;
    long clen = 0;
    int haveHdr = 0;
    *hdrLen = *bodyLen = 0;
    for (;;) {
        if (!waitReadable(c, len ? 1500 : 400)) return 0;   /* idle browser pre-connects are dropped */
        int r = (int)recv(c, buf + len, (int)(cap - 1 - len), 0);
        if (r <= 0) return 0;
        len += (size_t)r;
        buf[len] = '\0';
        if (!haveHdr) {
            char *e = strstr(buf, "\r\n\r\n");
            if (e) {
                haveHdr = 1;
                *hdrLen = (size_t)(e - buf) + 4;
                clen = contentLength(buf, *hdrLen);
                if (clen < 0 || (size_t)clen > cap - 1 - *hdrLen) return 0;
            } else if (len >= cap - 1) {
                return 0;
            }
        }
        if (haveHdr && len >= *hdrLen + (size_t)clen) {
            *bodyLen = (size_t)clen;
            return 1;
        }
    }
}

/* ---------- one client ---------- */
static void handleClient(sock_t c, int *quit) {
    char buf[REQ_MAX];
    size_t hl, bl;
    if (!readRequest(c, buf, sizeof buf, &hl, &bl)) return;

    char method[8], target[512];
    if (sscanf(buf, "%7s %511s", method, target) != 2) { sendText(c, 400, "Bad request."); return; }

    const char *query = "";
    char *q = strchr(target, '?');
    if (q) { *q = '\0'; query = q + 1; }

    if (strncmp(target, "/api/", 5) == 0) {
        size_t plen = strlen(query) + bl + 2;
        char *params = malloc(plen);
        if (!params) { sendText(c, 400, "Out of memory."); return; }
        snprintf(params, plen, "%s&%.*s", query, (int)bl, buf + hl);

        StrBuf out;
        sb_init(&out);
        int code = apiHandle(method, target, params, &out, quit);
        if (strcmp(target, "/api/state") != 0) printf("[%s] %s -> %d\n", method, target, code);
        sendResponse(c, code, "application/json; charset=utf-8", out.data, out.len);
        sb_free(&out);
        free(params);
        return;
    }

    if (strcmp(method, "GET") != 0) { sendText(c, 405, "Method not allowed."); return; }

    if (strcmp(target, "/") == 0 || strcmp(target, "/index.html") == 0)
        sendFile(c, "web/index.html", "text/html; charset=utf-8");
    else if (strcmp(target, "/style.css") == 0)
        sendFile(c, "web/style.css", "text/css; charset=utf-8");
    else if (strcmp(target, "/app.js") == 0)
        sendFile(c, "web/app.js", "application/javascript; charset=utf-8");
    else if (strcmp(target, "/favicon.ico") == 0)
        sendResponse(c, 204, "image/x-icon", "", 0);
    else
        sendText(c, 404, "Not found.");
}

static void openInBrowser(int port) {
    char cmd[128];
#ifdef _WIN32
    snprintf(cmd, sizeof cmd, "start \"\" http://127.0.0.1:%d", port);
#elif defined(__APPLE__)
    snprintf(cmd, sizeof cmd, "open http://127.0.0.1:%d >/dev/null 2>&1 &", port);
#else
    snprintf(cmd, sizeof cmd, "xdg-open http://127.0.0.1:%d >/dev/null 2>&1 &", port);
#endif
    if (system(cmd) != 0) { /* the user can open the link manually */ }
}

/* ---------- server loop ---------- */
int serverRun(int port, int openBrowser) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { fprintf(stderr, "Winsock failed to start.\n"); return 0; }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    sock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == BAD_SOCK) { fprintf(stderr, "Could not create socket.\n"); return 0; }

#ifndef _WIN32
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   /* only this computer can connect */
    addr.sin_port        = htons((unsigned short)port);

    if (bind(ls, (struct sockaddr *)&addr, sizeof addr) != 0 || listen(ls, 16) != 0) {
        fprintf(stderr, "Could not use port %d. Is another copy already running? Try: kitchen 9090\n", port);
        CLOSESOCK(ls);
        return 0;
    }

    printf("Server running at  http://127.0.0.1:%d\n", port);
    printf("Press Ctrl+C or use the Exit button on the page to stop.\n\n");
    if (openBrowser) openInBrowser(port);

    int quit = 0;
    while (!quit) {
        sock_t c = accept(ls, NULL, NULL);
        if (c == BAD_SOCK) continue;
        handleClient(c, &quit);
        CLOSESOCK(c);
    }

    CLOSESOCK(ls);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
}
