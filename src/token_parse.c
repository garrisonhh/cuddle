#include <math.h>

#include <cuddle/meta.h>
#include <cuddle/tokenize.h>

static void append_u8ch() {
    ;
}

static void parse_escaped_string(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    size_t index = 0;
    kdl_u8ch_t *trav = tzr->buf + 1;

    while (*trav && *trav != L'"') {
        if (*trav == L'\\') {
            kdl_u8ch_t ch; // for unicode sequences

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
    kdl_utf8_copy(token->string, tzr->buf + 1);
}

static inline long parse_sign(kdl_u8ch_t **trav) {
    long sign = **trav == L'-' ? -1 : 1;

    *trav += **trav == L'-' || **trav == L'+';

    return sign;
}

static long parse_num_base(kdl_u8ch_t **trav, int base) {
    long integral = 0;

    while (1) {
        if (**trav >= L'0' && **trav < L'0' + base) {
            integral *= base;
            integral += **trav - L'0';
        } else if (**trav != L'_') {
            return integral;
        }

        ++*trav;
    }
}

static long parse_hex(kdl_u8ch_t **trav) {
    long integral = 0;

    while (1) {
        // TODO this could definitely be a lot cleaner
        if (**trav >= L'0' && **trav <= L'9') {
            integral *= 16;
            integral += **trav - L'0';
        } else if (**trav >= L'A' && **trav <= L'F') {
            integral *= 16;
            integral += 10 + **trav - L'A';
        } else if (**trav >= L'a' && **trav <= L'f') {
            integral *= 16;
            integral += 10 + **trav - L'a';
        } else if (**trav != L'_') {
            return integral;
        }

        ++*trav;
    }
}

static double parse_dec(kdl_u8ch_t **trav) {
    double number = parse_num_base(trav, 10);

    // fractional values
    if (**trav == L'.') {
        double fractional = 0.0, multiplier = 0.1;

        ++*trav;

        while (**trav != L'e' && **trav != L'E' && **trav) {
            if (**trav != L'_') {
                fractional += (double)(**trav - L'0') * multiplier;
                multiplier *= 0.1;
            }

            ++*trav;
        }

        // number before exponentiation
        number += fractional;

        // exponents
        if (**trav == L'e' || **trav == L'E') {
            ++*trav;

            number *= pow(
                10.0,
                parse_sign(trav) * (double)parse_num_base(trav, 10)
            );
        }
    }

    return number;
}

// returns if number was valid
static bool parse_number(kdl_tokenizer_t *tzr, kdl_token_t *token) {
    kdl_u8ch_t *trav = tzr->buf;
    double sign = parse_sign(&trav);
    double number;

    if (*trav == L'0') {
        switch (*(trav + 1)) {
        case L'x':
            trav += 2;
            number = parse_hex(&trav);

            break;
        case L'b':
            trav += 2;
            number = parse_num_base(&trav, 2);

            break;
        case L'o':
            trav += 2;
            number = parse_num_base(&trav, 8);

            break;
        default:
            number = parse_dec(&trav);

            break;
        }
    } else {
        number = parse_dec(&trav);
    }

    number *= sign;

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

        // TODO replacement for wprintf here for better debugging
        KDL_ERROR("encountered an unknown value!");
    }
}

void generate_token(kdl_tokenizer_t *tzr, kdl_token_t *token) {
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

            kdl_utf8_copy(token->string, tzr->buf);
        } else {
            type_characters_token(tzr, token);

            break;
        }

        break;
    case KDL_SEQ_CHILD_BEGIN:
        token->type = KDL_TOK_CHILD_BEGIN;

        break;
    case KDL_SEQ_CHILD_END:
        token->type = KDL_TOK_CHILD_END;

        break;
    default:
        KDL_ERROR(
            "wtf is this?? token parse retrieved a %s\n",
            KDL_TOKENIZER_STATES[tzr->last_state]
        );
    }

    // flags
    if (token->type == KDL_TOK_STRING || token->type == KDL_TOK_IDENTIFIER) {
        token->node = tzr->expect_node;
        tzr->expect_node = false;
    } else {
        token->node = false;
    }

    token->property = tzr->state == KDL_SEQ_ASSIGNMENT;
}
