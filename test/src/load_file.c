#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

#include <cuddle/cuddle.h>

// since I can't get fread to work
bool wchar_fread(wchar_t *buf, size_t len_buf, FILE *file) {
    char mb_buf[len_buf];
    size_t n_read = fread(mb_buf, sizeof(mb_buf[0]), len_buf, file);

    mbstowcs(buf, mb_buf, len_buf);

    return n_read > 0;
}

int main(int argc, char **argv) {
    // file stuff
    setlocale(LC_ALL, "en_US.UTF-8");

    if (argc != 2) {
        fprintf(stderr, "please supply a file path as first argument.\n");
        exit(-1);
    }

    FILE *file = fopen(argv[1], "r");

    if (!file) {
        fprintf(stderr, "can't open file :(\n");
        exit(-1);
    }

    // tokenizing stuff
    kdl_tokenizer_t tzr;
    kdl_token_t token;

    const size_t megabyte = 1024 * 1024;
    const size_t len_buf = megabyte / sizeof(wchar_t);
    wchar_t tzr_buf[len_buf], tok_buf[len_buf];

    kdl_tokenizer_make(&tzr, tzr_buf);
    kdl_token_make(&token, tok_buf);

    // read and feed
    wchar_t buf[len_buf];

    while (wchar_fread(buf, len_buf, file)) {
        kdl_tok_feed(&tzr, buf, len_buf);

        while (kdl_tok_next(&tzr, &token)) {
            wprintf(L"%-32s %-64ls ", KDL_TOKEN_TYPES[token.type], tzr.buf);

            switch (token.type) {
            default:
                break;
            case KDL_TOK_NULL:
                wprintf(L"-");

                break;
            case KDL_TOK_BOOL:
                wprintf(token.boolean ? L"true" : L"false");

                break;
            case KDL_TOK_NUMBER:
                wprintf(L"%f", token.number);

                break;
            case KDL_TOK_STRING:
                wprintf(token.string);

                break;
            }

            putwchar(L'\n');
        }
    }

    fclose(file);

    return 0;
}