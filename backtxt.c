#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"
#include "code.h"
#include "backend.h"
#include "symtab.h"


static uint64_t
solve_import(const char *p)
{
    return hash(p);
}


void
emit_finalize(void)
{
}


void
emit_load_direct(const char *value, BOOL deref0)
{
    printf("### r0 = %s\n", value);
    if (deref0) {
        printf("### r0 = [r0]\n");
    }
}


void
emit_load_indirect(const char *lvalue, BOOL deref0)
{
    char *tmp = create_address_str(lvalue, 0);
    printf("### r0 = %s\n", tmp);
    printf("### r0 = [r0]\n");
    if (deref0) {
        printf("### r0 = [r0]\n");
    }
    free(tmp);
}


void
emit_store_indirect(const char *lvalue)
{
    char *tmp = create_address_str(lvalue, 0);
    printf("### r4 = &%s\n", lvalue);
    printf("### [r4] = r0\n");
    free(tmp);
}


void
emit_store_direct(const char *lvalue)
{
    printf("### r4 = %s\n", lvalue);
    printf("### [r4] = r0\n");
}


void
emit_add(const char *value, const char *addend, int deref0, BOOL swap)
{
    if (swap) {
        const char *tmp = value;
        value = addend;
        addend = tmp;
    }
    printf("### r0 = %s\n", value);
    printf("### r1 = %s\n", addend);
    while (deref0--) {
        printf("### r0 = [r0]\n");
    }
    printf("### r0 += r1\n");
}


void
emit_call(const char *func, char **args, int nargs, int deref0, BOOL inloop, BOOL retval, int attr)
{
    char *tmp = NULL;
    int rargs = nargs;
    if (rargs > arch_regparm) {
        rargs = arch_regparm;
    }
    if (rargs) {
        assert(rargs <= 4);
        if (rargs > 3) {
            printf("### r0 = %s\n", args[0]);
            printf("### r1 = %s\n", args[1]);
            printf("### r2 = %s\n", args[2]);
            printf("### r3 = %s\n", args[3]);
        } else if (rargs > 2) {
            printf("### r0 = %s\n", args[0]);
            printf("### r1 = %s\n", args[1]);
            printf("### r2 = %s\n", args[2]);
            printf("### r3 = NULL\n");
        } else if (rargs > 1) {
            printf("### r0 = %s\n", args[0]);
            printf("### r1 = %s\n", args[1]);
        } else {
            printf("### r0 = %s\n", args[0]);
        }
    }
    while (deref0--) {
        printf("### r0 = [r0]\n");
    }
    if (attr & ATTRIB_NORETURN) {
        assert(nargs - rargs <= 1);
        printf("### %s\n", func);
        for (; rargs < nargs; rargs++) {
            printf("### a%d = %s\n", rargs - arch_regparm, args[rargs]);
        }
        return;
    }
    printf("### r4 = %s\n", func);
    if ((attr & ATTRIB_STACK) || inloop) {
        printf("### add sp\n");
    }
    if (inloop) {
        tmp = new_name("res");
        add_label(tmp, -1);
        printf("### %s:\n", tmp);
    }
    if (!(attr & ATTRIB_STDCALL) && nargs > rargs) {
        assert(nargs - rargs <= 4);
        printf("### call r4(%d)\n", nargs - rargs);
    } else {
        printf("### call r4\n");
    }
    for (; rargs < nargs; rargs++) {
        printf("### a%d = %s\n", rargs - arch_regparm, args[rargs]);
    }
    if (tmp) {
        char *sav;
        char *gdg = new_name("gdg");
        if (retval) {
            sav = emit_save();
        }
        add_extern(gdg, 0, 0);
        make_symbol_used(gdg);
        emit_load_direct(gdg, FALSE);
        emit_store_direct(tmp);
        if (retval) {
            emit_restore(sav);
        }
        free(gdg);
        free(tmp);
    }
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
    printf("### mov r1, r0\n");
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
    printf("### mov r0, r1\n");
    (void)scratch;
#endif
}


void
emit_goto(const char *label)
{
    printf("### goto %s\n", label);
}


void
emit_cond(const char *label, enum cond_t cond)
{
    BOOL inverse = (cond == COND_EQ);
    assert(cond == COND_NE || cond == COND_EQ);
    printf("### if %s(r0) goto %s\n", inverse ? "!" : "", label);
}


void
emit_label(const char *label, BOOL used)
{
    if (!used) {
        if (get_symbol(label)) { /* XXX maybe we took its address */
            printf("### %s:\n", label);
            return;
        }
        cry("unused label '%s'\n", label);
        return;
    }
    add_label(label, 1);
    printf("### %s:\n", label);
}


void
emit_extern(const char *import, int attr)
{
    /* should not emit anything, but add symbol as extern */
    printf(";;; extern %s\n", import);
    add_extern(import, solve_import(import), attr);
}


void
emit_fast(const char *var, const char *val)
{
    /* should not emit anything, this is for informative purposes only */
    printf(";;; %s := %s\n", var, val);
}


void
emit_initialize(void)
{
}


const char *
backend_name(void)
{
    return "text";
}


int arch_regparm = 4;
