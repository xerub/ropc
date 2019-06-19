#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"
#include "code.h"
#include "backend.h"
#include "symtab.h"
#include "binary.h"


#define X0 (1 << 0)
#define X1 (1 << 1)
#define X2 (1 << 2)
#define X3 (1 << 3)
#define X4 (1 << 4)
#define X5 (1 << 5)
#define X6 (1 << 6)
#define X7 (1 << 7)
#define X8 (1 << 8)
#define X9 (1 << 9)
#define X10 (1 << 10)
#define X11 (1 << 11)
#define X12 (1 << 12)
#define X13 (1 << 13)
#define X14 (1 << 14)
#define X15 (1 << 15)
#define X16 (1 << 16)
#define X17 (1 << 17)
#define X18 (1 << 18)
#define X19 (1 << 19)
#define X20 (1 << 20)
#define X21 (1 << 21)
#define X22 (1 << 22)
#define X23 (1 << 23)
#define X24 (1 << 24)
#define X25 (1 << 25)
#define X26 (1 << 26)
#define X27 (1 << 27)
#define X28 (1 << 28)
#define X29 (1 << 29)
#define LR (1 << 30)
#define SP (1 << 31)

typedef char *BRICK; // XXX make it elt

enum R_OP {
    NOP,
    LABEL,
    LDR_X19,
    LDR_X0,
    LDR_X1,
    LDR_X2,
    LDR_X3,
    LDR_X0_X0,
    ADD_X0_X1,
    SUB_X0_X1,
    MUL_X0_X19,
    BLR_X19,
    STR_X0_X19,
    COMMUTE,
    NOT_X0,
    SEL_1,
    SEL_2,
    PIVOT,
    MOV_X1_X0,
    MOV_X0_X1,
    ADD_SP,
    LDR_X29,
    RET_X8,
    BR_X3,
    BR_X16,
    BX_IMM,
    BX_IMM_1
};

struct R_OPDEF {
    enum R_OP op;
    unsigned spill;     // which registers are undefined (NB: stack load is not considered spilling)
    unsigned output;    // which registers will be loaded from stack (NB: set of *all* registers loaded from stack)
    unsigned auxout;    // which registers will be loaded from stack as a side-effect (NB: subset of [output] registers which are not targeted by this op)
    int incsp;
    unsigned long long addr;
    const char *text;
};


