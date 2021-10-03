#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

#include <cuddle/cuddle.h>

int main(int argc, char **argv) {
    // file stuff
    setlocale(LC_ALL, "C");

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

    kdl_tokenizer_make(&tzr, tzr_buf, sizeof(tzr_buf));
    kdl_token_make(&token, tok_buf);

    // read and feed
    char buf[megabyte];
    int level = 0;

    while (fread(buf, sizeof(buf[0]), len_buf, file)) {
        kdl_tok_feed(&tzr, buf, len_buf);

        while (kdl_tok_next(&tzr, &token)) {
            if (token.node) {
                wprintf(L"\n");

                for (int i = 0; i < level; ++i)
                    wprintf(L"  ");
            }

            switch (token.type) {
            case KDL_TOK_NULL:
                wprintf(L"null");

                break;
            case KDL_TOK_BOOL:
                wprintf(token.boolean ? L"true" : L"false");

                break;
            case KDL_TOK_NUMBER:
                wprintf(L"%f", token.number);

                break;
            case KDL_TOK_IDENTIFIER:
                wprintf(L"%ls", token.string);

                break;
            case KDL_TOK_STRING:
                wprintf(L"\"%ls\"", token.string);

                break;
            case KDL_TOK_CHILD_BEGIN:
                putwchar(L'{');
                ++level;

                break;
            case KDL_TOK_CHILD_END:
                --level;

                wprintf(L"\n");

                for (int i = 0; i < level; ++i)
                    wprintf(L"  ");

                putwchar(L'}');

                break;
            }

            putwchar(token.property ? L'=' : L' ');
        }
    }

    fclose(file);

    return 0;
}
