#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"
#include "code.h"
#include "backend.h"
#include "binary.h"
#include "symtab.h"


const int arch_regparm = 4;


static uint64_t
solve_import(const char *p)
{
    if (binmap) {
        uint64_t rv;
        char *tmp = prepend('_', p);
        rv = parse_symbols(binmap, tmp);
        free(tmp);
        if (!rv) {
            die("undefined import '%s'\n", p);
        }
        return rv;
    }
    return hash(p);
}


static void
load_value(int i, int ch, const char *arg)
{
    if (arg) {
        const struct SYM *p;
        if (is_address(arg)) {
            char *na = curate_address(arg);
            printx("    %c%d = &%s\n", ch, i, na);
            free(na);
            return;
        }
        p = get_symbol(arg);
        if (p) {
            if (p->type == SYMBOL_EXTERN) {
                printx("    %c%d = %-22s // extern: 0x%llx\n", ch, i, arg, p->addr);
            } else if (p->type == SYMBOL_LABEL) {
                printx("    %c%d = &%-21s // label\n", ch, i, p->key);
            } else {
                assert(p->type == SYMBOL_NORMAL);
                if (p->val && is_address(p->val)) {
                    char *na = curate_address(p->val);
                    printx("    %c%d = %-22s // = &%s\n", ch, i, arg, na);
                    free(na);
                } else if (p->val && try_symbol_extern(p->val)) {
                    printx("    %c%d = %-22s // = %s (extern: 0x%llx)\n", ch, i, arg, p->val, get_symbol(p->val)->addr);
                } else {
                    printx("    %c%d = %-22s // = %s\n", ch, i, arg, p->val ? p->val : "0");
                }
            }
            return;
        }
    }
    printx("    %c%d = %s\n", ch, i, arg ? arg : "0");
}


void
emit_finalize(void)
{
}


void
emit_load_direct(const char *value, BOOL deref0)
{
    load_value(0, 'r', value);
    if (deref0) {
        printx("    r0 = *r0\n");
    }
}


void
emit_load_indirect(const char *lvalue, BOOL deref0)
{
    char *tmp = create_address_str(lvalue, 0);
    load_value(0, 'r', tmp);
    printx("    r0 = *r0\n");
    if (deref0) {
        printx("    r0 = *r0\n");
    }
    free(tmp);
}


void
emit_store_indirect(const char *lvalue)
{
    char *tmp = create_address_str(lvalue, 0);
    load_value(4, 'r', tmp);
    printx("    *r4 = r0\n");
    free(tmp);
}


void
emit_store_direct(const char *lvalue)
{
    load_value(4, 'r', lvalue);
    printx("    *r4 = r0\n");
}


void
emit_or(const char *value, const char *addend, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, addend);
    load_value(1, 'r', addend);
    load_value(0, 'r', value);
    while (deref0--) {
        printx("    r0 = *r0\n");
    }
    printx("    r0 |= r1\n");
}


void
emit_xor(const char *value, const char *addend, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, addend);
    load_value(1, 'r', addend);
    load_value(0, 'r', value);
    while (deref0--) {
        printx("    r0 = *r0\n");
    }
    printx("    r0 ^= r1\n");
}


void
emit_and(const char *value, const char *addend, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, addend);
    load_value(1, 'r', addend);
    load_value(0, 'r', value);
    while (deref0--) {
        printx("    r0 = *r0\n");
    }
    printx("    r0 &= r1\n");
}


void
emit_add(const char *value, const char *addend, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, addend);
    load_value(1, 'r', addend);
    load_value(0, 'r', value);
    while (deref0--) {
        printx("    r0 = *r0\n");
    }
    printx("    r0 += r1\n");
}


void
emit_sub(const char *value, const char *addend, int deref0)
{
    load_value(1, 'r', addend);
    load_value(0, 'r', value);
    while (deref0--) {
        printx("    r0 = *r0\n");
    }
    printx("    r0 -= r1\n");
}


void
emit_mul(const char *value, const char *multiplier, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, multiplier);
    load_value(1, 'r', multiplier);
    load_value(0, 'r', value);
    while (deref0--) {
        printx("    r0 = *r0\n");
    }
    printx("    r0 *= r1\n");
}


