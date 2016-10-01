#include <assert.h>
#include <stdio.h> /* XXX for debug */
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"
#include "symtab.h"


static struct SYM *symtab = NULL;


static struct SYM *
new_symbol(const char *key, const void *val, int type)
{
    struct SYM *p = malloc(sizeof(struct SYM));
    if (p) {
        p->used = UNUSED;
        p->type = type;
        p->idx = 0;
        p->addr = 0;
        p->attr = 0;
        p->key = xstrdup(key);
        p->val = val ? xstrdup(val) : NULL;
        p->next = symtab;
    }
    assert(p);
    return p;
}


const struct SYM *
get_symbol(const char *key)
{
    struct SYM *p;
    for (p = symtab; p; p = p->next) {
        if (!strcmp(p->key, key)) {
            break;
        }
    }
    return p;
}


void
add_symbol_defined(const char *key, const void *val, int attr)
{
    const struct SYM *p = get_symbol(key);
    if (p) {
        die("symbol '%s' already defined\n", key);
    }
    assert(val);
    symtab = new_symbol(key, val, SYMBOL_NORMAL);
    symtab->attr |= attr;
}


void
add_symbol_forward(const char *key, int attr)
{
    const struct SYM *p = get_symbol(key);
    if (p) {
        return;
    }
    symtab = new_symbol(key, NULL, SYMBOL_NORMAL);
    symtab->attr |= attr;
}


enum use_t
get_symbol_used(const char *key)
{
    const struct SYM *p = get_symbol(key);
    if (!p) {
        die("symbol '%s' not defined\n", key);
    }
    return p->used;
}


void
make_symbol_used(const char *key)
{
    struct SYM *p = (struct SYM *)get_symbol(key); // wrong cast
    if (!p) {
        die("symbol '%s' not defined\n", key);
    }
    if (p->type != SYMBOL_EXTERN) {
        if (p->type != SYMBOL_NORMAL) {
            cry("using symbol '%s' is probably wrong\n", key);
        }
        p->used = USED;
    }
}


void
mark_all_used(enum use_t u)
{
    struct SYM *p;
    for (p = symtab; p; p = p->next) {
        if (p->used == USED) {
            p->used = u;
        }
    }
}


BOOL
try_symbol_extern(const char *key)
{
    const struct SYM *p = get_symbol(key);
    if (!p) {
        /* XXX die("symbol '%s' not defined\n", key); */
        return FALSE;
    }
    return (p->type == SYMBOL_EXTERN);
}


int
try_symbol_attr(const char *key)
{
    const struct SYM *p = get_symbol(key);
    if (!p) {
        /* die("symbol '%s' not defined\n", key); */
        return 0;
    }
    return p->attr;
}


void
emit_symbols(void)
{
    struct SYM *p;
    for (p = symtab; p; p = p->next) {
        if (p->used == UNUSED) {
            switch (p->type) {
                case SYMBOL_NORMAL:
                    if (p->val) {
                        if (IS_ADDRESS(p->val)) {
                            printf("%-7s du    %s\n", p->key, p->val + 1);
                        } else if (try_symbol_extern(p->val)) {
                            printf("%-7s dg    0x%-28llX; -> %s\n", p->key, get_symbol(p->val)->addr, p->val);
                        } else {
                            printf("%-7s dd    %s\n", p->key, p->val);
                        }
                    } else if (p->attr & ATTRIB_VOLATILE) {
                        printf("%-7s dd    0\n", p->key);
                    } else {
                        printf(";-- %s = ?\n", p->key);
                    }
                    break;
                case SYMBOL_STRING:
                    printf("%-7s db    %s, 0\n", p->key, p->val);
                    printf("        align 4\n");
                    break;
                case SYMBOL_EXTERN:
                case SYMBOL_LABEL:
                    break;
                default: {
                    int i;
                    printf("%s:\n", p->key);
                    for (i = 0; i < p->type; i++) {
                        char *val = ((char **)p->val)[i];
                        if (IS_ADDRESS(val)) {
                            printf("        du    %s\n", val + 1);
                        } else {
                            printf("        dd    %s\n", val);
                        }
                        free(val);
                    }
                }
            }
        }
    }
}


void
free_symbols(void)
{
    struct SYM *p, *q;
    emit_symbols();
    for (p = symtab; p; ) {
        q = p->next;
#if 0
        if (p->used == UNUSED) {
            switch (p->type) {
                case SYMBOL_NORMAL:
                    printf(";-- %s = %s\n", p->key, p->val ? p->val : "?");
                    break;
                case SYMBOL_STRING:
                    printf(";-- %s = %s, 0\n", p->key, p->val);
                    break;
                case SYMBOL_EXTERN:
                case SYMBOL_LABEL:
                    break;
                default: {
                    int i;
                    printf(";-- %s = {\n", p->key);
                    for (i = 0; i < p->type; i++) {
                        char *val = ((char **)p->val)[i];
                        printf("\t%s\n", val);
                        free(val);
                    }
                    printf("}\n");
                }
            }
        }
#endif
        free(p->key);
        free(p->val);
        free(p);
        p = q;
    }
    symtab = p;
}


/*** strings ******************************************************************/


char *
add_string(const char *arg)
{
    char *tmp;
    struct SYM *p;
    for (p = symtab; p; p = p->next) {
        if (p->type == SYMBOL_STRING && !strcmp(p->val, arg)) {
            return p->key;
        }
    }
    tmp = new_name("str");
    symtab = new_symbol(tmp, arg, SYMBOL_STRING);
    free(tmp);
    return symtab->key;
}


char *
add_vector(const char **args, int narg)
{
    char *tmp;
    assert(narg);
    tmp = new_name("vec"); /* XXX coallesce */
    symtab = new_symbol(tmp, NULL, narg);
    free(tmp);
    symtab->val = (void *)args; /* XXX abuse */
    return symtab->key;
}


void
add_extern(const char *import, unsigned long long addr, int attr)
{
    const struct SYM *p = get_symbol(import);
    if (p) {
        die("symbol '%s' already defined\n", import);
    }
    symtab = new_symbol(import, NULL, SYMBOL_EXTERN);
    symtab->addr = addr;
    symtab->attr = attr;
}


void
add_label(const char *label, int idx)
{
    const struct SYM *p = get_symbol(label);
    if (p) {
        die("symbol '%s' already defined\n", label);
    }
    symtab = new_symbol(label, NULL, SYMBOL_LABEL);
    symtab->idx = idx;
}


const char *
get_label_with_label(const char *target)
{
    struct SYM *p;
    for (p = symtab; p; p = p->next) {
        if (p->type == SYMBOL_LABEL && p->val && !strcmp(p->val, target)) {
            return p->key;
        }
    }
    return NULL;
}


void
add_label_with_label(const char *label, const char *target)
{
    const struct SYM *p = get_symbol(label);
    if (p) {
        die("symbol '%s' already defined\n", label);
    }
    symtab = new_symbol(label, target, SYMBOL_LABEL);
    symtab->idx = -1;
}
