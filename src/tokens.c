#include <cuddle/meta.h>
#include <cuddle/tokens.h>

#define X(value) #value
const char TOKEN_TYPES[][32] = { KDL_TOKEN_TYPES_X };
const char TOKENIZER_STATES[][32] = { KDL_TOKENIZER_STATES_X };
#undef X

static bool is_whitespace(wchar_t ch) {
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

// treats CRLF as two breaks
static bool is_break(wchar_t ch) {
    switch (ch) {
    case 0x000A:
    case 0x000C:
    case 0x000D:
    case 0x0085:
    case 0x2028:
    case 0x2029:
    case L';':
        return true;

    default:
        return false;

    }
}

static inline wchar_t last_char(kdl_tokenizer_t *tzr) {
    return tzr->len_token > 0 ? tzr->token[tzr->len_token - 1] : L'\0';
}

/*
 * TODO this function is kind of a behemoth, I'm now realizing I unintentionally
 * gave it 2 responsibilities: finding the next state, and setting flags based
 * on that state. splitting these responsibilities up would significantly
 * simplify it
 */
static kdl_tokenizer_state_e find_next_state(kdl_tokenizer_t *tzr, wchar_t ch) {
    kdl_tokenizer_state_e next_state = tzr->state;

    switch (tzr->state) {
    case KDL_SEQ_BREAK:
        next_state = KDL_SEQ_WHITESPACE;

        tzr->save_break = 1;
        tzr->token_break = 1;

        /* fallthru */
    case KDL_SEQ_WHITESPACE:
        if (!is_whitespace(ch)) {
            if (is_break(ch))
                next_state = KDL_SEQ_BREAK;
            else if (ch == L'"')
                next_state = KDL_SEQ_STRING;
            else
                next_state = KDL_SEQ_CHARACTER;
        }

        break;
    case KDL_SEQ_CHARACTER:
        // identify state changes (yeah this is some messy logic)
        if (is_whitespace(ch)) {
            next_state = KDL_SEQ_WHITESPACE;

            tzr->token_break = 1;
        } else if (is_break(ch)) {
            next_state = KDL_SEQ_BREAK;

            tzr->token_break = 1;
        } else if (ch == L'"') {
            next_state = KDL_SEQ_STRING;

            tzr->token_break = 1;
        } else if (tzr->check_comm) {
            bool found_comm = true;

            switch (ch) {
            case L'*':
                next_state = KDL_SEQ_C_COMM;

                break;
            case L'/':
                next_state = KDL_SEQ_CPP_COMM;

                break;
            case L'-':
                if (tzr->len_token == 1)
                    next_state = KDL_SEQ_SD_COMM;

                break;
            default:
                found_comm = false;

                break;
            }

            // if a comment is found, discard L'/', then break token if its
            // length is longer than 0
            if (found_comm && --tzr->len_token)
                tzr->token_break = 1;
        } else if (ch == L'/') {
            tzr->check_comm = 1;
        } else if (ch == WEOF) {
            next_state = KDL_SEQ_EOF;

            tzr->token_break = 1;
        }

        break;
    case KDL_SEQ_CPP_COMM:
        if (is_break(ch))
            next_state = KDL_SEQ_BREAK;

        break;
    case KDL_SEQ_C_COMM:
        // TODO handle nested c comments
        if (ch == L'/' && last_char(tzr) == L'*')
            next_state = KDL_SEQ_WHITESPACE;

        break;
    case KDL_SEQ_STRING:
        if (tzr->str_escape) {
            // escape characters
            ; // TODO
            tzr->str_escape = 0;
        } else if (ch == L'"') {
            next_state = KDL_SEQ_WHITESPACE;

            tzr->token_break = 1;
        } else if (ch == L'\\') {
            // flag escape next char
            tzr->str_escape = 1;
        }

        break;
    default:
        KDL_UNIMPL(
            "unhandled tokenizer state %s\n",
            TOKENIZER_STATES[tzr->state]
        );
    }

    return next_state;
}

// act on flags set from previous process calls before finding next state
static void act_on_previous_state(kdl_tokenizer_t *tzr) {
    tzr->save_break = 0;

    if (tzr->token_break) {
        tzr->token_break = 0;
        tzr->len_token = 0;
    }

    if (tzr->reset_index) {
        tzr->reset_index = 0;
        tzr->len_token = 0;
    }
}

static kdl_token_type_e detect_token_type(kdl_tokenizer_t *tzr) {
    switch (tzr->state) {
    case KDL_SEQ_BREAK:
        return KDL_TOK_BREAK;

    case KDL_SEQ_CHARACTER:
        if (tzr->len_token == 1) {
            if (tzr->token[0] == L'{')
                return KDL_TOK_CHILD_BEGIN;
            else if (tzr->token[0] == L'}')
                return KDL_TOK_CHILD_END;
        }

        return KDL_TOK_NODE;

    case KDL_SEQ_STRING:
        return KDL_TOK_STRING;

    case KDL_SEQ_RAW_STRING:
        return KDL_TOK_RAW_STRING;

    default:
        KDL_ERROR(
            "attempted to detect type of token after %s\n",
            TOKENIZER_STATES[tzr->state]
        );

    }
}

// after finding next state, set flags
static void act_on_state(
    kdl_tokenizer_t *tzr, wchar_t ch, kdl_tokenizer_state_e next_state
) {
    // token transition flags
    if (next_state != tzr->state) {
        switch (next_state) {
        case KDL_SEQ_RAW_STRING:
        case KDL_SEQ_STRING:
            tzr->reset_index = 1; // remove initial quotes

            /* fallthru */
        case KDL_SEQ_CHARACTER:
            tzr->save_token = 1;

            break;
        default:
            tzr->save_token = 0;

            break;
        }
    }

    // save tokens if called for
    if (tzr->save_token)
        tzr->token[tzr->len_token++] = ch;

    if (tzr->save_break)
        tzr->token[tzr->len_token++] = L';';

    // detect token type once broken
    if (tzr->token_break)
        tzr->token_type = detect_token_type(tzr);
}

static void process_char(kdl_tokenizer_t *tzr, wchar_t ch) {
    kdl_tokenizer_state_e next_state;

    // keep that state machine rolling
    act_on_previous_state(tzr);
    next_state = find_next_state(tzr, ch);
    act_on_state(tzr, ch, next_state);

    // debugging
#if 0
    if (tzr->state != next_state) {
        wprintf(
            L"\'%lc\' %hs -> %hs\n",
            ch, TOKENIZER_STATES[tzr->state], TOKENIZER_STATES[next_state]
        );
    }
#endif

#if 1
    if (tzr->token_break) {
        tzr->token[tzr->len_token] = L'\0';
        wprintf(L"%-32s|%ls|\n", TOKEN_TYPES[tzr->token_type], tzr->token);
        fflush(stdout);
    }
#endif

    tzr->state = next_state;
}

void kdl_tokenizer_make(kdl_tokenizer_t *tzr) {
    *tzr = (kdl_tokenizer_t){0};
}

void kdl_tokenizer_feed(kdl_tokenizer_t *tzr, wchar_t *data, size_t length) {
    for (size_t i = 0; i < length && tzr->state != KDL_SEQ_EOF; ++i)
        process_char(tzr, data[i]);
}
