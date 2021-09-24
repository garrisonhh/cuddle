#include <math.h>

#include <cuddle/meta.h>
#include <cuddle/tokens.h>

#define X(name) #name
const char KDL_TOKEN_TYPES[][32] = { KDL_TOKEN_TYPES_X };
const char KDL_TOKEN_EXPECTS[][32] = { KDL_TOKEN_EXPECT_X };
const char TOKENIZER_STATES[][32] = { KDL_TOKENIZER_STATES_X };
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
        return true;

    default:
        return ch >= 0x2000 && ch <= 0x200A;

    }
}

static bool is_break(wchar_t ch) {
    switch (ch) {
    case 0x000A:
    case 0x000C:
    case 0x000D:
    case 0x0085:
    case 0x2028:
    case 0x2029:
    case L';':
    case (wchar_t)WEOF:
        return true;

    default:
        return false;

    }
}

// assumes that tokenizer isn't in a special sequence (like a string or comment)
static kdl_tokenizer_state_e detect_next_state(
    kdl_tokenizer_t *tzr, wchar_t ch
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
    case L'"': return KDL_SEQ_STRING;
    case L'(': return KDL_SEQ_ANNOTATION;
    }

    // multi char mappings
    if (tzr->state == KDL_SEQ_CHARACTER) {
        // TODO
    }
    
    // unidentified character, must be identifier or something
    return KDL_SEQ_CHARACTER;
}

/*
 * this function's responsibilities are limited exclusively to splitting tokens
 * up through the tokenizer state machine. anything else is out of scope.
 */
static void consume_char(kdl_tokenizer_t *tzr) {
    // reset flags sent to tok_next()
    tzr->token_break = false;

    if (tzr->reset_buf) {
        tzr->reset_buf = false;
        tzr->buf_len = 0;
    }

    // detect state changes
    wchar_t ch = tzr->data[tzr->data_idx++];
    kdl_tokenizer_state_e next_state = tzr->state;

    switch (tzr->state) {
    default:
        next_state = detect_next_state(tzr, ch);

        break;
    case KDL_SEQ_C_COMM:
    case KDL_SEQ_CPP_COMM:
    case KDL_SEQ_STRING:
    case KDL_SEQ_RAW_STR:
    case KDL_SEQ_ANNOTATION:
        KDL_UNIMPL("encountered sequence\n");

        break;
    }

    // act on state change
    if (next_state != tzr->state) {
        tzr->reset_buf = 1;

        switch (tzr->state) {
        case KDL_SEQ_CHARACTER:
        case KDL_SEQ_STRING:
        case KDL_SEQ_RAW_STR:
            tzr->token_break = true;

            break;
        default:
            break;
        }
    }

    // store token
    tzr->buf[tzr->buf_len] = tzr->last_char;
    tzr->buf[++tzr->buf_len] = L'\0';

    // store state
    tzr->state = next_state;
    tzr->last_char = ch;
}

#if 0
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

                    unsigned diff = *trav - L'0'; // unsigned to avoid >= 0 cmp

                    if (diff < 10) {
                        ch *= 16;
                        ch += diff;
                    } else if ((diff = *trav - L'A') < 6) {
                        ch *= 16;
                        ch += diff + 10;
                    } else {
                        --trav;

                        break;
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

    *trav += **trav == L'-' || **trav == L'+';

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

// TODO binary, octal, hexadecimal numbers
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

static void parse_raw_string(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    // find beginning of raw string
    size_t start = 0;

    while (tzr->buf[start++] != L'"')
        ;

    // find end of string
    size_t end = tzr->buf_len;

    while (tzr->buf[--end] != L'"')
        ;

    // chop end of string and strcpy
    tzr->buf[end] = L'\0';
    wcscpy(token->string, tzr->buf + start);
}

// detect and parse string types
// returns whether string is found
static bool find_str(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    size_t index;

    switch (tzr->buf[0]) {
    case L'"':
        token->type = KDL_TOK_STRING;
        parse_escaped_string(tzr, token);

        return true;
    case L'r':
        // check for a sequence of '#' terminated by a '"'
        index = 0;

        while (tzr->buf[++index] == L'#')
            ;

        if (tzr->buf[index] == L'"') {
            token->type = KDL_TOK_STRING;
            parse_raw_string(tzr, token);

            return true;
        }
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
            // generate error message
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
#endif

/*
 * fills in 'token' with data of next token and returns true, or returns false
 * if the current token isn't finished yet. this lets you use the idiom:
 *
 * while (kdl_tok_next()) { [ do something with token... ] }
 *
 * this functions responsibilities are limited to sanitizing the raw token
 * stream from the state machine (processing line break escapes, slashdashes,
 * etc.), managing expectations, and returning fully typed and usable tokens.
 *
 * TODO properly parse slashdash comments for nodes and properties
 */
bool kdl_tok_next(kdl_tokenizer_t *tzr, __attribute__((unused)) kdl_token_t *token) {
    while (tzr->data_idx < tzr->data_len && tzr->data[tzr->data_idx]) {
        consume_char(tzr);

        if (tzr->token_break)
            return true;
    }

    return false;
}
