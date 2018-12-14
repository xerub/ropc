#ifndef UTIL_H_
#define UTIL_H_

#ifndef FALSE
#define FALSE 0
#define TRUE !FALSE
typedef int BOOL;
#endif
/*
#include <stdbool.h>
#define BOOL bool
#define TRUE true
#define FALSE false
*/

#define IS_ADDRESS(arg) (*(arg) == '&')

#ifndef __GNUC__
int popcount(unsigned int v);
#else
#define popcount __builtin_popcount
#endif

void *xmalloc(size_t size);
char *xstrdup(const char *s);

char *new_name(const char *tmpl);
char *prepend(int ch, const char *str);

char *create_address_str(const char *str, int offset);
char *create_number_str(BOOL negative, const char *str);
char *create_op_str(const char *str1, const char *str2, int op);

char *copy_address_sym(const char *str);

unsigned int hash(const char *p);

void die(const char *fmt, ...) __attribute__ ((format(printf, 1, 2), noreturn));
void cry(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));

#endif
