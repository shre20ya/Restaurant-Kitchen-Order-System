#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "orders.h"
#include "queue.h"
#include "menu.h"

static int fail(char *msg, size_t n, const char *text) {
    snprintf(msg, n, "%s", text);
    return 0;
}

int createOrder(int table, int vip, const char *items, char *msg, size_t n) {
    if (table < 1 || table > 50) return fail(msg, n, "Table number must be between 1 and 50.");
    Order *o = calloc(1, sizeof *o);
    if (!o) return fail(msg, n, "Out of memory.");

    const char *p = items ? items : "";
    while (*p) {
        char *end;
        long id = strtol(p, &end, 10);
        if (end == p || *end != ':') { free(o); return fail(msg, n, "Invalid item list."); }
        p = end + 1;
        long qty = strtol(p, &end, 10);
        if (end == p) { free(o); return fail(msg, n, "Invalid item list."); }
        p = end;
        if (*p == ',') p++;
        else if (*p)   { free(o); return fail(msg, n, "Invalid item list."); }

        if (!findMenu((int)id))    { free(o); return fail(msg, n, "Unknown menu item."); }
        if (qty < 1 || qty > 20)   { free(o); return fail(msg, n, "Quantity must be 1 to 20."); }

        int merged = 0;
        for (int i = 0; i < o->nLines; i++)
            if (o->lines[i].menuId == (int)id) {
                o->lines[i].qty += (int)qty;
                if (o->lines[i].qty > 20) o->lines[i].qty = 20;
                merged = 1;
                break;
            }
        if (!merged) {
            if (o->nLines >= MAX_LINES) { free(o); return fail(msg, n, "Too many different items (max 10)."); }
            o->lines[o->nLines].menuId = (int)id;
            o->lines[o->nLines].qty = (int)qty;
            o->nLines++;
        }
    }
    if (o->nLines == 0) { free(o); return fail(msg, n, "Add at least one item to the order."); }

    for (int i = 0; i < o->nLines; i++) {
        const MenuItem *m = findMenu(o->lines[i].menuId);
        o->total += m->price * (float)o->lines[i].qty;
        if (m->prepMin > o->prepMin) o->prepMin = m->prepMin;
    }
    o->table   = table;
    o->pri     = vip ? VIP : NORMAL;
    o->id      = nextId++;
    o->status  = PENDING;
    o->created = time(NULL);
    pushPriority(o);
    snprintf(msg, n, "Order #%d placed for table %d%s.", o->id, table, vip ? " (VIP)" : "");
    return 1;
}

int startNext(char *msg, size_t n) {
    if (!pending) return fail(msg, n, "No pending orders in the queue.");
    if (countList(cooking) >= CHEFS) {
        snprintf(msg, n, "All %d chefs are busy. Mark an order READY first.", CHEFS);
        return 0;
    }
    Order *o = detach(&pending, pending->id);
    o->status = PREPARING;
    o->started = time(NULL);
    append(&cooking, o);
    snprintf(msg, n, "Started cooking order #%d (table %d).", o->id, o->table);
    return 1;
}

static int move(Order **from, Order **to, Status st, int id, char *msg, size_t n,
                const char *notFound, const char *okFmt) {
    Order *o = detach(from, id);
    if (!o) { snprintf(msg, n, "%s", notFound); return 0; }
    o->status = st;
    if (st == READY) o->finished = time(NULL);
    append(to, o);
    snprintf(msg, n, okFmt, o->id);
    return 1;
}

int markReady(int id, char *msg, size_t n) {
    return move(&cooking, &ready, READY, id, msg, n,
                "That order is not being cooked.", "Order #%d is READY to serve.");
}

int serveOrder(int id, char *msg, size_t n) {
    return move(&ready, &done, SERVED, id, msg, n,
                "That order is not ready to serve.", "Order #%d served. Enjoy!");
}

int cancelOrder(int id, char *msg, size_t n) {
    return move(&pending, &done, CANCELLED, id, msg, n,
                "Only pending orders can be cancelled.", "Order #%d cancelled.");
}

void computeStats(Stats *s) {
    memset(s, 0, sizeof *s);
    s->pending = countList(pending);
    s->cooking = countList(cooking);
    s->ready   = countList(ready);
    double sec = 0;
    for (const Order *o = done; o; o = o->next) {
        if (o->status == SERVED) {
            s->served++;
            s->revenue += o->total;
            if (o->pri == VIP) s->vipServed++;
            if (o->started && o->finished >= o->started) {
                sec += difftime(o->finished, o->started);
                s->timed++;
            }
            for (int i = 0; i < o->nLines; i++)
                for (int m = 0; m < menuCount; m++)
                    if (menu[m].id == o->lines[i].menuId) s->sold[m] += o->lines[i].qty;
        } else if (o->status == CANCELLED) {
            s->cancelled++;
        }
    }
    s->avgPrepSec = s->timed ? sec / s->timed : 0;
}
