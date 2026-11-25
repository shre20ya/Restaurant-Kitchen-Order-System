#include <stdio.h>
#include <string.h>
#include "menu.h"

MenuItem menu[MAX_MENU];
int menuCount = 0;

void defaultMenu(void) {
    const MenuItem d[] = {
        {1, "Margherita Pizza", 249.0f, 12}, {2, "Pasta Alfredo", 199.0f, 10},
        {3, "Grilled Chicken",  299.0f, 15}, {4, "Caesar Salad",  149.0f, 5},
        {5, "French Fries",      99.0f, 6},  {6, "Veg Burger",    129.0f, 8},
        {7, "Tomato Soup",       99.0f, 7},  {8, "Cold Coffee",    89.0f, 3}
    };
    menuCount = (int)(sizeof d / sizeof d[0]);
    memcpy(menu, d, sizeof d);
}

const MenuItem *findMenu(int id) {
    for (int i = 0; i < menuCount; i++)
        if (menu[i].id == id) return &menu[i];
    return NULL;
}

int addMenuItem(const char *name, float price, int prepMin, char *msg, size_t n) {
    if (menuCount >= MAX_MENU) { snprintf(msg, n, "Menu is full (max %d items).", MAX_MENU); return 0; }
    if (!name || !name[0])     { snprintf(msg, n, "Item name cannot be empty."); return 0; }
    if (strlen(name) >= NAME_LEN) { snprintf(msg, n, "Name is too long (max %d characters).", NAME_LEN - 1); return 0; }
    if (price < 0.01f || price > 100000.0f) { snprintf(msg, n, "Price must be between 0.01 and 100000."); return 0; }
    if (prepMin < 1 || prepMin > 120)       { snprintf(msg, n, "Prep time must be 1 to 120 minutes."); return 0; }

    MenuItem m;
    memset(&m, 0, sizeof m);
    m.id = menuCount ? menu[menuCount - 1].id + 1 : 1;
    strncpy(m.name, name, NAME_LEN - 1);
    m.price = price;
    m.prepMin = prepMin;
    menu[menuCount++] = m;
    snprintf(msg, n, "Added '%s' to the menu.", m.name);
    return 1;
}
