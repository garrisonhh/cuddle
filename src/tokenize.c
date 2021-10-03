#include <cuddle/meta.h>
#include <cuddle/tokenize.h>
#include "token_parse.h"

#define X(name) #name
const char KDL_TOKEN_TYPES[][32] = { KDL_TOKEN_TYPES_X };
const char KDL_TOKENIZER_STATES[][32] = { KDL_TOKENIZER_STATES_X };
#undef X

void kdl_tokenizer_make(kdl_tokenizer_t *tzr, kdl_u8ch_t *buffer, size_t buf_size) {
    *tzr = (kdl_tokenizer_t){
        .buf = buffer,
        .buf_size = buf_size,
        .expect_node = true
    };
}

void kdl_token_make(kdl_token_t *token, char *buffer) {
    *token = (kdl_token_t){
        .string = buffer
    };
}

void kdl_tok_feed(kdl_tokenizer_t *tzr, char *data, size_t length) {
    kdl_utf8_feed(&tzr->utf8, data, length);
}

static bool is_whitespace(kdl_u8ch_t ch) {
    switch (ch) {
    case 0x0009:
    case 0x0020:
    case 0x00A0:
    case 0x1680:
    case 0x202F:
    case 0x205F:
    case 0x3000:
        return true;

    default:
        return ch >= 0x2000 && ch <= 0x200A;

    }
}

static bool is_newline(kdl_u8ch_t ch) {
    switch (ch) {
    case 0x000A:
    case 0x000C:
    case 0x000D:
    case 0x0085:
    case 0x2028:
    case 0x2029:
        return true;

    default:
        return false;

    }
}

static inline bool is_break(kdl_u8ch_t ch) {
    return is_newline(ch) || ch == L';' || ch == (kdl_u8ch_t)WEOF;
}

// assumes that tokenizer isn't in a special sequence (like a string or comment)
static kdl_tokenizer_state_e detect_next_state(
    kdl_tokenizer_t *tzr, kdl_u8ch_t ch
) {
    // character classes
    if (is_whitespace(ch))
        return KDL_SEQ_WHITESPACE;
    else if (is_break(ch))
        return KDL_SEQ_BREAK;

    // direct char mappings
    switch (ch) {
    case L'{': return KDL_SEQ_CHILD_BEGIN;
    case L'}': return KDL_SEQ_CHILD_END;
    case L'=': return KDL_SEQ_ASSIGNMENT;
    case L'(': return KDL_SEQ_ANNOTATION;
    case L'\\': return KDL_SEQ_BREAK_ESC;
    case L'"':
        // could be raw string or string
        if (tzr->state == KDL_SEQ_CHARACTER) {
            if (tzr->last_char == L'r') {
                // no '#'s
                tzr->raw_count = 1;

                return KDL_SEQ_RAW_STR;
            } else if (tzr->buf[0] == L'r') {
                // check for 1+ '#'s
                tzr->raw_count = 1;
                tzr->buf[tzr->buf_len] = tzr->last_char;

                for (size_t i = 1; i <= tzr->buf_len; ++i) {
                    if (tzr->buf[i] == L'#')
                        ++tzr->raw_count;
                    else
                        return KDL_SEQ_STRING;
                }

                return KDL_SEQ_RAW_STR;
            }
        }

        return KDL_SEQ_STRING;
    }

    // multi char mappings
    if (tzr->state == KDL_SEQ_CHARACTER && tzr->last_char == L'/') {
        switch (ch) {
        case L'*':
            return KDL_SEQ_C_COMM;
        case L'/':
            return KDL_SEQ_CPP_COMM;
        case L'-':
            if (tzr->buf_len == 0)
                return KDL_SEQ_SD_COMM;

            break;
        }
    }

    // unidentified, must be identifier or non-string data type
    return KDL_SEQ_CHARACTER;
}

/*
 * this function's responsibilities are limited exclusively to splitting tokens
 * up through the tokenizer state machine. anything else is out of scope.
 */
