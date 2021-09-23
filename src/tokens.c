#include <math.h>

#include <cuddle/meta.h>
#include <cuddle/tokens.h>

#define X(name) #name
const char KDL_TOKEN_TYPES[][32] = { KDL_TOKEN_TYPES_X };
const char KDL_TOKEN_EXPECTS[][32] = { KDL_TOKEN_EXPECT_X };
#undef X

#define X(name, stores_data) #name
const char TOKENIZER_STATES[][32] = { KDL_TOKENIZER_STATES_X };
#undef X

#define X(name, stores_data) stores_data
const bool TOKENIZER_STATE_STORES[] = { KDL_TOKENIZER_STATES_X };
#undef X

void kdl_tokenizer_make(kdl_tokenizer_t *tzr, wchar_t *buffer) {
    *tzr = (kdl_tokenizer_t){
        .buf = buffer
    };
}

void kdl_token_make(kdl_token_t *token, wchar_t *buffer) {
    *token = (kdl_token_t){
        .string = buffer
    };
}

void kdl_tok_feed(kdl_tokenizer_t *tzr, wchar_t *data, size_t length) {
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
    return is_newline(ch) || ch == L';' || ch == (wchar_t)WEOF;
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

// whether current token is slashdashed
static inline bool is_slashdashed(kdl_tokenizer_t *tzr) {
    return tzr->buf_len >= 2 && tzr->buf[0] == L'/' && tzr->buf[1] == L'-';
}

static void parse_escaped_string(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    size_t index = 0;
    wchar_t *trav = tzr->buf + 1;

    while (*trav) {
        if (*trav == L'\\') {
            wchar_t ch; // for unicode sequences

            ++trav;

            // handle string escape
            switch (*trav) {
#define ESC_CASE(ch, esc) case ch: token->string[index++] = esc; break
            ESC_CASE(L'n', L'\n');
            ESC_CASE(L'r', L'\r');
            ESC_CASE(L't', L'\t');
            ESC_CASE(L'\\', L'\\');
            ESC_CASE(L'/', L'/');
            ESC_CASE(L'"', L'"');
            ESC_CASE(L'b', L'\b');
            ESC_CASE(L'f', L'\f');
#undef ESC_CASE
            case L'u':
                // parse unicode escape sequence
                ch = 0;

                for (size_t i = 0; i < 6; ++i) {
                    ++trav;

                    int diff = *trav - L'0';

                    if (diff >= 0 && diff < 10) {
                        ch *= 16;
                        ch += diff;
                    } else {
                        diff = *trav - L'A';

                        if (diff >= 0 && diff < 6) {
                            ch *= 16;
                            ch += diff + 10;
                        } else {
                            --trav;

                            break;
                        }
                    }
                }

                token->string[index++] = ch;

                break;
            default:
                token->string[index++] = *trav;

                break;
            }
        } else {
            token->string[index++] = *trav;
        }

        ++trav;
    }

    token->string[index] = L'\0';
}

static inline long parse_sign(wchar_t **trav) {
    long sign = **trav == L'-' ? -1 : 1;

    *trav += (**trav == L'-' || **trav == L'+');

    return sign;
}

// used to parse until first decimal and to parse exponents
static long parse_integral(wchar_t **trav) {
    long sign = parse_sign(trav), integral = 0;

    while (**trav != L'.' && **trav) {
        if (**trav != L'_') {
            integral *= 10;
            integral += **trav - L'0';
        }

        ++*trav;
    }

    return sign * integral;
}

static void parse_number(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    wchar_t *trav = tzr->buf;

    // integral value
    double sign = parse_sign(&trav);

    token->number = parse_integral(&trav) * sign;

    // fractional values
    if (*trav == L'.') {
        double fractional = 0.0, multiplier = 0.1;

        ++trav;

        while (*trav != L'e' && *trav != L'E' && *trav) {
            if (*trav != L'_') {
                fractional += (double)(*trav - L'0') * multiplier;
                multiplier *= 0.1;
            }

            ++trav;
        }

        // number before exponentiation
        token->number += fractional * sign;

        // exponents
        if (*trav == L'e' || *trav == L'E') {
            ++trav;

            token->number *= pow(
                10.0,
                parse_sign(&trav) * (double)parse_integral(&trav)
            );
        }
    }

}

// detect and parse string types
// returns whether string is found
static bool find_str(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    switch (tzr->buf[0]) {
    case L'"':
        token->type = KDL_TOK_STRING;
        parse_escaped_string(tzr, token);

        return true;
    case L'r':
        if (tzr->buf[0] == L'#' || tzr->buf[1] == L'"')
            KDL_UNIMPL("we aren't parsing raw strings yet!\n");
    }

    return false;
}

// detect and parse all value types
static void find_value(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    if (find_str(tzr, token))
        return;

    switch (tzr->buf[0]) {
    case L'-':
        if (tzr->buf_len == 1) {
            token->type = KDL_TOK_NULL;

            break;
        }

        goto found_number;
    default:
        if (tzr->buf[0] < L'0' || tzr->buf[0] > L'9') {
            const size_t scratch_size = 256;
            char scratch[scratch_size + 1];

            wcstombs(scratch, tzr->buf, scratch_size);
            scratch[scratch_size] = 0;

            KDL_ERROR("unknown token: \"%s\"\n", scratch);
        }

        /* fallthru */
    case L'+':
    found_number:
        token->type = KDL_TOK_NUMBER;
        parse_number(tzr, token);

        break;
    case L't':
        token->type = KDL_TOK_BOOL;
        token->boolean = true;

        break;
    case L'f':
        token->type = KDL_TOK_BOOL;
        token->boolean = false;

        break;
    case L'n':
        token->type = KDL_TOK_NULL;

        break;
    }
}

// types and parses individual tokens
static void generate_token(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    // child ends/beginnings
    if (tzr->buf_len == 1) {
        if (tzr->buf[0] == L'{') {
            token->type = KDL_TOK_CHILD_BEGIN;

            return;
        } else if (tzr->buf[0] == L'}') {
            token->type = KDL_TOK_CHILD_END;

            return;
        }
    }

    // detect token type
    switch (tzr->expects) {
    case KDL_EXP_NODE_ID:
        token->type = KDL_TOK_NODE_ID;

        break;
    case KDL_EXP_ATTRIBUTE:
        if (tzr->prop_name) {
            token->type = KDL_TOK_PROP_ID;

            break;
        }

        /* fallthru */
    case KDL_EXP_PROP_VALUE:
        // detect value type
        find_value(tzr, token);

        break;
    }

    // if id is actually a string, parse it
    if (token->type == KDL_TOK_NODE_ID || token->type == KDL_TOK_PROP_ID)
        if (!find_str(tzr, token))
            wcscpy(token->string, tzr->buf); // must be identifier, no parsing
}

/*
 * fills in 'token' with data of next token and returns true, or returns false
 * if the current token isn't finished yet. this lets you use the idiom:
 *
 * while (kdl_tok_next()) { [ do something with token... ] }
 *
 * this functions responsibilities are limited to sanitizing the raw token
 * stream from the state machine (processing line break escapes, slashdashes,
 * etc.), managing expectations, and returning fully typed and usable tokens.
 */
bool kdl_tok_next(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    while (tzr->data_idx < tzr->data_len) {
        // churn the state machine
        consume_char(tzr);

        // process a new potential token
        bool generated_token = false;

        if (tzr->token_break) {
            if (tzr->buf_len == 1 && tzr->buf[0] == L'\\') {
                // escape node break
                tzr->discard_break = true;
            } else if (!is_slashdashed(tzr)) {
                // generate a token
                generate_token(tzr, token);

                // set next state
                switch (tzr->expects) {
                case KDL_EXP_PROP_VALUE:
                case KDL_EXP_NODE_ID:
                    tzr->expects = KDL_EXP_ATTRIBUTE;

                    break;
                case KDL_EXP_ATTRIBUTE:
                    if (tzr->prop_name)
                        tzr->expects = KDL_EXP_PROP_VALUE;

                    break;
                }

                generated_token = true;
            }
        }

        // process a node break
        if (tzr->node_break) {
            if (tzr->discard_break)
                tzr->discard_break = false;
            else
                tzr->expects = KDL_EXP_NODE_ID;
        }

        if (generated_token)
            return true;

        // break at NUL
        if (!tzr->data[tzr->data_idx])
            break;
    }

    return false;
}
