#ifndef KDL_TOKENS_H
#define KDL_TOKENS_H

#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>

#ifndef KDL_TOKEN_BUFFER
#define KDL_TOKEN_BUFFER (2048)
#endif

#define KDL_TOKEN_TYPES_X\
    X(KDL_TOK_UNIDENTIFIED),\
    X(KDL_TOK_BREAK),\
    X(KDL_TOK_NODE),\
    X(KDL_TOK_PROPERTY),\
    X(KDL_TOK_CHILD_BEGIN),\
    X(KDL_TOK_CHILD_END),\
    X(KDL_TOK_STRING),\
    X(KDL_TOK_RAW_STRING),\
    X(KDL_TOK_NUMBER),\
    X(KDL_TOK_BOOL),\
    X(KDL_TOK_NULL)

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
    X(KDL_SEQ_LONG_RAW_STR, 1)

#define X(name) name
typedef enum kdl_token_type {
    KDL_TOKEN_TYPES_X
} kdl_token_type_e;
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

    // state
    wchar_t last_chars[2]; // used for detecting char sequences
    kdl_tokenizer_state_e state;

    unsigned token_break: 1;
    unsigned node_break: 1;
} kdl_tokenizer_t;

typedef struct kdl_token {
    kdl_token_type_e type;

    union kdl_token_data {
        wchar_t str[KDL_TOKEN_BUFFER];
        double num;
        bool _bool;
    } data;
} kdl_token_t;

// initialize a tokenizer context
void kdl_tokenizer_make(kdl_tokenizer_t *);

// pass in 
void kdl_tokenizer_feed(kdl_tokenizer_t *, wchar_t *data, size_t length);
bool kdl_tokenizer_next_token(kdl_tokenizer_t *, kdl_token_t *);

#endif
