#include <stdio.h>
#include <stdlib.h>
#include "storage.h"
#include "types.h"
#include "queue.h"
#include "menu.h"

int saveAll(void) {
    FILE *f = fopen(DATA_FILE, "wb");
    if (!f) return 0;
    Order *lists[4] = { pending, cooking, ready, done };
    int total = 0;
    for (int i = 0; i < 4; i++) total += countList(lists[i]);

    fwrite(&nextId, sizeof nextId, 1, f);
    fwrite(&menuCount, sizeof menuCount, 1, f);
    fwrite(menu, sizeof(MenuItem), (size_t)menuCount, f);
    fwrite(&total, sizeof total, 1, f);
    for (int i = 0; i < 4; i++)
        for (Order *c = lists[i]; c; c = c->next) {
            Order tmp = *c;
            tmp.next = NULL;
            fwrite(&tmp, sizeof tmp, 1, f);
        }
    fclose(f);
    return 1;
}

int loadAll(void) {
    FILE *f = fopen(DATA_FILE, "rb");
    if (!f) return 0;
    int total = 0;
    if (fread(&nextId, sizeof nextId, 1, f) != 1 ||
        fread(&menuCount, sizeof menuCount, 1, f) != 1 ||
        menuCount < 0 || menuCount > MAX_MENU ||
        fread(menu, sizeof(MenuItem), (size_t)menuCount, f) != (size_t)menuCount ||
        fread(&total, sizeof total, 1, f) != 1 || total < 0) {
        fclose(f);
        return 0;
    }
    for (int i = 0; i < total; i++) {
        Order *o = malloc(sizeof *o);
        if (!o || fread(o, sizeof *o, 1, f) != 1) { free(o); break; }
        switch (o->status) {
            case PENDING:   pushPriority(o);     break;
            case PREPARING: append(&cooking, o); break;
            case READY:     append(&ready, o);   break;
            default:        append(&done, o);    break;
        }
    }
    fclose(f);
    return 1;
}