void
emit_div(const char *value, const char *multiplier, int deref0)
{
    load_value(1, 'r', multiplier);
    load_value(0, 'r', value);
    while (deref0--) {
        printx("    r0 = *r0\n");
    }
    printx("    r0 /= r1\n");
}


void
emit_call(const char *func, char **args, int nargs, int deref0, BOOL inloop, BOOL retval, int attr, int regparm, int restack)
{
    char *tmp = NULL;
    int rargs = nargs;
    if (args == NULL) {
        rargs = nargs = 0;
    }
    if (rargs > regparm) {
        rargs = regparm;
    }
    if (rargs) {
        assert(rargs <= 4);
        switch (rargs) {
            case 4:
                load_value(3, 'r', args[3]);
            case 3:
                load_value(2, 'r', args[2]);
            case 2:
                load_value(1, 'r', args[1]);
            case 1:
                load_value(0, 'r', args[0]);
        }
    }
    while (deref0--) {
        printx("    r0 = *r0\n");
    }
    if (attr & ATTRIB_NORETURN) {
        if (nargs > rargs) {
            printx("    %s{%d}\n", func, nargs - rargs);
        } else {
            printx("    %s\n", func);
        }
        for (; rargs < nargs; rargs++) {
            load_value(rargs - regparm, 'a', args[rargs]);
        }
        return;
    }
    load_value(4, 'r', func);
    if ((attr & ATTRIB_STACK) || inloop) {
        printx("    stack 0x%x\n", (restack > 0) ? restack : inloop_stack);
    }
    if (inloop) {
        tmp = new_name("res");
        add_label(tmp, 0xdead);
        printx("%s:\n", tmp);
    }
    if (!(attr & ATTRIB_STDCALL) && nargs > rargs) {
        printx("    call r4{%d}\n", nargs - rargs);
    } else {
        printx("    call r4\n");
    }
    for (; rargs < nargs; rargs++) {
        load_value(rargs - regparm, 'a', args[rargs]);
    }
    if (inloop) {
        char *sav;
        char *gdg = new_name("gdg");
        if (retval) {
            sav = emit_save();
        }
        add_extern(gdg, 0xca11, 0, -1);
        make_symbol_used(gdg);
        emit_load_direct(gdg, FALSE);
        emit_store_indirect(tmp);
        if (retval) {
            emit_restore(sav);
        }
        free(gdg);
    }
    free(tmp);
}


char *
emit_save(void)
{
#ifdef SLOW_LOAD_SAVE
    char *scratch = new_name("tmp");
    add_symbol_forward(scratch, 0);
    emit_store_indirect(scratch);
    return scratch;
#else
    printx("    r1 = r0\n");
    return NULL;
#endif
}


void
emit_restore(char *scratch)
{
#ifdef SLOW_LOAD_SAVE
    assert(scratch);
    emit_load_direct(scratch, FALSE);
    make_symbol_used(scratch);
    free(scratch);
#else
    printx("    r0 = r1\n");
    (void)scratch;
#endif
}


void
emit_goto(const char *label)
{
    printx("    goto %s\n", label);
}


void
emit_cond(const char *label, enum cond_t cond)
{
    BOOL inverse = (cond == COND_EQ);
    assert(cond == COND_NE || cond == COND_EQ);
    printx("    if %s(r0) goto %s\n", inverse ? "!" : "", label);
}


void
emit_label(const char *label, int used, BOOL last)
{
    add_label(label, 0x1abe1);
    if (!used) {
        printx("%s:\n", label);
        cry("unused label '%s'\n", label);
    } else if (last) {
        printx("%s:\n", label);
    } else {
        printx("%s:\n", label);
    }
}


void
emit_extern(const char *import, unsigned long long val, int attr, int regparm)
{
    /* should not emit anything, but add symbol as extern */
    printx(";;; extern %s\n", import);
    add_extern(import, (val != -1ULL) ? val : solve_import(import), attr, regparm);
}


void
emit_fast(const char *var, const char *val)
{
    /* should not emit anything, this is for informative purposes only */
    printx(";;; %s := %s\n", var, val);
}


void
emit_initialize(void)
{
}


int
backend_test_gadgets(int verbose)
{
    return 0;
}


const char *
backend_name(void)
{
    return "text";
}