static struct R_OPDEF optab[] = {
    { NOP },
    { LABEL,        /**/ 0xFFFFFFFF,                     /**/ 0,                                              /**/ 0,           /**/  0, 0, NULL,                                                          },
    { LDR_X19,      /**/ 0,                              /**/                            X19|X20|        X29, /**/     X20|X29, /**/  0, 0, "ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret", },
    { LDR_X0,       /**/                            X29, /**/ X0,                                             /**/ 0,           /**/  0, 0, "ldr x0, [sp] / blr x8",                                       },
    { LDR_X1,       /**/                            X29, /**/    X1,                                          /**/ 0,           /**/  0, 0, "ldr x1, [sp] / blr x8",                                       },
    { LDR_X2,       /**/                            X29, /**/       X2,                                       /**/ 0,           /**/  0, 0, "ldr x2, [sp] / blr x8",                                       },
    { LDR_X3,       /**/                            X29, /**/          X3,                                    /**/ 0,           /**/  0, 0, "ldr x3, [sp] / blr x8",                                       },
    { LDR_X0_X0,    /**/                            X29, /**/                                            X29, /**/         X29, /**/  0, 0, "ldr x0, [x0] / blr x8",                                       },
    { ADD_X0_X1,    /**/ X0,                             /**/                                            X29, /**/         X29, /**/  0, 0, "add x0, x0, x1 / ldp x29, x30, [sp], #0x10 / ret",            },
    { SUB_X0_X1,    /**/ X0,                             /**/                                            X29, /**/         X29, /**/  0, 0, "sub x0, x0, x1 / ldp x29, x30, [sp], #0x10 / ret",            },
    { MUL_X0_X19,   /**/ X0|X1,                          /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, 0, "mul x0, x0, x19 / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret", },
    { BLR_X19,      /**/ X0|X1|X2|X3|X4|X5|X6|X7|X8,     /**/                            X19|X20|X21|X22|X29, /**/ 0,           /**/  0, 0, "blr x19 / ldp x29, x30, [sp, #0x20] / ldp x20, x19, [sp, #0x10] / ldp x22, x21, [sp], #0x30 / ret", },
    { STR_X0_X19,   /**/ 0,                              /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, 0, "str x0, [x19] / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret", },
    { COMMUTE,      /**/ 0,                              /**/                                            X29, /**/ 0,           /**/  0, 0, "mov sp, x29 / ldp x29, x30, [sp], #0x10 / ret",               },
    { NOT_X0,       /**/ X0,                             /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, 0, "cmp x0, #0 / cset w0, eq / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret", },
    { SEL_1,        /**/ X0,                             /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, 0, "csel x19, x19, xzr, ne / mov x0, x19 / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret", },
    { SEL_2,        /**/ X0,                             /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, 0, "csel x19, x19, x0, eq / mov x0, x19 / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret", },
    { PIVOT,        /**/ 0xFFFFFFFF,                     /**/ SP, /* XXX not needed but keeps alignment */    /**/ 0,           /**/  0, 0, "mov sp, x1 / br x0",                                          },
    { MOV_X1_X0,    /**/    X1|                     X29, /**/                                            X29, /**/         X29, /**/  0, 0, "mov x1, x0 / blr x8",                                         },
    { MOV_X0_X1,    /**/ X0|                        X29, /**/                                            X29, /**/         X29, /**/  0, 0, "mov x0, x1 / blr x8",                                         },
    { ADD_SP,       /**/ 0,                              /**/ 0,                                              /**/ 0,           /**/ -1, 0, NULL                                                           },
    { LDR_X29,      /**/ 0,                              /**/                                            X29, /**/ 0,           /**/  0, 0, "ldp x29, x30, [sp], #0x10 / ret",                             },
    { RET_X8,       /**/ X29,                            /**/                         X8,                     /**/ 0,           /**/  0, 0, "ldr x8, [sp, #8] / blr x8",                                   },
    { BR_X3,        /**/ X0|X1|X2|X3|X4|X5|X6|X7|X8,     /**/                                            X29, /**/         X29, /**/  0, 0, "ldp x29, x30, [sp], #0x10 / br x3",                           },
    { BR_X16,       /**/ X0|X1|X2|X3|X4|X5|X6|X7|X8,     /**/ X0|X1|X2|X3|X4|X5|X6|X7|                   X29, /**/         X29, /**/  0, 0, "mov x16, x0 / ldp x7, x6, [sp], #0x10 / ldp x5, x4, [sp], #0x10 / ldp x3, x2, [sp], #0x10 / ldp x1, x0, [sp], #0x10 / ldp x29, x30, [sp], #0x10 / br x16", },
    { BX_IMM,       /**/ 0xFFFFFFFF,                     /**/ 0,                                              /**/ 0,           /**/  0, 0, NULL,                                                          },
    { BX_IMM_1,     /**/ 0xFFFFFFFF,                     /**/ 0,                                              /**/ 0,           /**/  1, 0, NULL,                                                          },
};


static int idx = 0;
static BRICK strip[10240];

