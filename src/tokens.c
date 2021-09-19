#include <cuddle/meta.h>
#include <cuddle/tokens.h>

const char TZR_STATES[][20] = {
    "whitespace",
    "character",
    "newline",
    "eof",
    "c_comm",
    "cpp_comm",
    "sd_comm",
    "raw_string",
    "string"
};

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

// treats CRLF as two newlines
static bool is_newline(wchar_t ch) {
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

static inline wchar_t last_char(kdl_tokenizer_t *tzr) {
    return tzr->len_token > 0 ? tzr->token[tzr->len_token - 1] : L'\0';
}

static void detect_token_type(kdl_tokenizer_t *tzr) {
    // TODO
}

static void process_char(kdl_tokenizer_t *tzr, wchar_t ch) {
    if (tzr->token_break) {
        tzr->token_break = 0;
        tzr->len_token = 0;
    }

    if (tzr->reset_index) {
        tzr->reset_index = 0;
        tzr->len_token = 0;
    }

    // determine next state
    kdl_tokenizer_state_e next_state = tzr->state;

    switch (tzr->state) {
    case KDL_SEQ_NEWLINE:
        next_state = KDL_SEQ_WHITESPACE;

        /* fallthru */
    case KDL_SEQ_WHITESPACE:
        if (!is_whitespace(ch)) {
            if (is_newline(ch))
                next_state = KDL_SEQ_NEWLINE;
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
        } else if (is_newline(ch)) {
            next_state = KDL_SEQ_NEWLINE;

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
        if (is_newline(ch))
            next_state = KDL_SEQ_NEWLINE;

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
        KDL_UNIMPL("unhandled tokenizer state! (%s)\n", TZR_STATES[tzr->state]);

        break;
    }

    // token transitions
    if (next_state != tzr->state) {
        // modify flags
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

    // save tokens
    if (tzr->save_token)
        tzr->token[tzr->len_token++] = ch;

    // apply token expectations
    ;

#if 0
    // debugging
    if (tzr->state != next_state) {
        wprintf(
            L"\'%lc\' %hs -> %hs\n",
            ch, TZR_STATES[tzr->state], TZR_STATES[next_state]
        );
    }
#endif

#if 1
    if (tzr->token_break) {
        tzr->token[tzr->len_token] = L'\0';
        wprintf(L"TOKEN |%ls|\n", tzr->token);
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
