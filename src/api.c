#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "api.h"
#include "types.h"
#include "queue.h"
#include "menu.h"
#include "orders.h"
#include "storage.h"

static const char *STATUS_NAME[] = { "PENDING", "PREPARING", "READY", "SERVED", "CANCELLED" };

/* ---- read one value from "a=1&b=hello+world%21" ---- */
static int param(const char *data, const char *key, char *out, size_t n) {
    size_t kl = strlen(key);
    const char *p = data;
    while (p && *p) {
        if (strncmp(p, key, kl) == 0 && p[kl] == '=') {
            p += kl + 1;
            size_t i = 0;
            while (*p && *p != '&' && i + 1 < n) {
                if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
                    char h[3] = { p[1], p[2], 0 };
                    out[i++] = (char)strtol(h, NULL, 16);
                    p += 3;
                } else if (*p == '+') {
                    out[i++] = ' ';
                    p++;
                } else {
                    out[i++] = *p++;
                }
            }
            out[i] = '\0';
            return 1;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    if (n) out[0] = '\0';
    return 0;
}

static int paramInt(const char *data, const char *key, int def) {
    char t[32];
    if (!param(data, key, t, sizeof t)) return def;
    return atoi(t);
}

/* ---- JSON builders ---- */
static void orderJson(StrBuf *sb, const Order *o, int waitMin, int pos) {
    sb_addf(sb, "{\"id\":%d,\"table\":%d,\"vip\":%s,\"status\":\"%s\",\"total\":%.2f,"
                "\"prepMin\":%d,\"wait\":%d,\"pos\":%d,"
                "\"created\":%lld,\"started\":%lld,\"finished\":%lld,\"items\":[",
            o->id, o->table, o->pri == VIP ? "true" : "false", STATUS_NAME[o->status],
            o->total, o->prepMin, waitMin, pos,
            (long long)o->created, (long long)o->started, (long long)o->finished);
    for (int i = 0; i < o->nLines; i++) {
        const MenuItem *m = findMenu(o->lines[i].menuId);
        if (i) sb_add(sb, ",");
        sb_addf(sb, "{\"id\":%d,\"qty\":%d,\"name\":", o->lines[i].menuId, o->lines[i].qty);
        sb_addjson(sb, m ? m->name : "(removed item)");
        sb_add(sb, "}");
    }
    sb_add(sb, "]}");
}

static void listJson(StrBuf *sb, const char *key, const Order *h, int withWait, int maxItems) {
    sb_addf(sb, "\"%s\":[", key);
    int total = countList(h), skip = (maxItems > 0 && total > maxItems) ? total - maxItems : 0;
    int ahead = 0, idx = 0, pos = 1, first = 1;
    for (; h; h = h->next, idx++) {
        if (idx < skip) continue;
        if (!first) sb_add(sb, ",");
        first = 0;
        int wait = withWait ? ahead / CHEFS + h->prepMin : 0;
        orderJson(sb, h, wait, withWait ? pos : 0);
        ahead += h->prepMin;
        pos++;
    }
    sb_add(sb, "]");
}

static void stateJson(StrBuf *sb) {
    Stats st;
    computeStats(&st);
    sb_addf(sb, "{\"now\":%lld,\"chefs\":%d,\"menu\":[", (long long)time(NULL), CHEFS);
    for (int i = 0; i < menuCount; i++) {
        if (i) sb_add(sb, ",");
        sb_addf(sb, "{\"id\":%d,\"price\":%.2f,\"prep\":%d,\"name\":", menu[i].id, menu[i].price, menu[i].prepMin);
        sb_addjson(sb, menu[i].name);
        sb_add(sb, "}");
    }
    sb_add(sb, "],");
    listJson(sb, "pending", pending, 1, 0);  sb_add(sb, ",");
    listJson(sb, "cooking", cooking, 0, 0);  sb_add(sb, ",");
    listJson(sb, "ready",   ready,   0, 0);  sb_add(sb, ",");
    listJson(sb, "done",    done,    0, 40); sb_add(sb, ",");
    sb_addf(sb, "\"stats\":{\"pending\":%d,\"cooking\":%d,\"ready\":%d,\"served\":%d,\"cancelled\":%d,"
                "\"vipServed\":%d,\"revenue\":%.2f,\"avgPrepSec\":%.0f,\"sold\":[",
            st.pending, st.cooking, st.ready, st.served, st.cancelled,
            st.vipServed, st.revenue, st.avgPrepSec);
    for (int i = 0; i < menuCount; i++) {
        if (i) sb_add(sb, ",");
        sb_addf(sb, "{\"qty\":%d,\"name\":", st.sold[i]);
        sb_addjson(sb, menu[i].name);
        sb_add(sb, "}");
    }
    sb_add(sb, "]}}");
}

static int reply(StrBuf *out, int ok, const char *text) {
    sb_add(out, ok ? "{\"ok\":true,\"message\":" : "{\"ok\":false,\"error\":");
    sb_addjson(out, text);
    sb_add(out, "}");
    if (ok) saveAll();                 /* auto-save after every successful change */
    return ok ? 200 : 400;
}

int apiHandle(const char *method, const char *path, const char *params, StrBuf *out, int *quit) {
    char msg[160] = "";
    int post = strcmp(method, "POST") == 0;

    if (strcmp(path, "/api/state") == 0) {
        stateJson(out);
        return 200;
    }
    if (!post) return reply(out, 0, "Use POST for this action.") + 5;   /* 405 */

    if (strcmp(path, "/api/order") == 0) {
        char items[256];
        param(params, "items", items, sizeof items);
        int ok = createOrder(paramInt(params, "table", 0), paramInt(params, "vip", 0), items, msg, sizeof msg);
        return reply(out, ok, msg);
    }
    if (strcmp(path, "/api/start") == 0)  return reply(out, startNext(msg, sizeof msg), msg);
    if (strcmp(path, "/api/ready") == 0)  return reply(out, markReady(paramInt(params, "id", 0), msg, sizeof msg), msg);
    if (strcmp(path, "/api/serve") == 0)  return reply(out, serveOrder(paramInt(params, "id", 0), msg, sizeof msg), msg);
    if (strcmp(path, "/api/cancel") == 0) return reply(out, cancelOrder(paramInt(params, "id", 0), msg, sizeof msg), msg);
    if (strcmp(path, "/api/menu") == 0) {
        char name[64], price[32];
        param(params, "name", name, sizeof name);
        param(params, "price", price, sizeof price);
        int ok = addMenuItem(name, (float)atof(price), paramInt(params, "prep", 0), msg, sizeof msg);
        return reply(out, ok, msg);
    }
    if (strcmp(path, "/api/exit") == 0) {
        saveAll();
        *quit = 1;
        return reply(out, 1, "Data saved. Server is shutting down.");
    }
    return reply(out, 0, "Unknown API endpoint.") + 4;   /* 404 */
}
