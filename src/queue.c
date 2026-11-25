#include <stdlib.h>
#include "queue.h"

Order *pending = NULL;
Order *cooking = NULL;
Order *ready   = NULL;
Order *done    = NULL;
int    nextId  = 1;

void pushPriority(Order *o) {
    o->next = NULL;
    if (!pending || o->pri > pending->pri) {
        o->next = pending;
        pending = o;
        return;
    }
    Order *cur = pending;
    while (cur->next && cur->next->pri >= o->pri) cur = cur->next;
    o->next = cur->next;
    cur->next = o;
}

void append(Order **head, Order *o) {
    o->next = NULL;
    if (!*head) { *head = o; return; }
    Order *cur = *head;
    while (cur->next) cur = cur->next;
    cur->next = o;
}

Order *detach(Order **head, int id) {
    Order *prev = NULL, *cur = *head;
    while (cur) {
        if (cur->id == id) {
            if (prev) prev->next = cur->next; else *head = cur->next;
            cur->next = NULL;
            return cur;
        }
        prev = cur;
        cur = cur->next;
    }
    return NULL;
}

int countList(const Order *h) {
    int n = 0;
    for (; h; h = h->next) n++;
    return n;
}

void freeList(Order **h) {
    while (*h) { Order *t = *h; *h = t->next; free(t); }
}