static unsigned dirty = 0xFFFFFFFF;
static int pointer[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

struct range *ranges = NULL;


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


// XXX fixme
static void
solve_op(enum R_OP op)
{
    uint64_t rv = 0;
    struct R_OPDEF *r = &optab[op];
    if (r->addr) {
        return;
    }
    if (!binmap) {
        r->addr = 0xFF000000 + op;
        return;
    }
    if (!ranges) {
        ranges = parse_ranges(binmap);
    }
    switch (op) {
        case NOP:
        case LABEL:
        case ADD_SP:
        case BX_IMM:
        case BX_IMM_1:
            return;
        case LDR_X19:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            break;
        case LDR_X0:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E0 03 40 F9 00 01 3F D6");
            break;
        case LDR_X1:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E1 03 40 F9 00 01 3F D6");
            break;
        case LDR_X2:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E2 03 40 F9 00 01 3F D6");
            break;
        case LDR_X3:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E3 03 40 F9 00 01 3F D6");
            break;
        case LDR_X0_X0:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 40 F9 00 01 3F D6");
            break;
        case ADD_X0_X1:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 01 8B FD 7B C1 A8 C0 03 5F D6");
            break;
        case SUB_X0_X1:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 01 CB FD 7B C1 A8 C0 03 5F D6");
            if (!rv) {
                rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 01 CB FD 7B 41 A9 FF 83 00 91 C0 03 5F D6");
                optab[SUB_X0_X1].incsp = 2;
                optab[SUB_X0_X1].text = "sub x0, x0, x1 / ldp x29, x30, [sp, #0x10] / add sp, sp, #0x20 / ret";
            }
            break;
        case MUL_X0_X19:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 7C 13 9B FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            break;
        case BLR_X19:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 60 02 3F D6 FD 7B 42 A9 F4 4F 41 A9 F6 57 C3 A8 C0 03 5F D6");
            break;
        case STR_X0_X19:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 60 02 00 F9 FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            break;
        case COMMUTE:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ BF 03 00 91 FD 7B C1 A8 C0 03 5F D6");
            break;
        case NOT_X0:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 1F 00 00 F1 E0 17 9F 1A FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            break;
        case SEL_1:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 73 12 9F 9A E0 03 13 AA FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            if (!rv) {
                rv = parse_string(ranges, binmap, NULL, NULL, "+ 73 12 9F 9A E0 03 13 AA FD 7B 42 A9 F4 4F 41 A9 F6 57 C3 A8 C0 03 5F D6");
                if (rv) {
                    optab[SEL_1].output |= X21|X22;
                    optab[SEL_1].auxout |= X21|X22;
                    optab[SEL_1].text = "csel x19, x19, xzr, ne / mov x0, x19 / ldp x29, x30, [sp, #0x20] / ldp x20, x19, [sp, #0x10] / ldp x22, x21, [sp], #0x30 / ret";
                    break;
                }
                rv = parse_string(ranges, binmap, NULL, NULL, "+ 73 12 9F 9A E0 03 13 AA FD 7B 42 A9 F4 4F 41 A9 FF C3 00 91 C0 03 5F D6");
                if (rv) {
                    optab[SEL_1].output |= X21|X22;
                    optab[SEL_1].auxout |= X21|X22;
                    optab[SEL_1].text = "csel x19, x19, xzr, ne / mov x0, x19 / ldp x29, x30, [sp, #0x20] / ldp x20, x19, [sp, #0x10] / add sp, sp, #0x30 / ret";
                    break;
                }
            }
            break;
        case SEL_2:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 73 02 80 9A E0 03 13 AA FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            break;
        case PIVOT:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 3F 00 00 91 00 00 1F D6");
            break;
        case MOV_X1_X0:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E1 03 00 AA 00 01 3F D6");
            break;
        case MOV_X0_X1:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E0 03 01 AA 00 01 3F D6");
            break;
        case LDR_X29:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ FD 7B C1 A8 C0 03 5F D6");
            break;
        case RET_X8:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E8 07 40 F9 00 01 3F D6");
            break;
        case BR_X3:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ FD 7B C1 A8 60 00 1F D6");
            break;
        case BR_X16:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ F0 03 00 AA E7 1B C1 A8 E5 13 C1 A8 E3 0B C1 A8 E1 03 C1 A8 FD 7B C1 A8 00 02 1F D6");
            break;
    }
    if (!rv) {
        die("undefined gadget '%s'\n", r->text);
    }
    r->addr = rv;
}


static BRICK
argdup(const char *arg)
{
    if (arg) {
        char *p = strdup(arg);
        assert(p);
        return p;
    }
    return NULL;
}


static int
make1(enum R_OP op, ...)
{
    int i;
    const struct R_OPDEF *r = &optab[op];
    unsigned spill = r->spill;
    va_list ap;
    va_start(ap, op);
    assert(idx < 10000);
    solve_op(op);
    switch (op) {
        case LDR_X0:
        case LDR_X1:
        case LDR_X2:
        case LDR_X3:
            i = op - LDR_X0;
            if (optimize_reg && pointer[i]) { // XXX && !dirty(i)
                strip[pointer[i]] = argdup(va_arg(ap, char *));
                pointer[i] = 0;
                goto done;
            }
            make1(RET_X8, NULL);
            strip[idx++] = (BRICK)op;
            break;
        case LDR_X0_X0:
        case MOV_X1_X0:
        case MOV_X0_X1:
            make1(RET_X8, NULL);
        case ADD_X0_X1:
        case SUB_X0_X1:
        case MUL_X0_X19:
            strip[idx++] = (BRICK)op;
            for (i = 0; i < r->incsp; i++) {
                strip[idx++] = NULL;
            }
            break;
        case LDR_X19:
            if (optimize_reg && pointer[19]) { // XXX && !dirty(19)
                strip[pointer[19]] = argdup(va_arg(ap, char *));
                pointer[19] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case ADD_SP:
            strip[idx++] = (BRICK)op;
            strip[idx++] = (BRICK)(long)va_arg(ap, int);
            break;
        case BX_IMM:
        case BX_IMM_1: {
            char *func = argdup(va_arg(ap, char *));
            char **args = va_arg(ap, char **);
            strip[idx++] = (BRICK)op;
            strip[idx++] = func;
            for (i = 0; i < r->incsp; i++) {
                strip[idx++] = argdup(args[i]);
            }
            break;
        }
        case BR_X3:
        case BR_X16:
            strip[idx++] = (BRICK)op;
            break;
        case BLR_X19:
            strip[idx++] = (BRICK)op;
            break;
        case STR_X0_X19:
            strip[idx++] = (BRICK)op;
            break;
        case LABEL:
            strip[idx++] = (BRICK)op;
            strip[idx++] = argdup(va_arg(ap, char *));
            spill = va_arg(ap, unsigned);
            strip[idx++] = (BRICK)(unsigned long)spill;
            break;
        case COMMUTE:
        case NOT_X0:
        case SEL_1:
        case SEL_2:
        case PIVOT:
            strip[idx++] = (BRICK)op;
            break;
        case LDR_X29:
            if (optimize_reg && pointer[29]) { // XXX && !dirty(29)
                strip[pointer[29]] = argdup(va_arg(ap, char *));
                pointer[29] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case RET_X8:
            if (optimize_reg && !(dirty & X8)) { // XXX hack
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        default:
            assert(0);
    }
    dirty &= ~r->output;
    dirty |= spill;
    for (i = 0; i < 32; i++) {
        if (r->output & (1 << i)) {
            if (r->auxout & (1 << i)) {
                pointer[i] = idx;
                strip[idx] = NULL;
            } else {
                pointer[i] = 0;
                strip[idx] = argdup(va_arg(ap, char *));
            }
            idx++;
        }
        if (dirty & (1 << i)) {
            pointer[i] = 0;
        }
    }
#if 666
    if (op == RET_X8) {
        make1(LDR_X29, NULL);
    }
#endif
  done:
    va_end(ap);
    return idx;
}


static void
play_data(const char *arg, int i, int ch)
{
    if (arg) {
        const struct SYM *p;
        if (is_address(arg)) {
            char *na = curate_address(arg);
            printf("        du    %-30s; -> %c%d\n", na, ch, i);
            free(na);
            return;
        }
        p = get_symbol(arg);
        if (p) {
            if (p->type == SYMBOL_EXTERN) {
                printf("        dg    0x%-28llX; -> %c%d: %s\n", p->addr, ch, i, arg);
            } else if (p->type == SYMBOL_LABEL) {
                printf("        du    %-30s; -> %c%d\n", p->key, ch, i);
            } else {
                assert(p->type == SYMBOL_NORMAL);
                if (p->val && is_address(p->val)) {
                    char *na = curate_address(p->val);
                    printf("%-7s du    %-30s; -> %c%d\n", arg, na, ch, i);
                    free(na);
                } else if (p->val && try_symbol_extern(p->val)) {
                    printf("%-7s dg    0x%-28llX; -> %s\n", arg, p->addr, p->val);
                } else {
                    printf("%-7s dd    %-30s; -> %c%d\n", arg, p->val ? p->val : "0", ch, i);
                }
            }
            return;
        }
    }
    printf("        dd    %-30s; -> %c%d\n", arg ? arg : "0", ch, i);
}


void
emit_finalize(void)
{
    int i;
    const BRICK *p = strip;
    const BRICK *q = p + idx;
    BRICK value[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    dirty = 0xFFFFFFFF;
    while (p < q) {
        enum R_OP op = (enum R_OP)(long)(*p++);
        const struct R_OPDEF *r = &optab[op];
        if (op == LABEL) {
            BRICK arg = *p++;
            unsigned spill = (unsigned long)*p++;
            printf("%s:\n", arg);
            free(arg);
            if (!spill) {
                // soft label: do not affect dirty set
                continue;
            }
            // regular label: update dirty set
            goto cont;
        }
        if (op == BX_IMM || op == BX_IMM_1) {
            BRICK arg = *p++;
            const struct SYM *p = get_symbol(arg);
            assert(p);
            if (p->type != SYMBOL_EXTERN) {
                printf("%-7s dd    %-30s; -> PC\n", arg, p->val ? p->val : "0");
            } else {
                printf("        dg    0x%-28llX; -> PC: %s\n", p->addr, arg);
            }
            free(arg);
        } else if (op == ADD_SP) {
            long restack = (long)*p++;
            printf("        times 0x%lx dd 0\n", (restack > 0) ? restack : inloop_stack);
        } else {
            printf("        dg    0x%-28llX; -> PC: %s\n", r->addr, r->text);
        }
        switch (op) {
            case LDR_X19:
            case LDR_X0:
            case LDR_X1:
            case LDR_X2:
            case LDR_X3:
            case LDR_X0_X0:
            case ADD_X0_X1:
            case SUB_X0_X1:
            case MUL_X0_X19:
            case BLR_X19:
            case STR_X0_X19:
            case COMMUTE:
            case NOT_X0:
            case SEL_1:
            case SEL_2:
            case PIVOT:
            case MOV_X1_X0:
            case MOV_X0_X1:
            case LDR_X29:
            case RET_X8:
            case BR_X3:
            case BR_X16:
            case BX_IMM:
            case BX_IMM_1:
                for (i = 0; i < r->incsp; i++) {
                    play_data(*p, i, 'A');
                    free(*p);
                    p++;
                }
            case ADD_SP:
                for (i = 0; i < 32; i++) {
                    if (r->output & (1 << i)) {
                        free(value[i]);
                        value[i] = *p++;
                    }
                }
                for (i = 28; i >= 0; i--) {
                    if (r->output & (1 << i)) {
                        play_data(value[i], i, 'X');
                    }
                }
                for (i = 29; i < 32; i++) {
                    if (r->output & (1 << i)) {
                        play_data(value[i], i, 'X');
                    }
                }
                break;
            default:
                assert(0);
        }
      cont:
        dirty &= ~r->output;
        dirty |= r->spill;
        if (show_reg_set) {
            printf(";");
            for (i = 0; i < 32; i++) {
                if (!(dirty & (1 << i))) {
                    if (i > 8 && i != 16 && i != 19 && i != 20 && i != 29) {
                        continue;
                    }
                    printf(" X%d=%s", i, value[i] ? value[i] : "0");
                } else {
                    free(value[i]);
                    value[i] = NULL;
                }
            }
            printf("\n");
        }
    }
    // XXX free all values
    for (i = 0; i < 32; i++) {
        free(value[i]);
        value[i] = NULL;
        pointer[i] = 0;
    }
    dirty = 0xFFFFFFFF;
    idx = 0;
    delete_ranges(ranges);
}


void
emit_load_direct(const char *value, BOOL deref0)
{
    make1(LDR_X0, value);
    if (deref0) {
        make1(LDR_X0_X0);
    }
}


void
emit_load_indirect(const char *lvalue, BOOL deref0)
{
    char *tmp = create_address_str(lvalue, 0);
    make1(LDR_X0, tmp);
    make1(LDR_X0_X0);
    if (deref0) {
        make1(LDR_X0_X0);
    }
    free(tmp);
}


void
emit_store_indirect(const char *lvalue)
{
    char *tmp = create_address_str(lvalue, 0);
    make1(LDR_X19, tmp);
    make1(STR_X0_X19);
    free(tmp);
}


void
emit_store_direct(const char *lvalue)
{
    make1(LDR_X19, lvalue);
    make1(STR_X0_X19);
}


void
emit_add(const char *value, const char *addend, int deref0, BOOL swap)
{
    if (swap) {
        const char *tmp = value;
        value = addend;
        addend = tmp;
    }
    make1(LDR_X0, value);
    make1(LDR_X1, addend);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    make1(ADD_X0_X1);
}


void
emit_sub(const char *value, const char *addend, int deref0)
{
    make1(LDR_X0, value);
    make1(LDR_X1, addend);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    make1(SUB_X0_X1);
}


void
emit_mul(const char *value, const char *multiplier, int deref0, BOOL swap)
{
    if (swap) {
        const char *tmp = value;
        value = multiplier;
        multiplier = tmp;
    }
    make1(LDR_X0, value);
    make1(LDR_X19, multiplier);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    make1(MUL_X0_X19);
}


void
emit_call(const char *func, char **args, int nargs, int deref0, BOOL inloop, BOOL retval, int attr, int regparm, int restack)
{
    char *tmp = NULL;
    enum R_OP op = BLR_X19;
    int rargs = nargs;
    if (rargs > regparm) {
        rargs = regparm;
    }
    assert(rargs <= 8 && nargs - rargs <= 5);
    if (nargs > rargs || (deref0 && rargs > 3)) {
        if (rargs <= 3) {
            switch (rargs) {
                case 3:
                    make1(LDR_X2, args[2]);
                case 2:
                    make1(LDR_X1, args[1]);
                case 1:
                    make1(LDR_X0, args[0]);
                case 0:
                    break;
            }
        } else {
            make1(LDR_X0, get_symbol("__gadget_ret")->key);
            make1(BR_X16, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        }
    } else {
        if (rargs <= 3) {
            switch (rargs) {
                case 3:
                    make1(LDR_X2, args[2]);
                case 2:
                    make1(LDR_X1, args[1]);
                case 1:
                    make1(LDR_X0, args[0]);
                case 0:
                    break;
            }
            op = BR_X3;
        } else {
            op = BR_X16;
        }
    }
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    if (attr & ATTRIB_NORETURN) {
        assert(rargs <= 3 && nargs - rargs <= 1);
        make1(BX_IMM + nargs - rargs, func, args + rargs);
        return;
    }
    switch (op) {
        case BLR_X19:
            make1(LDR_X19, func);
            break;
        case BR_X3:
            make1(LDR_X3, func);
            break;
        case BR_X16:
            make1(LDR_X0, func);
            break;
        default:
            assert(0);
    }
    if ((attr & ATTRIB_STACK) || inloop) {
        char *skip = new_name("skp");
        char *skip8 = create_address_str(skip, -8);
        make1(LDR_X29, skip8);
        make1(COMMUTE, NULL);
        make1(ADD_SP, restack);
        make1(LABEL, skip, 0);
        add_label(skip, -1);
        free(skip8);
        free(skip);
    }
    if (inloop) {
        tmp = new_name("res");
        add_label(tmp, -1);
        make1(LABEL, tmp, 0);
    }
    if (attr & ATTRIB_STDCALL) {
        cry("attribute stdcall not implemented\n");
    }
    switch (op) {
        case BLR_X19:
            args += rargs;
            make1(op, args[3], args[2], args[1], args[0], args[4]);
            break;
        case BR_X3:
            make1(op);
            break;
        case BR_X16:
            make1(op, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
            break;
        default:
            assert(0);
    }
    if (tmp) {
        char *sav;
        char *gdg = new_name("gdg");
        if (retval) {
            sav = emit_save();
        }
        add_extern(gdg, optab[op].addr, 0, -1);
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
    make1(MOV_X1_X0);
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
    make1(MOV_X0_X1);
    (void)scratch;
#endif
}


void
emit_goto(const char *label)
{
    char *label8 = create_address_str(label, -8);
    make1(LDR_X29, label8);
    make1(COMMUTE, NULL);
    free(label8);
}


void
emit_cond(const char *label, enum cond_t cond)
{
    char *next = new_name("nxt");
    char *next8 = create_address_str(next, -8);
    char *label8 = create_address_str(label, -8);
    BOOL inverse = (cond == COND_EQ);
    assert(cond == COND_NE || cond == COND_EQ);
    make1(NOT_X0);
    if (inverse) {
        make1(LDR_X19, next8);
        make1(SEL_1);
        make1(LDR_X19, label8);
    } else {
        make1(LDR_X19, label8);
        make1(SEL_1);
        make1(LDR_X19, next8);
    }
    make1(SEL_2);
    make1(MOV_X1_X0);
    make1(LDR_X0, get_symbol("__gadget_nop")->key);
    make1(PIVOT, NULL);
    make1(LABEL, next, 0xFFFFFFFF);
    add_label(next, idx);
    free(label8);
    free(next8);
    free(next);
}


void
emit_label(const char *label, BOOL used)
{
    if (!used) {
        if (get_symbol(label)) { /* XXX maybe we took its address */
            make1(LABEL, label, 0);
            return;
        }
        cry("unused label '%s'\n", label);
        return;
    }
    add_label(label, idx);
    make1(LABEL, label, 0xFFFFFFFF);
}


void
emit_extern(const char *import, unsigned long long val, int attr, int regparm)
{
    /* should not emit anything, but add symbol as extern */
    printf(";;; extern %s\n", import);
    add_extern(import, (val != -1ULL) ? val : solve_import(import), attr, regparm);
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
    solve_op(LDR_X29);
    add_extern("__gadget_nop", optab[LDR_X29].addr, 0, -1);
    add_extern("__gadget_ret", optab[LDR_X29].addr + 4, 0, -1);
}


const char *
backend_name(void)
{
    return "ARM64";
}


int arch_regparm = 8;
