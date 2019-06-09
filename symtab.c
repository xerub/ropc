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
        p->regparm = -1;
        p->restack = -1;
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


int
try_symbol_regparm(const char *key)
{
    const struct SYM *p = get_symbol(key);
    if (!p) {
        /* die("symbol '%s' not defined\n", key); */
        return -1;
    }
    if (p->attr & ATTRIB_REGPARM) {
        return p->regparm;
    }
    return -1;
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
                        if (is_address(p->val)) {
                            char *na = curate_address(p->val);
                            printf("%-7s du    %s\n", p->key, na);
                            free(na);
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
                case SYMBOL_EXTERN:
                case SYMBOL_LABEL:
                    break;
                default: {
                    int i;
                    printf("%s:\n", p->key);
                    for (i = 0; i < p->type; i++) {
                        char *val = ((char **)p->val)[i];
                        if (is_address(val)) {
                            char *na = curate_address(val);
                            printf("        du    %s\n", na);
                            free(na);
                        } else if (try_symbol_extern(val)) {
                            printf("        dg    0x%-28llX; -> %s\n", get_symbol(val)->addr, val);
                        } else {
                            printf("        dd    %s\n", val);
                        }
                        free(val);
                    }
                }
            }
        }
    }
    for (p = symtab; p; p = p->next) {
        if (p->used == UNUSED && p->type == SYMBOL_STRING) {
            printf("%-7s db    %s, 0\n", p->key, p->val);
        }
    }
}


void
free_symbols(void)
{
    struct SYM *p, *q;
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
    char *esc = NULL;
    if (nasm_esc_str) {
        size_t len = strlen(arg);
        assert(len);
        esc = xmalloc(len + 1);
        memmove(esc, arg, len + 1);
        esc[0] = esc[len - 1] = '`';
        arg = esc;
    }
    for (p = symtab; p; p = p->next) {
        if (p->type == SYMBOL_STRING && !strcmp(p->val, arg)) {
            free(esc);
            return p->key;
        }
    }
    tmp = new_name("str");
    symtab = new_symbol(tmp, arg, SYMBOL_STRING);
    free(tmp);
    free(esc);
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
add_extern(const char *import, unsigned long long addr, int attr, int regparm)
{
    const struct SYM *p = get_symbol(import);
    if (p) {
        die("symbol '%s' already defined\n", import);
    }
    symtab = new_symbol(import, NULL, SYMBOL_EXTERN);
    symtab->addr = addr;
    symtab->attr = attr;
    symtab->regparm = regparm;
    symtab->restack = -1;
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
