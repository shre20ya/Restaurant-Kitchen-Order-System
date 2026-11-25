/*
 * Restaurant Kitchen Order Queue System  -  Web edition (pure C backend)
 *
 * Usage:  kitchen [port] [--no-browser]
 * Order lifecycle: PENDING -> PREPARING -> READY -> SERVED (or CANCELLED)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "menu.h"
#include "storage.h"
#include "server.h"

int main(int argc, char **argv) {
    int port = 8080, openBrowser = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-browser") == 0) openBrowser = 0;
        else if (atoi(argv[i]) > 0 && atoi(argv[i]) < 65536) port = atoi(argv[i]);
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("=== Restaurant Kitchen Order Queue System ===\n");

    FILE *probe = fopen("web/index.html", "rb");
    if (!probe) {
        fprintf(stderr, "Cannot find web/index.html. Run the program from the project folder.\n");
        return 1;
    }
    fclose(probe);

    if (loadAll()) printf("Loaded saved data from %s\n", DATA_FILE);
    else           defaultMenu();

    int ok = serverRun(port, openBrowser);
    saveAll();

    freeList(&pending); freeList(&cooking);
    freeList(&ready);   freeList(&done);
    printf("Data saved. Goodbye!\n");
    return ok ? 0 : 1;
}
