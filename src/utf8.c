#include <cuddle/utf8.h>

void kdl_utf8_make(kdl_utf8_t *state) {
    *state = (kdl_utf8_t){0};
}

void kdl_utf8_feed(kdl_utf8_t *state, char *data, size_t length) {
    state->data = (unsigned char *)data;
    state->data_len = length;
    state->data_idx = 0;
}

bool kdl_utf8_next(kdl_utf8_t *state, kdl_u8ch_t *out_ch) {
    kdl_u8ch_t ch;
    int mb_bytes;

    if (state->left_bytes) {
        // parse leftovers
        ch = state->leftover;
        mb_bytes = state->left_bytes;
        state->left_bytes = 0;
    } else {
        // parse first byte
        kdl_u8ch_t current = state->data[state->data_idx++];

        if (current < 0x80) {
            mb_bytes = 0;
            ch = current;
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

void kdl_utf8_copy(kdl_u8ch_t *dst, kdl_u8ch_t *src) {
    while ((*dst++ = *src++))
        ;
}

// writes results to dst
void kdl_utf8_to_mbs(kdl_u8ch_t ch, char dst[4], size_t *out_bytes) {
    if (ch < 0x80) {
        dst[0] = ch;
        *out_bytes = 1;
    } else if (ch < 0x800) {
        dst[0] = (ch >> 6) | 0xC0;
        *out_bytes = 2;
    } else if (ch < 0x10000) {
        dst[0] = (ch >> 12) | 0xE0;
        *out_bytes = 3;
    } else {
        dst[0] = (ch >> 18) | 0xF0;
        *out_bytes = 4;
    }

    for (size_t i = *out_bytes - 1; i > 0; --i) {
        dst[i] = (ch & 0x3F) | 0x80;
        ch >>= 6;
    }
}
