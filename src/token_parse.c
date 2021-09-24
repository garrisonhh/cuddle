#include <cuddle/meta.h>
#include <cuddle/tokenize.h>

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
