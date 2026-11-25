#ifndef API_H
#define API_H

#include "strbuf.h"

/*
 * Handles /api/... requests. `params` holds URL-encoded key=value pairs
 * (query string and body combined). Returns the HTTP status code and
 * writes a JSON document into `out`. Sets *quit = 1 when the server should stop.
 */
int apiHandle(const char *method, const char *path, const char *params, StrBuf *out, int *quit);

#endif
