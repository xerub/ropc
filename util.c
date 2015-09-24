#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"
#include "lexer.h"


#ifndef __GNUC__
int
popcount(unsigned int v)
{
    int c;
    for (c = 0; v; c++) {
        v &= v - 1;
    }
    return c;
}
#endif


void *
xmalloc(size_t size)
{
    void *p = malloc(size);
    assert(p);
    return p;
}


char *
xstrdup(const char *s)
{
    void *p = strdup(s);
    assert(p);
    return p;
}


char *
new_name(const char *tmpl)
{
    static int i = 0;
    char *p, tmp[256];
    snprintf(tmp, sizeof(tmp), "L_%s_%d", tmpl, i++);
    p = strdup(tmp);
    assert(p);
    return p;
}


char *
prepend(int ch, const char *str)
{
    int len = strlen(str) + 1;
    char *p = malloc(len + 1);
    if (p) {
        p[0] = ch;
        memcpy(p + 1, str, len);
    }
    assert(p);
    return p;
}


char *
create_address_str(const char *str)
{
    return prepend('&', str);
}


char *
create_number_str(BOOL negative, const char *str)
{
    char *p;
    if (str[0] == '0' && str[1] && tolower(str[1]) != 'x') {
        /* parse octals */
        unsigned long number = 0;
        for (++str; *str; str++) {
            number = number * 8 + (*str - '0');
        }
        p = malloc(32);
        if (p) {
            char *q = p;
            if (negative) {
                *q++ = '-';
            }
            sprintf(q, "%lu", number);
        }
    } else if (negative) {
        p = prepend('-', str);
    } else {
        p = strdup(str);
    }
    assert(p);
    return p;
}


char *
create_sum_str(const char *str1, const char *str2)
{
    int len1;
    int len2;
    char *p;
    if (IS_ADDRESS(str2)) {
        const char *tmp;
        assert(!IS_ADDRESS(str1));
        tmp = str1;
        str1 = str2;
        str2 = tmp;
    }
    len1 = strlen(str1);
    len2 = strlen(str2) + 1;
    p = malloc(len1 + 3 + len2);
    if (p) {
        memcpy(p, str1, len1);
        strcpy(p + len1, " + ");
        memcpy(p + len1 + 3, str2, len2);
    }
    assert(p);
    return p;
}


char *
copy_address_sym(const char *str)
{
    if (IS_ADDRESS(str)) {
        int len;
        char *p;
        const char *s;
        for (s = str + 1; *s && *s != ' '; s++) {
        }
        len = s - str;
        p = malloc(len);
        if (p) {
            len--;
            memcpy(p, str + 1, len);
            p[len] = '\0';
        }
        assert(p);
        return p;
    }
    return NULL;
}


unsigned int
hash(const char *p)
{
    unsigned int h = *p;
    if (h) {
        for (p += 1; *p; p++) {
            h = (h << 5) - h + *p;
        }
    }
    return h;
}


void
die(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", token.filename, token.lineno);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}


void
cry(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: warning: ", token.filename, token.lineno);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
