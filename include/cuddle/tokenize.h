#ifndef KDL_TOKENS_H
#define KDL_TOKENS_H

#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>

#define KDL_TOKEN_TYPES_X\
    /* values */\
    X(KDL_TOK_IDENTIFIER),\
    X(KDL_TOK_STRING),\
    X(KDL_TOK_NUMBER),\
    X(KDL_TOK_BOOL),\
    X(KDL_TOK_NULL),\
    /* symbols */\
    X(KDL_TOK_CHILD_BEGIN),\
    X(KDL_TOK_CHILD_END)

// table of (name, tentative type)
#define KDL_TOKENIZER_STATES_X\
    X(KDL_SEQ_WHITESPACE),\
    X(KDL_SEQ_CHARACTER),\
    /* symbols */\
    X(KDL_SEQ_BREAK),\
    X(KDL_SEQ_BREAK_ESC),\
    X(KDL_SEQ_ASSIGNMENT),\
    X(KDL_SEQ_CHILD_BEGIN),\
    X(KDL_SEQ_CHILD_END),\
    /* toggled states */\
    X(KDL_SEQ_STRING),\
    X(KDL_SEQ_RAW_STR),\
    X(KDL_SEQ_C_COMM),\
    X(KDL_SEQ_CPP_COMM),\
    X(KDL_SEQ_ANNOTATION)

#define X(name) name
typedef enum kdl_token_type {
    KDL_TOKEN_TYPES_X
} kdl_token_type_e;

typedef enum kdl_tokenizer_state {
    KDL_TOKENIZER_STATES_X
} kdl_tokenizer_state_e;
#undef X

// stringified enums for error messages, debugging, etc
extern const char KDL_TOKEN_TYPES[][32];
extern const char KDL_TOKENIZER_STATES[][32];

typedef struct kdl_tokenizer {
    // current data
    wchar_t *data;
    size_t data_len, data_idx;

    // token buffer
    wchar_t *buf;
    size_t buf_len; // this is current length, not total memory length

    // parsing state
    wchar_t last_char; // used for detecting char sequences
    kdl_tokenizer_state_e state, last_state;

    int c_comm_level; // for stacked c comments
    int raw_count, raw_current; // for counting raw string '#'s

    unsigned force_detect: 1;
    unsigned reset_buf: 1;
    unsigned token_break: 1;

    // token typing state
    unsigned break_escape: 1;
    unsigned expect_node: 1;
    unsigned slashdash_node: 1;
    unsigned slashdash_prop: 1;
} kdl_tokenizer_t;

typedef struct kdl_token {
    kdl_token_type_e type;

    wchar_t *string;
    size_t str_len;
    double number;
    unsigned boolean: 1;

    // set to true for identifiers/strings marking a node or prop
    unsigned node: 1;
    unsigned property: 1;
} kdl_token_t;

void kdl_tokenizer_make(kdl_tokenizer_t *, wchar_t *buffer);
void kdl_token_make(kdl_token_t *, wchar_t *buffer);

void kdl_tok_feed(kdl_tokenizer_t *, wchar_t *data, size_t length);
bool kdl_tok_next(kdl_tokenizer_t *, kdl_token_t *);

#endif
