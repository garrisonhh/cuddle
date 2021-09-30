#include <cuddle/utf8.h>

void kdl_utf8_make(kdl_utf8_t *state) {
    *state = (kdl_utf8_t){0};
}

void kdl_utf8_feed(kdl_utf8_t *state, char *data, size_t length) {
    state->data = (unsigned char *)data;
    state->data_len = length;
    state->data_idx = 0;
}

bool kdl_utf8_next(kdl_utf8_t *state, wide_t *out_ch) {
    wide_t ch;
    int mb_bytes;

    if (state->left_bytes) {
        // parse leftovers
        ch = state->leftover;
        mb_bytes = state->left_bytes;
        state->left_bytes = 0;
    } else {
        // parse first byte
        wide_t current = state->data[state->data_idx++];

        if (current < 0x80) {
            mb_bytes = 0;
            ch = current & 0x7F;
        } else if (current < 0xE0) {
            mb_bytes = 1;
            ch = current & 0x1F;
        } else if (current < 0xF0) {
            mb_bytes = 2;
            ch = current & 0x0F;
        } else {
            mb_bytes = 3;
            ch = current & 0x07;
        }
    }

    // parse rest of character and return
    while (state->data_idx < state->data_len && mb_bytes) {
        ch = (ch << 6) | (state->data[state->data_idx++] & 0x3F);
        --mb_bytes;
    }

    if (mb_bytes) {
        // bytes left over
        state->leftover = ch;
        state->left_bytes = mb_bytes;
        
        return false;
    }

    *out_ch = ch;

    return true;
}

void kdl_utf8_copy(wide_t *dst, wide_t *src) {
    while ((*dst++ = *src++))
        ;
}
