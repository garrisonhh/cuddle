#include <cuddle/meta.h>
#include <cuddle/tokens.h>

#define X(name) #name
const char TOKEN_TYPES[][32] = { KDL_TOKEN_TYPES_X };
#undef X

#define X(name, stores_data) #name
const char TOKENIZER_STATES[][32] = { KDL_TOKENIZER_STATES_X };
#undef X

#define X(name, stores_data) stores_data
const bool TOKENIZER_STATE_STORES[] = { KDL_TOKENIZER_STATES_X };
#undef X

void kdl_tokenizer_feed(kdl_tokenizer_t *tzr, wchar_t *data, size_t length) {
    tzr->data = data;
    tzr->data_len = length;
    tzr->data_idx = 0;
}

static bool is_whitespace(wchar_t ch) {
    switch (ch) {
    case 0x0009:
    case 0x0020:
    case 0x00A0:
    case 0x1680:
    case 0x202F:
    case 0x205F:
    case 0x3000:
    case L'=': // yes, this looks weird but it works I promise!
        return true;

    default:
        return ch >= 0x2000 && ch <= 0x200A;

    }
}

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

// might treat CRLF as two breaks?
static inline bool is_break(wchar_t ch) {
    return is_newline(ch) || ch == L';' || ch == WEOF;
}

// TODO pass a block of memory in for the buffer instead of #defining a size
void kdl_tokenizer_make(kdl_tokenizer_t *tzr) {
    *tzr = (kdl_tokenizer_t){0};
}

/*
 * this function's responsibilities are limited exclusively to splitting tokens
 * up. anything else is out of scope. since tokenizing is a pretty complex state
 * machine, it's super important that this function stays as tight as possible.
 */
static void consume_char(kdl_tokenizer_t *tzr) {
    wchar_t ch = tzr->data[tzr->data_idx++];

    // process and reset persistent flags
    if (tzr->token_break) {
        tzr->buf_len = 0;
        tzr->prop_name = false;

        tzr->token_break = false;
    }

    tzr->node_break = false;

    // determine next state
    kdl_tokenizer_state_e next_state = tzr->state;

    switch (next_state) {
    case KDL_SEQ_BREAK:
        next_state = KDL_SEQ_WHITESPACE;

        /* fallthru */
    case KDL_SEQ_WHITESPACE:
        if (!is_whitespace(ch))
            next_state = is_break(ch) ? KDL_SEQ_BREAK : KDL_SEQ_CHARACTER;

        break;
    case KDL_SEQ_CHARACTER:
        if (is_whitespace(ch))
            next_state = KDL_SEQ_WHITESPACE;
        else if (is_break(ch))
            next_state = KDL_SEQ_BREAK;

        break;
    case KDL_SEQ_C_COMM:
        if (ch == L'/' && tzr->last_chars[1] == L'*')
            next_state = KDL_SEQ_WHITESPACE;

        break;
    case KDL_SEQ_CPP_COMM:
        if (is_newline(ch))
            next_state = KDL_SEQ_WHITESPACE;

        break;
    case KDL_SEQ_STRING:
        if (ch == L'"' && tzr->last_chars[1] != L'\\')
            next_state = KDL_SEQ_WHITESPACE;

        break;
    case KDL_SEQ_RAW_STR:
        if (ch == L'"')
            next_state = KDL_SEQ_WHITESPACE;

        break;
    case KDL_SEQ_LONG_RAW_STR:
        if (ch == L'#' && tzr->last_chars[1] == L'"')
            next_state = KDL_SEQ_WHITESPACE;

        break;
    case KDL_SEQ_ANNOTATION:
        if (ch == L')')
            next_state = KDL_SEQ_WHITESPACE;

        break;
    }

    // detect special characters
    if (next_state == KDL_SEQ_CHARACTER) {
        if (ch == L'"') {
            // detect string types
            if (tzr->last_chars[0] == L'r' && tzr->last_chars[1] == L'#')
                next_state = KDL_SEQ_LONG_RAW_STR;
            else if (tzr->last_chars[1] == L'r')
                next_state = KDL_SEQ_RAW_STR;
            else
                next_state = KDL_SEQ_STRING;
        } else if (tzr->last_chars[1] == L'/') {
            // detect comments
            if (ch == L'/')
                next_state = KDL_SEQ_CPP_COMM;
            else if (ch == L'*')
                next_state = KDL_SEQ_C_COMM;
        } else if (ch == L'(') {
            // type annotations
            next_state = KDL_SEQ_ANNOTATION;
        }
    } else if (next_state == KDL_SEQ_WHITESPACE && ch == L'=') {
        tzr->prop_name = true;
    }

    // act upon state transition
    if (next_state != tzr->state) {
        // flags
        tzr->token_break = TOKENIZER_STATE_STORES[tzr->state]
                        && !TOKENIZER_STATE_STORES[next_state];
        tzr->node_break = next_state == KDL_SEQ_BREAK;

        // remove the last '/' from buf when encountering a comment
        if (tzr->state == KDL_SEQ_CHARACTER
         && (next_state == KDL_SEQ_C_COMM || next_state == KDL_SEQ_CPP_COMM)) {
            // if buffer length is now zero, remove the token break so zero
            // length tokens aren't produced
            if (!--tzr->buf_len)
                tzr->token_break = false;
        }
    }

    // act upon new state
    if (TOKENIZER_STATE_STORES[next_state])
        tzr->buf[tzr->buf_len++] = ch;

    if (tzr->token_break)
        tzr->buf[tzr->buf_len] = L'\0';

    // set persistent stuff for the next iteration
    tzr->state = next_state;

    tzr->last_chars[0] = tzr->last_chars[1];
    tzr->last_chars[1] = ch;
}