static void consume_char(kdl_tokenizer_t *tzr, kdl_u8ch_t ch) {
    // reset flags sent to tok_next()
    tzr->token_break = false;

    if (tzr->reset_buf) {
        tzr->reset_buf = false;
        tzr->buf_len = 0;
    }

    // detect state changes
    kdl_tokenizer_state_e next_state = tzr->state;
    bool force_change = false;

    if (tzr->force_detect) {
        /*
         * after sequences, detection is forced. this allows for tokens to be
         * detected without spacing in between them like `"str"(hi)1234//comm`
         */
        tzr->force_detect = false;
        force_change = true;

        next_state = detect_next_state(tzr, ch);
    } else {
        switch (tzr->state) {
        default:
            next_state = detect_next_state(tzr, ch);

            break;
        case KDL_SEQ_C_COMM:
            // track nested comments and stuff
            if (tzr->last_char == L'/' && ch == L'*') {
                ++tzr->c_comm_level;
            } else if (tzr->last_char == L'*' && ch == L'/') {
                if (tzr->c_comm_level)
                    --tzr->c_comm_level;
                else
                    tzr->force_detect = true;
            }

            break;
        case KDL_SEQ_CPP_COMM:
            if (is_newline(ch))
                next_state = KDL_SEQ_BREAK;

            break;
        case KDL_SEQ_STRING:
            tzr->force_detect = ch == L'"';

            break;
        case KDL_SEQ_RAW_STR:
            // await matching '#' sequence
            if (ch == L'"') {
                tzr->raw_current = 1;
            } else if (tzr->raw_current && ch == L'#') {
                ++tzr->raw_current;
            } else {
                if (tzr->raw_current == tzr->raw_count)
                    next_state = detect_next_state(tzr, ch);

                tzr->raw_current = 0;
            }

            break;
        case KDL_SEQ_ANNOTATION:
            tzr->force_detect = ch == L')';

            break;
        }
    }

    // act on state change
    if (next_state != tzr->state || force_change) {
        tzr->reset_buf = true;

        switch (tzr->state) {
        case KDL_SEQ_CHARACTER:
            // check for multi-char sequences which start out as characters
            if (next_state == KDL_SEQ_C_COMM
             || next_state == KDL_SEQ_CPP_COMM) {
                if (!tzr->buf_len) {
                    break;
                } else {
                    /*
                     * TODO this prevents the slash from being recorded in the
                     * token buffer, but is definitely hacky. find a better
                     * solution?
                     */
                    tzr->buf_len += 2;
                }
            } else if (next_state == KDL_SEQ_RAW_STR
                    || next_state == KDL_SEQ_SD_COMM) {
                break;
            }

            /* fallthru */
        case KDL_SEQ_STRING:
        case KDL_SEQ_RAW_STR:
        // below cases don't return values, but are necessary for proper parsing
        case KDL_SEQ_CHILD_BEGIN:
        case KDL_SEQ_CHILD_END:
            tzr->token_break = true;

            break;
        default:
            break;
        }
    }

    // store token
    if (tzr->buf_len < (tzr->buf_size - 1)) {
        tzr->buf[tzr->buf_len] = tzr->last_char;
        tzr->buf[++tzr->buf_len] = L'\0';
    } else {
        KDL_ERROR(
            "tokenizer tried to write past the end of the supplied buffer. plea"
            "se supply a larger buffer.\n"
        );
    }

    // store state
    tzr->last_state = tzr->state;
    tzr->state = next_state;
    tzr->last_char = ch;
}

/*
 * fills in 'token' with data of next token and returns true, or returns false
 * if the current token isn't finished yet (needs more data). this lets you use
 * the idiom:
 *
 * while (kdl_tok_next()) { [ do something with token... ] }
 *
 * this functions responsibilities are limited to sanitizing the raw token
 * stream from the state machine (processing line break escapes and slashdashes)
 * and returning fully typed and usable tokens to the user
 */
bool kdl_tok_next(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    kdl_u8ch_t ch;

    while (kdl_utf8_next(&tzr->utf8, &ch) && ch && ch != (kdl_u8ch_t)WEOF) {
        consume_char(tzr, ch);

        // line break and node slashdash state machine
        switch (tzr->last_state) {
        case KDL_SEQ_CHILD_BEGIN:
            ++tzr->sd_node_level;
            tzr->expect_node = true;

            break;
        case KDL_SEQ_CHILD_END:
            --tzr->sd_node_level;
            tzr->expect_node = true;

            break;
        case KDL_SEQ_BREAK:
            if (tzr->break_escape)
                tzr->break_escape = false;
            else
                tzr->expect_node = true;

            // end of slashdash nodes
            if (tzr->sd_node && tzr->sd_node_level == 0)
                tzr->sd_node = false;

            break;
        case KDL_SEQ_BREAK_ESC:
            tzr->break_escape = true;

            break;
        case KDL_SEQ_SD_COMM:
            if (!(tzr->sd_node || tzr->sd_value)) {
                if (tzr->expect_node) {
                    tzr->sd_node = true;
                    tzr->sd_node_level = 0;
                } else {
                    tzr->sd_value = true;
                }
            }

            break;
        default:
            break;
        }

        // tokens
        if (tzr->token_break && !tzr->sd_node) {
            if (tzr->sd_value) {
                // slashdash values until they aren't prop names
                if (tzr->state != KDL_SEQ_ASSIGNMENT)
                    tzr->sd_value = false;
            } else {
                // valid non-slashdashed token; type, parse, and pass
                generate_token(tzr, token);

                return true;
            }
        }
    }

    return false;
}
