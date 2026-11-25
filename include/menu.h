#ifndef MENU_H
#define MENU_H

#include <stddef.h>
#include "types.h"

extern MenuItem menu[MAX_MENU];
extern int menuCount;

void            defaultMenu(void);
const MenuItem *findMenu(int id);
int             addMenuItem(const char *name, float price, int prepMin, char *msg, size_t n);

#endif
