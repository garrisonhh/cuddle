#ifndef KDL_UTF8_H
#define KDL_UTF8_H

#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>

// utf8 parsing state
typedef struct kdl_utf8 {
    unsigned char *data;
    size_t data_len, data_idx;

    // when fails to finish a char, it's left here
    wchar_t leftover;
    int left_bytes;
} kdl_utf8_t;

void kdl_utf8_make(kdl_utf8_t *); // just zero-inits
void kdl_utf8_feed(kdl_utf8_t *, char *data, size_t length);

/*
 * returns false if failed to finish char, outputs WEOF at end of file and
 * L'\0' at end of string.
 */
bool kdl_utf8_next(kdl_utf8_t *, wchar_t *out_ch);

#endif
