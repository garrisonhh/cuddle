#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

#include <cuddle/utf8.h>

int main() {
    unsigned short x = -1;

    printf("%hu\n", x);

    exit(0);

    setlocale(LC_ALL, "en_US.UTF-8");

    char str[] = "\u263A\u263A\u263A\n";
    kdl_utf8_t state;
    wchar_t ch = L'A';

    kdl_utf8_make(&state);

    kdl_utf8_feed(&state, str, 5);

    while (kdl_utf8_next(&state, &ch) && ch)
        wprintf(L"ch: %lc 0x%02x\n", ch, ch);

    kdl_utf8_feed(&state, str + 5, 6);

    while (kdl_utf8_next(&state, &ch) && ch)
        wprintf(L"ch: %lc 0x%02x\n", ch, ch);

    return 0;
}
