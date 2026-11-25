#ifndef TYPES_H
#define TYPES_H

#include <time.h>

#define MAX_MENU    30
#define MAX_LINES   10
#define NAME_LEN    32
#define CHEFS       2          /* orders that can be cooked at the same time */
#define DATA_FILE   "kitchen.dat"

typedef enum { PENDING, PREPARING, READY, SERVED, CANCELLED } Status;
typedef enum { NORMAL = 0, VIP = 1 } Priority;

typedef struct {
    int   id;
    char  name[NAME_LEN];
    float price;
    int   prepMin;             /* minutes needed to cook this item */
} MenuItem;

typedef struct {
    int menuId;
    int qty;
} OrderLine;

typedef struct Order {
    int       id;
    int       table;
    Priority  pri;
    Status    status;
    OrderLine lines[MAX_LINES];
    int       nLines;
    float     total;
    int       prepMin;         /* estimated prep time = slowest item */
    time_t    created, started, finished;
    struct Order *next;
} Order;

#endif
