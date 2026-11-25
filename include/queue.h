#ifndef QUEUE_H
#define QUEUE_H

#include "types.h"

/* The four order lists + id counter */
extern Order *pending;   /* priority queue: VIP first, FIFO inside a priority */
extern Order *cooking;
extern Order *ready;
extern Order *done;      /* served + cancelled history */
extern int    nextId;

void   pushPriority(Order *o);          /* insert into pending queue by priority */
void   append(Order **head, Order *o);  /* add at end of a list */
Order *detach(Order **head, int id);    /* remove by id, returns node or NULL */
int    countList(const Order *h);
void   freeList(Order **h);

#endif
