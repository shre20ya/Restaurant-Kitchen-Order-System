#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strbuf.h"

static void reserve(StrBuf *sb, size_t extra) {
    if (sb->len + extra + 1 <= sb->cap) return;
    size_t cap = sb->cap ? sb->cap : 1024;
    while (cap < sb->len + extra + 1) cap *= 2;
    char *p = realloc(sb->data, cap);
    if (!p) { fprintf(stderr, "Out of memory\n"); exit(1); }
    sb->data = p;
    sb->cap = cap;
}

void sb_init(StrBuf *sb) {
    sb->data = NULL;
    sb->len = sb->cap = 0;
    reserve(sb, 0);
    sb->data[0] = '\0';
}

void sb_free(StrBuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

void sb_add(StrBuf *sb, const char *s) {
    size_t n = strlen(s);
    reserve(sb, n);
    memcpy(sb->data + sb->len, s, n + 1);
    sb->len += n;
}

void sb_addf(StrBuf *sb, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n > 0) {
        reserve(sb, (size_t)n);
        vsnprintf(sb->data + sb->len, (size_t)n + 1, fmt, ap2);
        sb->len += (size_t)n;
    }
    va_end(ap2);
}

void sb_addjson(StrBuf *sb, const char *s) {
    sb_add(sb, "\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '"':  sb_add(sb, "\\\""); break;
            case '\\': sb_add(sb, "\\\\"); break;
            case '\n': sb_add(sb, "\\n");  break;
            case '\r': sb_add(sb, "\\r");  break;
            case '\t': sb_add(sb, "\\t");  break;
            default:
                if (c < 0x20) sb_addf(sb, "\\u%04x", c);
                else { char t[2] = { (char)c, 0 }; sb_add(sb, t); }
        }
    }
    sb_add(sb, "\"");
}
