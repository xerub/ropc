#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "util.h"
#include "lexer.h"


#define isalnu_(c) (isalnum(c) || ((c) == '_'))


#define ADDRESS_MARK '@'


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
create_address_str(const char *str, int offset)
{
    if (offset) {
        int len = strlen(str) + 1;
        char *p = malloc(len + 16);
        if (p) {
            p[0] = ADDRESS_MARK;
            memcpy(p + 1, str, len - 1);
            sprintf(p + len, " %+d", offset);
        }
        assert(p);
        return p;
    }
    return prepend(ADDRESS_MARK, str);
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
create_op_str(const char *str1, const char *str2, int op)
{
    int len1;
    int len2;
    char *p;
#if 0
    do {
        unsigned long long num1;
        unsigned long long num2;
        errno = 0;
        num1 = strtoull(str1, &p, 0);
        if (errno || p <= str1 || *p) {
            break;
        }
        errno = 0;
        num2 = strtoull(str2, &p, 0);
        if (errno || p <= str2 || *p) {
            break;
        }
        switch (op) {
            case '+':
                num1 += num2;
                break;
            case '-':
                num1 -= num2;
                break;
            case '*':
                num1 *= num2;
                break;
            case '&':
                num1 &= num2;
                break;
            case '|':
                num1 &= num2;
                break;
            case '^':
                num1 &= num2;
                break;
            default:
                continue;
        }
        p = malloc(32);
        if (!p) {
            break;
        }
        sprintf(p, "%llu", num1);
        return p;
    } while (0);
#endif
    if (is_address(str2) && (op == '+' || op == '*' || op == '&' || op == '|' || op == '^')) {
        const char *tmp;
        assert(!is_address(str1));
        tmp = str1;
        str1 = str2;
        str2 = tmp;
    }
    len1 = strlen(str1);
    len2 = strlen(str2);
    p = malloc(1 + len1 + 3 + len2 + 1 + 1);
    if (p) {
        p[0] = '(';
        memcpy(p + 1, str1, len1);
        p[++len1] = ' ';
        p[++len1] = op;
        p[++len1] = ' ';
        memcpy(p + 1 + len1, str2, len2);
        p[1 + len1 + len2] = ')';
        p[1 + len1 + len2 + 1] = '\0';
    }
    assert(p);
    return p;
}


int
is_pot_str(const char *str)
{
    char *p;
    int trailing = 0;
    unsigned long long b;
    errno = 0;
    b = strtoull(str, &p, 0);
    if (errno || p <= str || *p) {
        return -1;
    }
    if (b == 0) {
        return 0;
    }
    if (b & (b - 1)) {
        return -1;
    }
    do {
        trailing++;
    } while (b >>= 1);
    return trailing;
}


const char *
is_address(const char *str)
{
    str = strchr(str, ADDRESS_MARK);
    if (!str) {
        return NULL;
    }
    assert(!strchr(str + 1, ADDRESS_MARK));
    return str;
}


char *
curate_address(const char *str)
{
    char *p;
    size_t len = strlen(str);
    const char *at = is_address(str);
    assert(at && !strchr(at + 1, ADDRESS_MARK));
    p = malloc(len);
    if (p) {
        size_t pos = at - str;
        memcpy(p, str, pos);
        strcpy(p + pos, at + 1);
    }
    assert(p);
    return p;
}


char *
copy_address_sym(const char *str)
{
    str = is_address(str);
    if (str) {
        int len;
        char *p;
        const char *s;
        for (s = str + 1; isalnu_(*s); s++) {
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


static char *outfn = NULL;
static FILE *outfp = NULL;

int
printx(const char *fmt, ...)
{
    int rv;
    va_list ap;
    va_start(ap, fmt);
    rv = vfprintf(outfp ? outfp : stdout, fmt, ap);
    va_end(ap);
    return rv;
}

void
new_printer(const char *filename)
{
    if (outfn) {
        FILE *f;
        char buf[BUFSIZ];
        assert(outfp);
        fflush(outfp);
        rewind(outfp);
        f = fopen(outfn, "wt");
        if (!f) {
            errx(1, "cannot write to '%s'", outfn);
        }
        while (fgets(buf, sizeof(buf), outfp)) {
            fputs(buf, f);
        }
        fclose(f);
        fclose(outfp);
        free(outfn);
    }

    if (!filename) {
        outfn = NULL;
        outfp = NULL;
        return;
    }

    outfn = xstrdup(filename);
    outfp = tmpfile();
    assert(outfp);
    unlink(outfn);
}
