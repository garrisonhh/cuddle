#ifndef KDL_TOKENS_H
#define KDL_TOKENS_H

#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>

#ifndef KDL_TOKEN_BUFFER
#define KDL_TOKEN_BUFFER (2048)
#endif

#define KDL_TOKEN_TYPES_X\
    X(KDL_TOK_NODE_ID),\
    X(KDL_TOK_PROP_ID),\
    X(KDL_TOK_CHILD_BEGIN),\
    X(KDL_TOK_CHILD_END),\
    X(KDL_TOK_STRING),\
    X(KDL_TOK_NUMBER),\
    X(KDL_TOK_BOOL),\
    X(KDL_TOK_NULL)

#define KDL_TOKEN_EXPECT_X\
    X(KDL_EXP_NODE_ID),\
    X(KDL_EXP_ATTRIBUTE /* arg, prop, or child_begin */),\
    X(KDL_EXP_PROP_VALUE)

/*
 * table of (name, stores_data)
 *
 * sequences generally alternate between whitespace and character, and then are
 * dynamically detected to be other types.
 *
 * slashdash comments aren't included here because they are detected after
 * a character sequence token break!
 */
#define KDL_TOKENIZER_STATES_X\
    X(KDL_SEQ_WHITESPACE,   0),\
    X(KDL_SEQ_CHARACTER,    1),\
    X(KDL_SEQ_BREAK,        0),\
    X(KDL_SEQ_C_COMM,       0),\
    X(KDL_SEQ_CPP_COMM,     0),\
    X(KDL_SEQ_STRING,       1),\
    X(KDL_SEQ_RAW_STR,      1),\
    X(KDL_SEQ_LONG_RAW_STR, 1),\
    X(KDL_SEQ_ANNOTATION,   0)

#define X(name) name
typedef enum kdl_token_type {
    KDL_TOKEN_TYPES_X
} kdl_token_type_e;

typedef enum kdl_token_expect {
    KDL_TOKEN_EXPECT_X
} kdl_token_expect_e;
#undef X

#define X(name, stores_data) name
typedef enum kdl_token_sequence_state {
    KDL_TOKENIZER_STATES_X
} kdl_tokenizer_state_e;
#undef X

typedef struct kdl_tokenizer {
    // current data
    wchar_t *data;
    size_t data_len, data_idx;

    // token buffer
    wchar_t buf[KDL_TOKEN_BUFFER];
    size_t buf_len;

    // parsing state
    wchar_t last_chars[2]; // used for detecting char sequences
    kdl_tokenizer_state_e state;

    unsigned token_break: 1; // true at end of token
    unsigned node_break: 1; // true at end of line (escapes are handled later!!)
    unsigned prop_name: 1; // true if token is a prop name
    unsigned: 0;

    // token typing state
    kdl_token_expect_e expects;

    unsigned discard_break: 1;
} kdl_tokenizer_t;

typedef struct kdl_token {
    kdl_token_type_e type;

    union kdl_token_data {
        wchar_t string[KDL_TOKEN_BUFFER];
        double number;
        bool boolean;
    } data;
} kdl_token_t;

void kdl_tokenizer_make(kdl_tokenizer_t *);

void kdl_tokenizer_feed(kdl_tokenizer_t *, wchar_t *data, size_t length);
bool kdl_tokenizer_next_token(kdl_tokenizer_t *, kdl_token_t *);

#endif
