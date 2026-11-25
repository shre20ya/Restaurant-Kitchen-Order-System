#ifndef STRBUF_H
#define STRBUF_H

#include <stddef.h>

/* Small growable string used to build JSON responses */
typedef struct {
    char  *data;
    size_t len, cap;
} StrBuf;

void sb_init(StrBuf *sb);
void sb_free(StrBuf *sb);
void sb_add(StrBuf *sb, const char *s);
void sb_addf(StrBuf *sb, const char *fmt, ...);
void sb_addjson(StrBuf *sb, const char *s);   /* adds s as an escaped JSON string with quotes */

#endif
