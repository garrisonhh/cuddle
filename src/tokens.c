#include <cuddle/meta.h>
#include <cuddle/tokens.h>

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
    return tzr->index > 0 ? tzr->token[tzr->index - 1] : L'\0';
}

static void process_char(kdl_tokenizer_t *tzr, wchar_t ch) {
    // determine next state
    kdl_tokenizer_state_e next_state = tzr->state;
    bool token_break = false;

    switch (tzr->state) {
    case KDL_SEQ_NEWLINE:
        next_state = KDL_SEQ_WHITESPACE;
        /* fallthru */
    case KDL_SEQ_WHITESPACE:
        if (!is_whitespace(ch)) {
            if (is_newline(ch))
                next_state = KDL_SEQ_NEWLINE;
            else
                next_state = KDL_SEQ_CHARACTER;
        }

        break;
    case KDL_SEQ_SD_COMM:
    case KDL_SEQ_CHARACTER:
        if (is_whitespace(ch)) {
            next_state = KDL_SEQ_WHITESPACE;
            token_break = true;
        } else if (is_newline(ch)) {
            next_state = KDL_SEQ_NEWLINE;
            token_break = true;
        } else if (tzr->check_comm) {
            switch (ch) {
            case L'*':
                next_state = KDL_SEQ_C_COMM;
                break;
            case L'/':
                next_state = KDL_SEQ_CPP_COMM;
                break;
            case L'-':
                if (tzr->index == 1)
                    next_state = KDL_SEQ_SD_COMM;
                break;
            }
        } else if (!ch) {
            next_state = KDL_SEQ_EOF;
            token_break = true;
        } else if (ch == L'/') {
            tzr->check_comm = 1;
        }

        break;
    case KDL_SEQ_CPP_COMM:
        if (is_newline(ch))
            next_state = KDL_SEQ_NEWLINE;

        break;
    case KDL_SEQ_C_COMM:
        if (ch == L'/' && last_char(tzr) == L'*')
            next_state = KDL_SEQ_WHITESPACE;

        // TODO handle nested c comments

        break;
    default:
        KDL_UNIMPL("unhandled tokenizer state\n");

        break;
    }

    // save tokens
    switch (next_state) {
    case KDL_SEQ_CHARACTER:
    case KDL_SEQ_STRING:
    case KDL_SEQ_RAW_STRING:
        tzr->token[tzr->index++] = ch;

        break;
    default:
        break;
    }

    // break tokens
    if (token_break && next_state != KDL_SEQ_SD_COMM) {
        tzr->token[tzr->index] = L'\0';

        wprintf(L"TOKEN: \"%ls\"\n", tzr->token);

        tzr->index = 0;
    }

    tzr->state = next_state;
}

void kdl_tokenizer_feed(kdl_tokenizer_t *tzr, wchar_t *data, size_t length) {
    for (size_t i = 0; i < length && tzr->state != KDL_SEQ_EOF; ++i) {
        process_char(tzr, data[i]);
    }
}