static inline bool is_slashdashed(kdl_tokenizer_t *tzr) {
    return tzr->buf_len >= 2 && tzr->buf[0] == L'/' && tzr->buf[1] == L'-';
}

// types and parses individual tokens
static void generate_token(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    // detect token type
    switch (tzr->state) {
    case KDL_SEQ_STRING:
        token->type = KDL_TOK_STRING;
    }

    // save data
    // TODO for real
    wcscpy(token->data.str, tzr->buf);
}

/*
 * fills in 'token' with data of next token and returns true, or returns false
 * if the current token isn't finished yet. this lets you use the idiom:
 *
 * while (next_token()) { [ do something with token... ] }
 *
 * this functions responsibilities are limited to sanitizing the raw token
 * stream from the state machine (processing line break escapes, slashdashes,
 * etc.), managing expectations, and returning fully typed and usable tokens.
 */
bool kdl_tokenizer_next_token(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    while (tzr->data_idx < tzr->data_len) {
        // churn the state machine
        consume_char(tzr);

        // process a new potential token
        if (tzr->token_break) {
            if (tzr->buf_len == 1 && tzr->buf[0] == L'\\') {
                // escape node break
                tzr->discard_break = true;
            } else if (!is_slashdashed(tzr)) {
                // generate a token
                generate_token(tzr, token);

                wprintf(L"|%ls| ", tzr->buf);

                // set next state
                switch (tzr->expects) {
                case KDL_EXP_NODE_ID:
                    tzr->expects = KDL_EXP_ATTRIBUTE;

                    break;
                case KDL_EXP_ATTRIBUTE:
                    if (tzr->prop_name)
                        tzr->expects = KDL_EXP_PROP_VALUE;

                    break;
                }

                if (tzr->buf_len == 1 && tzr->buf[0] == L'{')
                    wprintf(L"\n");

                return true;
            }
        }

        // process a node break
        if (tzr->node_break) {
            if (tzr->discard_break)
                tzr->discard_break = false;
            else {
                tzr->expects = KDL_EXP_NODE_ID;

                wprintf(L"\n");
            }
        }
    }

    return false;
}
