#ifndef KDL_TOKENS_H
#define KDL_TOKENS_H

#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>

#ifndef KDL_TOKEN_BUFFER
#define KDL_TOKEN_BUFFER (512)
#endif

#define KDL_TOKEN_TYPES_X\
    X(KDL_TOK_UNIDENTIFIED),\
    X(KDL_TOK_BREAK),\
    X(KDL_TOK_NODE),\
    X(KDL_TOK_PROPERTY),\
    X(KDL_TOK_ARGUMENT),\
    X(KDL_TOK_CHILD_BEGIN),\
    X(KDL_TOK_CHILD_END),\
    X(KDL_TOK_STRING),\
    X(KDL_TOK_RAW_STRING),\
    X(KDL_TOK_NUMBER),\
    X(KDL_TOK_TRUE),\
    X(KDL_TOK_FALSE),\
    X(KDL_TOK_NULL)

#define KDL_TOKENIZER_STATES_X\
    X(KDL_SEQ_WHITESPACE),\
    X(KDL_SEQ_NEWLINE),\
    X(KDL_SEQ_EOF),\
    X(KDL_SEQ_C_COMM),\
    X(KDL_SEQ_CPP_COMM),\
    X(KDL_SEQ_SD_COMM),\
    X(KDL_SEQ_CHARACTER),\
    X(KDL_SEQ_RAW_STRING),\
    X(KDL_SEQ_STRING)

#define X(value) value
typedef enum kdl_token_type {
    KDL_TOKEN_TYPES_X
} kdl_token_type_e;

typedef enum kdl_token_sequence_state {
    KDL_TOKENIZER_STATES_X
} kdl_tokenizer_state_e;
#undef X

typedef struct kdl_tokenizer {
    // tokens are built character by character using this buffer
    wchar_t token[KDL_TOKEN_BUFFER];
    size_t len_token;

    // token types are determined after breakage
    kdl_token_type_e token_type;

    // tokenizer state
    kdl_tokenizer_state_e state;
    unsigned token_break: 1; // whenever a token is completed
    unsigned check_comm: 1; // when a '/' character is found
    unsigned str_escape: 1; // when a '\' is found in a string 
    unsigned save_token: 1; // when state saves tokens
    unsigned reset_index: 1; // reset index on next process_char()
} kdl_tokenizer_t;

// just zero-inits the tokenizer
void kdl_tokenizer_make(kdl_tokenizer_t *);
void kdl_tokenizer_feed(kdl_tokenizer_t *, wchar_t *data, size_t length);

#endif
