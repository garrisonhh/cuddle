#ifndef KDL_UTF8_H
#define KDL_UTF8_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef int32_t wide_t;

#ifndef WEOF
#define WEOF (-1)
#endif

// utf8 parsing state
typedef struct kdl_utf8 {
    unsigned char *data;
    size_t data_len, data_idx;

    // when fails to finish a char, it's left here
    wide_t leftover;
    int left_bytes;
} kdl_utf8_t;

/*
 * utf8 character parser
 */
void kdl_utf8_make(kdl_utf8_t *); // just zero-inits
void kdl_utf8_feed(kdl_utf8_t *, char *data, size_t length);

/*
 * returns false if failed to finish char, outputs WEOF at end of file and
 * L'\0' at end of string.
 */
bool kdl_utf8_next(kdl_utf8_t *, wide_t *out_ch);

/*
 * utf8 string utilities
 */

void kdl_utf8_copy(wide_t *dst, wide_t *src);

#endif
