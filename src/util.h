#ifndef UTIL_H
#define UTIL_H

#include <string.h>

int last_index(const char *s, char c) {
    int i = strlen(s);

    while(i-- > 0 && s[i] != c);

    return i;
}

#endif