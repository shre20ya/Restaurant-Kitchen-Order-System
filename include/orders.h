#ifndef ORDERS_H
#define ORDERS_H

#include <stddef.h>
#include "types.h"

/* Every function returns 1 on success, 0 on failure. `msg` receives a human-readable result. */
int  createOrder(int table, int vip, const char *items, char *msg, size_t n);  /* items = "id:qty,id:qty" */
int  startNext(char *msg, size_t n);
int  markReady(int id, char *msg, size_t n);
int  serveOrder(int id, char *msg, size_t n);
int  cancelOrder(int id, char *msg, size_t n);

typedef struct {
    int    pending, cooking, ready, served, cancelled, vipServed, timed;
    float  revenue;
    double avgPrepSec;
    int    sold[MAX_MENU];     /* quantity sold per menu index */
} Stats;

void computeStats(Stats *s);

#endif
