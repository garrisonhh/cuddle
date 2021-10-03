#include <stdio.h>
#include <stdlib.h>

#include <cuddle/utf8.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

void print_mbstr(char *str) {
    puts(str);

    for (size_t i = 0; str[i]; ++i)
        printf("%hhu ", str[i]);

    puts("");
}

int main() {
    kdl_utf8_t utf8;
    char text[] = "Kat Marchán <kzm@zkat.tech>", buf[256];
    kdl_u8ch_t wtext[256] = {0};
    
    kdl_utf8_make(&utf8);
    kdl_utf8_feed(&utf8, text, ARRAY_SIZE(text));

    // encode and decode
    kdl_u8ch_t *trav = wtext;

    while (kdl_utf8_next(&utf8, trav++))
        ;

    trav = wtext;
    char decode[4];
    size_t total = 0, size, index = 0;

    while (total < ARRAY_SIZE(text)) {
        kdl_utf8_to_mbs(*trav++, decode, &size);

        for (size_t i = 0; i < size; ++i)
            buf[index++] = decode[i];

        total += size;
    }

    print_mbstr(text);
    print_mbstr(buf);
}
