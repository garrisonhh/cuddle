#include <math.h>

#include <cuddle/meta.h>
#include <cuddle/tokenize.h>

static void parse_escaped_string(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    size_t index = 0;
    wchar_t *trav = tzr->buf + 1;

    while (*trav && *trav != L'"') {
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

static void parse_raw_string(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    // chop off ending quote
    size_t i;

    for (i = tzr->buf_len; tzr->buf[i] != L'"'; --i)
        ;

    tzr->buf[i] = L'\0';

    // copy from after beginning quote
    wcscpy(token->string, tzr->buf + 1);
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

// returns if number was valid
// TODO binary, octal, hexadecimal numbers
static bool parse_number(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    wchar_t *trav = tzr->buf;

    // integral value
    double sign = parse_sign(&trav);
    double number = parse_integral(&trav) * sign;

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
        number += fractional * sign;

        // exponents
        if (*trav == L'e' || *trav == L'E') {
            ++trav;

            number *= pow(
                10.0,
                parse_sign(&trav) * (double)parse_integral(&trav)
            );
        }
    }

    // return validity
    if (!*trav) {
        token->number = number;

        return true;
    }

    return false;
}

static void type_characters_token(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    switch (tzr->buf[0]) {
    case L't':
        token->type = KDL_TOK_BOOL;
        token->boolean = true;

        return;
    case L'f':
        token->type = KDL_TOK_BOOL;
        token->boolean = false;

        return;
    case L'n':
        token->type = KDL_TOK_NULL;

        return;
    default:
        if (parse_number(tzr, token)) {
            token->type = KDL_TOK_NUMBER;

            return;
        }

        KDL_ERROR("unknown value!!!\n");
    }
}

void generate_token(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    // reset token
    token->node = token->property = false;

    // find token type and parse
    switch (tzr->last_state) {
    case KDL_SEQ_STRING:
        token->type = KDL_TOK_STRING;
        parse_escaped_string(tzr, token);

        break;
    case KDL_SEQ_RAW_STR:
        token->type = KDL_TOK_STRING;
        parse_raw_string(tzr, token);

        break;
    case KDL_SEQ_CHARACTER:
        if (tzr->state == KDL_SEQ_ASSIGNMENT || tzr->expect_node) {
            token->type = KDL_TOK_IDENTIFIER;
            wcscpy(token->string, tzr->buf);
        } else {
            type_characters_token(tzr, token);

            return;
        }

        break;
    case KDL_SEQ_CHILD_BEGIN:
        token->type = KDL_TOK_CHILD_BEGIN;

        return;
    case KDL_SEQ_CHILD_END:
        token->type = KDL_TOK_CHILD_END;

        return;
    default:
        KDL_ERROR("idek %s\n", KDL_TOKENIZER_STATES[tzr->last_state]);
    }

    // flags
    if (tzr->expect_node) {
        tzr->expect_node = false;
        token->node = true;
    } else if (tzr->state == KDL_SEQ_ASSIGNMENT) {
        token->property = true;
    }
}
