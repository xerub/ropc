//#define COND_BY_PIVOT
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

typedef char *BRICK;

enum R_OP {
    BAD,
    LABEL,
    LDR_X0,
    LDR_X1,
    LDR_X2,
    LDR_X3,
    LDR_X4,
    LDR_X19,
    LDR_X19X20,
    LDR_X29,
    LANDING,
    RET_X8,
    LDR_X0_X0,
    STR_X0_X19,
    MOV_X1_X0,
    MOV_X0_X1,
    OR_X0_reg,
    XOR_X0_reg,
    AND_X0_reg,
    ADD_X0_reg,
    SUB_X0_reg,
    MUL_X0_reg,
    DIV_X0_reg,
    BR_X3,
    BR_X4,
    BR_X16,
    BLR_X19,
    COMMUTE,
    SELECT,
#ifdef COND_BY_PIVOT
    PIVOT,
#endif
    BX_IMM,
    BX_IMM_1
};

#define GADGET_with_call        0x100
#define GADGET_stack_x29_10     0x200
#define GADGET_inverse          0x400
#define GADGET_zero_float       0x800

struct R_OPDEF {
    enum R_OP op;
    unsigned spill;     // which registers are undefined (NB: stack load is not considered spilling)
    unsigned output;    // which registers will be loaded from stack (NB: set of *all* registers loaded from stack)
    unsigned auxout;    // which registers will be loaded from stack as a side-effect (NB: subset of [output] registers which are not targeted by this op)
    int incsp;
    int flags;
    unsigned long long addr;
    const char *text;
};


static struct R_OPDEF optab[] = {
    { BAD,          /**/ 0,                              /**/ 0,                                              /**/ 0,           /**/  0, 0, 0, NULL },
    { LABEL,        /**/ 0,                              /**/ 0,                                              /**/ 0,           /**/  0, 0, 0, NULL },
    { LDR_X0,       /**/                            X29, /**/ X0,                                             /**/ 0,           /**/  0, 0, 0, "ldr x0, [sp] / blr x8" },
    { LDR_X1,       /**/                            X29, /**/    X1,                                          /**/ 0,           /**/  0, 0, 0, "ldr x1, [sp] / blr x8" },
    { LDR_X2,       /**/                            X29, /**/       X2,                                       /**/ 0,           /**/  0, 0, 0, "ldr x2, [sp] / blr x8" },
    { LDR_X3,       /**/                            X29, /**/          X3,                                    /**/ 0,           /**/  0, 0, 0, "ldr x3, [sp] / blr x8" },
    { LDR_X4,       /**/                            X29, /**/             X4,                                 /**/ 0,           /**/  0, 0, 0, "ldr x4, [sp] / blr x8" },
    { LDR_X19,      /**/ 0,                              /**/                            X19|X20|        X29, /**/     X20|X29, /**/  0, 0, 0, "ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret" },
    { LDR_X19X20,   /**/ 0,                              /**/                            X19|X20|        X29, /**/         X29, /**/  0, 0, 0, "ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret" },
    { LDR_X29,      /**/ 0,                              /**/                                            X29, /**/ 0,           /**/  0, 0, 0, "ldp x29, x30, [sp], #0x10 / ret" },
    { LANDING,      /**/ 0,                              /**/                                            X29, /**/         X29, /**/  0, 0, 0, "ldp x29, x30, [sp], #0x10 / ret" },
    { RET_X8,       /**/ 0,                              /**/                         X8,                     /**/ 0,           /**/  0, 0, 0, "ldr x8, [sp, #8] / blr x8" },
    { LDR_X0_X0,    /**/ X0|                        X29, /**/                                            X29, /**/         X29, /**/  0, 0, 0, "ldr x0, [x0] / blr x8" },
    { STR_X0_X19,   /**/ 0,                              /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, 0, 0, "str x0, [x19] / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret" },
    { MOV_X1_X0,    /**/    X1|                     X29, /**/                                            X29, /**/         X29, /**/  0, 0, 0, "mov x1, x0 / blr x8" },
    { MOV_X0_X1,    /**/ X0|                        X29, /**/                                            X29, /**/         X29, /**/  0, 0, 0, "mov x0, x1 / blr x8" },
    { OR_X0_reg,    /**/ X0,                             /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  2, LDR_X19, 0, "orr x0, x0, x19 / ldp x29, x30, [sp, #0x20] / ldp x20, x19, [sp, #0x10] / add sp, sp, #0x30 / ret" },
    { XOR_X0_reg,   /**/ X0,                             /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, LDR_X19, 0, "eor x0, x0, x19 / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret" },
    { AND_X0_reg,   /**/ X0,                             /**/                                            X29, /**/         X29, /**/  4, LDR_X1,  0, "and x0, x1, x0 / ldp x29, x30, [sp, #0x20] / add sp, sp, #0x30 / ret" },
    { ADD_X0_reg,   /**/ X0,                             /**/                                            X29, /**/         X29, /**/  0, LDR_X1,  0, "add x0, x0, x1 / ldp x29, x30, [sp], #0x10 / ret" },
    { SUB_X0_reg,   /**/ X0,                             /**/                                            X29, /**/         X29, /**/  0, LDR_X1,  0, "sub x0, x0, x1 / ldp x29, x30, [sp], #0x10 / ret" },
    { MUL_X0_reg,   /**/ X0,                             /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, LDR_X19, 0, "mul x0, x0, x19 / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret" },
    { DIV_X0_reg,   /**/ X0,                             /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, LDR_X19, 0, "udiv x0, x0, x19 / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret" },
    { BR_X3,        /**/ X0|X1|X2|X3|X4|X5|X6|X7|X8,     /**/                                            X29, /**/         X29, /**/  0, 0, 0, "ldp x29, x30, [sp], #0x10 / br x3" },
    { BR_X4,        /**/ X0|X1|X2|X3|X4|X5|X6|X7|X8,     /**/                                            X29, /**/         X29, /**/  0, 0, 0, "ldp x29, x30, [sp], #0x10 / br x4" },
    { BR_X16,       /**/ X0|X1|X2|X3|X4|X5|X6|X7|X8,     /**/ X0|X1|X2|X3|X4|X5|X6|X7|                   X29, /**/         X29, /**/  0, 0, 0, "mov x16, x0 / ldp x7, x6, [sp], #0x10 / ldp x5, x4, [sp], #0x10 / ldp x3, x2, [sp], #0x10 / ldp x1, x0, [sp], #0x10 / ldp x29, x30, [sp], #0x10 / br x16" },
    { BLR_X19,      /**/ X0|X1|X2|X3|X4|X5|X6|X7|X8,     /**/                            X19|X20|X21|X22|X29, /**/ 0,           /**/  0, 0, 0, "blr x19 / ldp x29, x30, [sp, #0x20] / ldp x20, x19, [sp, #0x10] / ldp x22, x21, [sp], #0x30 / ret" },
    { COMMUTE,      /**/ 0,                              /**/                                            X29, /**/         X29, /**/  0, 0, 0, "mov sp, x29 / ldp x29, x30, [sp], #0x10 / ret" },
    { SELECT,       /**/ X0,                             /**/                            X19|X20|        X29, /**/ X19|X20|X29, /**/  0, 0, 0, "cmp x0, #0 / csel x0, x19, x20, eq / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret" },
#ifdef COND_BY_PIVOT
    { PIVOT,        /**/ 0,                              /**/ SP, /* XXX needed to keep alignment */          /**/ 0,           /**/  0, 0, 0, "mov sp, x1 / br x0" },
#endif
    { BX_IMM,       /**/ 0xFFFFFFFF,                     /**/ 0,                                              /**/ 0,           /**/  0, 0, 0, NULL },
    { BX_IMM_1,     /**/ 0xFFFFFFFF,                     /**/ 0,                                              /**/ 0,           /**/  1, 0, 0, NULL },
};


static int idx = 0;
static BRICK strip[10240];

static unsigned dirty = 0xFFFFFFFF;
static int pointer[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static struct range *ranges = NULL;

const int arch_regparm = 8;


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


static struct R_OPDEF *
try_solve_op(enum R_OP op)
{
    uint64_t rv = 0;
    struct R_OPDEF *r = &optab[op];
    if (r->addr) {
        return r;
    }
    if (!binmap) {
        r->addr = 0xFF000000 + op;
        return r;
    }
    if (!ranges) {
        ranges = parse_ranges(binmap, binsz);
    }
    switch (op) {
        case BAD:
        case LABEL:
        case BX_IMM:
        case BX_IMM_1:
            return NULL;
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
        case LDR_X4:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E4 03 40 F9 00 01 3F D6");
            break;
        case LDR_X19:
        case LDR_X19X20:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            break;
        case LDR_X29:
        case LANDING:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ FD 7B C1 A8 C0 03 5F D6");
            break;
        case RET_X8:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E8 07 40 F9 00 01 3F D6");
            break;
        case LDR_X0_X0:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 40 F9 00 01 3F D6");
            break;
        case STR_X0_X19:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 60 02 00 F9 FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            break;
        case MOV_X1_X0:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E1 03 00 AA 00 01 3F D6");
            break;
        case MOV_X0_X1:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ E0 03 01 AA 00 01 3F D6");
            break;
        case OR_X0_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 13 AA FD 7B 42 A9 F4 4F 41 A9 FF C3 00 91 C0 03 5F D6");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 13 AA BF 43 00 D1 FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            if (rv) {
                r->incsp = 0;
                r->flags = LDR_X19 | GADGET_stack_x29_10;
                r->text = "orr x0, x0, x19 / sub sp, x29, #0x10 / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret";
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 20 00 00 AA C0 03 5F D6");
            if (rv) {
                r->output = 0;
                r->auxout = 0;
                r->incsp = 0;
                r->flags = LDR_X1 | GADGET_with_call;
                r->text = "orr x0, x1, x0 / ret";
                add_extern("__gadget_orr", rv, 0, -1);
                break;
            }
            break;
        case XOR_X0_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 13 CA FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            break;
        case AND_X0_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 20 00 00 8A FD 7B 42 A9 FF C3 00 91 C0 03 5F D6");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 13 8A BF 43 00 D1 FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            if (rv) {
                r->output = X19 | X20 | X29;
                r->auxout = X19 | X20 | X29;
                r->incsp = 0;
                r->flags = LDR_X19 | GADGET_stack_x29_10;
                r->text = "and x0, x0, x19 / sub sp, x29, #0x10 / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret";
                break;
            }
            break;
        case ADD_X0_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 01 8B FD 7B C1 A8 C0 03 5F D6");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 13 8B FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            if (rv) {
                r->output |= X19 | X20;
                r->auxout |= X19 | X20;
                r->flags = LDR_X19;
                r->text = "add x0, x0, x19 / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret";
                break;
            }
            break;
        case SUB_X0_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 01 CB FD 7B C1 A8 C0 03 5F D6");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 01 CB FD 7B 41 A9 FF 83 00 91 C0 03 5F D6");
            if (rv) {
                r->incsp = 2;
                r->text = "sub x0, x0, x1 / ldp x29, x30, [sp, #0x10] / add sp, sp, #0x20 / ret";
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 00 13 CB FD 7B 42 A9 F4 4F 41 A9 F6 57 C3 A8 C0 03 5F D6");
            if (rv) {
                r->output |= X19 | X20 | X21 | X22;
                r->auxout |= X19 | X20 | X21 | X22;
                r->flags = LDR_X19;
                r->text = "sub x0, x0, x19 / ldp x29, x30, [sp, #0x20] / ldp x20, x19, [sp, #0x10] / ldp x22, x21, [sp], #0x30 / ret";
                break;
            }
            break;
        case MUL_X0_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 7C 13 9B FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            break;
        case DIV_X0_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 08 D3 9A FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 08 D3 9A 02 00 00 14 00 00 80 D2 FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 00 08 C1 9A C0 03 5F D6");
            if (rv) {
                r->output = 0;
                r->auxout = 0;
                r->flags = LDR_X1 | GADGET_with_call;
                r->text = "udiv x0, x0, x1 / ret";
                add_extern("__gadget_div", rv, 0, -1);
                break;
            }
            break;
        case BR_X3:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ FD 7B C1 A8 60 00 1F D6");
            break;
        case BR_X4:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ FD 7B C1 A8 80 00 1F D6");
            break;
        case BR_X16:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ F0 03 00 AA E7 1B C1 A8 E5 13 C1 A8 E3 0B C1 A8 E1 03 C1 A8 FD 7B C1 A8 00 02 1F D6");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ F0 03 00 AA E7 1B C1 6C E5 13 C1 6C E3 0B C1 6C E1 03 C1 6C E7 1B C1 A8 E5 13 C1 A8 E3 0B C1 A8 E1 03 C1 A8 FD 7B C1 A8 00 02 1F D6");
            if (rv) {
                r->flags = GADGET_zero_float;
                r->incsp = 8;
                r->text = "mov x16, x0 / ldp d7, d6, [sp], #0x10 / ldp d5, d4, [sp], #0x10 / ldp d3, d2, [sp], #0x10 / ldp d1, d0, [sp], #0x10 / ldp x7, x6, [sp], #0x10 / ldp x5, x4, [sp], #0x10 / ldp x3, x2, [sp], #0x10 / ldp x1, x0, [sp], #0x10 / ldp x29, x30, [sp], #0x10 / br x16";
            }
            break;
        case BLR_X19:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 60 02 3F D6 FD 7B 42 A9 F4 4F 41 A9 F6 57 C3 A8 C0 03 5F D6");
            break;
        case COMMUTE:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ BF 03 00 91 FD 7B C1 A8 C0 03 5F D6");
            break;
        case SELECT:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 1F 00 00 F1 60 02 94 9A FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 1F 00 00 F1 80 12 93 9A FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6");
            if (rv) {
                r->text = "cmp x0, #0 / csel x0, x20, x19, ne / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret";
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 1f 00 00 f1 80 02 93 9a fd 7b 41 a9 f4 4f c2 a8 c0 03 5f d6");
            if (rv) {
                r->flags = GADGET_inverse;
                r->text = "cmp x0, #0 / csel x0, x20, x19, eq / ldp x29, x30, [sp, #0x10] / ldp x20, x19, [sp], #0x20 / ret";
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 1F 00 00 F1 60 02 94 9A FD 7B 42 A9 F4 4F 41 A9 F6 57 C3 A8 C0 03 5F D6");
            if (rv) {
                r->output |= X21 | X22;
                r->auxout |= X21 | X22;
                r->text = "cmp x0, #0 / csel x0, x19, x20, eq / ldp x29, x30, [sp, #0x20] / ldp x20, x19, [sp, #0x10] / ldp x22, x21, [sp], #0x30 / ret";
                break;
            }
            break;
#ifdef COND_BY_PIVOT
        case PIVOT:
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 3F 00 00 91 00 00 1F D6");
            break;
#endif
    }
    r->addr = rv;
    return r;
}


static const struct R_OPDEF *
solve_op(enum R_OP op)
{
    struct R_OPDEF *r = try_solve_op(op);
    if (r && !r->addr) {
        die("undefined gadget '%s'\n", r->text);
    }
    return r;
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
    if (r->flags & GADGET_zero_float) {
        cry("floating point registers are zeroed before this call\n");
    }
    switch (op) {
        case LABEL:
            strip[idx++] = (BRICK)op;
            strip[idx++] = argdup(va_arg(ap, char *));
            spill = va_arg(ap, unsigned);
            strip[idx++] = (BRICK)(unsigned long)spill;
            add_label(strip[idx - 2], idx - 3);
            break;
        case LDR_X0:
        case LDR_X1:
        case LDR_X2:
        case LDR_X3:
        case LDR_X4:
            i = op - LDR_X0;
            if (optimize_reg && pointer[i]) {
                strip[pointer[i]] = argdup(va_arg(ap, char *));
                pointer[i] = 0;
                goto done;
            }
            make1(RET_X8, NULL);
            strip[idx++] = (BRICK)op;
            break;
        case LDR_X19:
            if (optimize_reg && pointer[19]) {
                strip[pointer[19]] = argdup(va_arg(ap, char *));
                pointer[19] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_X19X20:
            if (optimize_reg && pointer[19] && pointer[20]) {
                strip[pointer[19]] = argdup(va_arg(ap, char *));
                strip[pointer[20]] = argdup(va_arg(ap, char *));
                pointer[19] = 0;
                pointer[20] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_X29:
            if (optimize_reg && pointer[29]) {
                strip[pointer[29]] = argdup(va_arg(ap, char *));
                pointer[29] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case RET_X8:
            if (optimize_reg && !(dirty & X8)) {
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_X0_X0:
        case MOV_X1_X0:
        case MOV_X0_X1:
            make1(RET_X8, NULL);
        case LANDING:
        case STR_X0_X19:
        case OR_X0_reg:
        case XOR_X0_reg:
        case AND_X0_reg:
        case ADD_X0_reg:
        case SUB_X0_reg:
        case MUL_X0_reg:
        case DIV_X0_reg:
        case BR_X3:
        case BR_X4:
        case BR_X16:
        case BLR_X19:
        case SELECT:
#ifdef COND_BY_PIVOT
        case PIVOT:
#endif
            strip[idx++] = (BRICK)op;
            for (i = 0; i < r->incsp; i++) {
                strip[idx++] = NULL;
            }
            break;
        case COMMUTE:
            strip[idx++] = (BRICK)op;
            strip[idx++] = (BRICK)(long)va_arg(ap, int);
            break;
        case BX_IMM:
        case BX_IMM_1: {
            char *func = argdup(va_arg(ap, char *));
            char **args = va_arg(ap, char **);
            op += ((op - BX_IMM) | 1); // XXX stack alignment
            strip[idx++] = (BRICK)op;
            strip[idx++] = func;
            for (i = 0; i < r->incsp; i++) {
                strip[idx++] = argdup(args[i]);
            }
            for (; i < optab[op].incsp; i++) {
                strip[idx++] = NULL;
            }
            break;
        }
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
        make1(LANDING, NULL);
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
            printx("        du    %-30s; -> %c%d\n", na, ch, i);
            free(na);
            return;
        }
        p = get_symbol(arg);
        if (p) {
            if (p->type == SYMBOL_EXTERN) {
                printx("        dg    0x%-28llX; -> %c%d: %s\n", p->addr, ch, i, arg);
            } else if (p->type == SYMBOL_LABEL) {
                printx("        du    %-30s; -> %c%d\n", p->key, ch, i);
            } else {
                assert(p->type == SYMBOL_NORMAL);
                if (p->val && is_address(p->val)) {
                    char *na = curate_address(p->val);
                    printx("%-7s du    %-30s; -> %c%d\n", arg, na, ch, i);
                    free(na);
                } else if (p->val && try_symbol_extern(p->val)) {
                    printx("%-7s dg    0x%-28llX; -> %s\n", arg, get_symbol(p->val)->addr, p->val);
                } else {
                    printx("%-7s dd    %-30s; -> %c%d\n", arg, p->val ? p->val : "0", ch, i);
                }
            }
            return;
        }
    }
    printx("        dd    %-30s; -> %c%d\n", arg ? arg : "0", ch, i);
}


void
emit_finalize(void)
{
    int i;
    unsigned sc = 0;
    const BRICK *p = strip;
    const BRICK *q = p + idx;
    BRICK value[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    dirty = 0xFFFFFFFF;
    while (p < q) {
        enum R_OP op = (enum R_OP)(long)(*p++);
        const struct R_OPDEF *r = &optab[op];
        unsigned spill = r->spill;
        if (op == BX_IMM || op == BX_IMM_1) {
            BRICK arg = *p++;
            const struct SYM *p = get_symbol(arg);
            assert(p);
            if (p->type != SYMBOL_EXTERN) {
                printx("%-7s dd    %-30s; -> PC\n", arg, p->val ? p->val : "0");
                sc++;
            } else {
                printx("        dg    0x%-28llX; -> PC: %s\n", p->addr, arg);
                sc++;
            }
            free(arg);
        } else if (op == LABEL) {
            BRICK arg = *p++;
            spill = (unsigned long)*p++;
            printx("%s:\n", arg);
            free(arg);
            if (!spill) {
                // soft label: do not affect dirty set
                continue;
            }
            // we can eliminate superfluous LANDING here
        } else {
            printx("        dg    0x%-28llX; -> PC: %s\n", r->addr, r->text);
            sc++;
        }
        if (op == COMMUTE) {
            long restack = (long)*p++;
            if (restack < 0) {
                restack = inloop_stack;
            }
            if (restack) {
                printx("        times 0x%lx dd 0\n", restack);
                sc += restack;
            }
        }
        switch (op) {
            case LABEL:
            case LDR_X0:
            case LDR_X1:
            case LDR_X2:
            case LDR_X3:
            case LDR_X4:
            case LDR_X19:
            case LDR_X19X20:
            case LDR_X29:
            case LANDING:
            case RET_X8:
            case LDR_X0_X0:
            case STR_X0_X19:
            case MOV_X1_X0:
            case MOV_X0_X1:
            case OR_X0_reg:
            case XOR_X0_reg:
            case AND_X0_reg:
            case ADD_X0_reg:
            case SUB_X0_reg:
            case MUL_X0_reg:
            case DIV_X0_reg:
            case BR_X3:
            case BR_X4:
            case BR_X16:
            case BLR_X19:
            case COMMUTE:
            case SELECT:
#ifdef COND_BY_PIVOT
            case PIVOT:
#endif
            case BX_IMM:
            case BX_IMM_1:
                for (i = 0; i < r->incsp; i++) {
                    play_data(*p, i, 'A');
                    sc++;
                    free(*p);
                    p++;
                }
                for (i = 0; i < 32; i++) {
                    if (r->output & (1 << i)) {
                        free(value[i]);
                        value[i] = *p++;
                    }
                }
                for (i = 28; i >= 0; i--) {
                    if (r->output & (1 << i)) {
                        play_data(value[i], i, 'X');
                        sc++;
                    }
                }
                for (i = 29; i < 32; i++) {
                    if (r->output & (1 << i)) {
                        play_data(value[i], i, 'X');
                        sc++;
                    }
                }
                break;
            default:
                assert(0);
        }
        dirty &= ~r->output;
        dirty |= spill;
        if (show_reg_set) {
            printx("; +0x%08x:", sc * 8);
            for (i = 0; i < 32; i++) {
                if (!(dirty & (1 << i))) {
                    if (i > 8 && i != 16 && i != 19 && i != 20 && i != 29) {
                        continue;
                    }
                    printx(" X%d=%s", i, value[i] ? value[i] : "0");
                } else {
                    free(value[i]);
                    value[i] = NULL;
                }
            }
            printx("\n");
        }
    }
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
emit_or(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(OR_X0_reg);
    make1(r->flags & 0xFF, value);
    make1(LDR_X0, value);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    if (r->flags & GADGET_stack_x29_10) {
        char *tmp = new_name("stk");
        char *reload = create_address_str(tmp, -8);
        make1(LDR_X29, reload);
        make1(r->op);
        make1(LABEL, tmp, 0);
        free(reload);
        free(tmp);
        return;
    }
    if (r->flags & GADGET_with_call) {
        make1(LDR_X3, "__gadget_orr");
        make1(BR_X3);
        return;
    }
    make1(r->op);
}


void
emit_xor(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(XOR_X0_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_X0, value);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    make1(r->op);
}


void
emit_and(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(AND_X0_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_X0, value);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    if (r->flags & GADGET_stack_x29_10) {
        char *tmp = new_name("stk");
        char *reload = create_address_str(tmp, -8);
        make1(LDR_X29, reload);
        make1(r->op);
        make1(LABEL, tmp, 0);
        free(reload);
        free(tmp);
        return;
    }
    make1(r->op);
}


void
emit_add(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(ADD_X0_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_X0, value);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    make1(r->op);
}


void
emit_sub(const char *value, const char *addend, int deref0)
{
    const struct R_OPDEF *r;
    r = solve_op(SUB_X0_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_X0, value);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    make1(r->op);
}


void
emit_mul(const char *value, const char *multiplier, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, multiplier);
    r = solve_op(MUL_X0_reg);
    make1(r->flags & 0xFF, multiplier);
    make1(LDR_X0, value);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    make1(r->op);
}


void
emit_div(const char *value, const char *multiplier, int deref0)
{
    const struct R_OPDEF *r;
    r = solve_op(DIV_X0_reg);
    make1(r->flags & 0xFF, multiplier);
    make1(LDR_X0, value);
    while (deref0--) {
        make1(LDR_X0_X0);
    }
    if (r->flags & GADGET_with_call) {
        make1(LDR_X3, "__gadget_div");
        make1(BR_X3);
        return;
    }
    make1(r->op);
}


void
emit_call(const char *func, char **args, int nargs, int deref0, BOOL inloop, BOOL retval, int attr, int regparm, int restack)
{
    int has_x4 = 1; // XXX optimistic
    char *tmp = NULL;
    enum R_OP op = BLR_X19;
    int rargs = nargs;
    if (args == NULL) {
        rargs = nargs = 0;
    }
    if (rargs > regparm) {
        rargs = regparm;
    }
    assert(rargs <= 8 && nargs - rargs <= 5);
    if (nargs > rargs || ((deref0 || (attr & ATTRIB_STACK) || inloop) && rargs > 3)) {
        if (rargs <= 3 + has_x4) {
            switch (rargs) {
                case 4:
                    make1(LDR_X3, args[3]);
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
        if (rargs <= 3 + has_x4) {
            switch (rargs) {
                case 4:
                    make1(LDR_X3, args[3]);
                case 3:
                    make1(LDR_X2, args[2]);
                case 2:
                    make1(LDR_X1, args[1]);
                case 1:
                    make1(LDR_X0, args[0]);
                case 0:
                    break;
            }
            op = BR_X3 + has_x4;
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
        case BR_X4:
            make1(LDR_X4, func);
            break;
        case BR_X16:
            make1(LDR_X0, func);
            break;
        default:
            assert(0);
    }
    if ((attr & ATTRIB_STACK) || inloop) {
        char *skip;
        tmp = new_name("res");
        skip = create_address_str(tmp, -8);
        make1(LDR_X29, skip);
        make1(COMMUTE, restack);
        make1(LABEL, tmp, X29);
        free(skip);
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
        case BR_X4:
            make1(op);
            break;
        case BR_X16:
            make1(op, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
            break;
        default:
            assert(0);
    }
    if (inloop) {
        char *sav;
        char *gdg = new_name("gdg");
        if (retval) {
            sav = emit_save();
        }
        add_extern(gdg, optab[op].addr, 0, -1);
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
    make1(COMMUTE, 0);
    free(label8);
}


void
emit_cond(const char *label, enum cond_t cond)
{
    const struct R_OPDEF *r = solve_op(SELECT);
    BOOL ginverse = !!(r->flags & GADGET_inverse);
#ifndef COND_BY_PIVOT
    char *dst = new_name("dst");
    char *next = new_name("nxt");
    char *pivot = create_address_str(dst, -8);
    char *next8 = create_address_str(next, -8);
    char *label8 = create_address_str(label, -8);
    BOOL inverse = (cond == COND_EQ) ^ ginverse;
    assert(cond == COND_NE || cond == COND_EQ);
    if (inverse) {
        make1(LDR_X19X20, label8, next8);
    } else {
        make1(LDR_X19X20, next8, label8);
    }
    make1(SELECT);
    emit_store_direct(pivot);
    if ((optab[STR_X0_X19].output & X29) == 0) {
        make1(LANDING);
    }
    make1(LABEL, dst, 0);
    make1(COMMUTE, 0);
    make1(LABEL, next, 0);
    free(label8);
    free(next8);
    free(pivot);
    free(next);
    free(dst);
#else
    char *next = new_name("nxt");
    char *next8 = create_address_str(next, -8);
    char *label8 = create_address_str(label, -8);
    BOOL inverse = (cond == COND_EQ) ^ ginverse;
    assert(cond == COND_NE || cond == COND_EQ);
    if (inverse) {
        make1(LDR_X19X20, label8, next8);
    } else {
        make1(LDR_X19X20, next8, label8);
    }
    make1(SELECT);
    make1(MOV_X1_X0);
    make1(LDR_X0, get_symbol("__gadget_nop")->key);
    make1(PIVOT, NULL);
    make1(LABEL, next, 0/*xFFFFFFFF*/);
    free(label8);
    free(next8);
    free(next);
#endif
}


void
emit_label(const char *label, int used, BOOL last)
{
    if (!used) {
        make1(LABEL, label, 0);
        cry("unused label '%s'\n", label);
    } else if (last) {
        make1(LABEL, label, 0xFFFFFFFF);
#ifndef COND_BY_PIVOT
        /* make1(LANDING); // XXX no need, because all gadgets pop X29 anyway */
#endif
    } else {
        make1(LABEL, label, 0);
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
    solve_op(LDR_X29);
    add_extern("__gadget_nop", optab[LDR_X29].addr, 0, -1); // XXX works because all gadgets pop X29, X30
    add_extern("__gadget_ret", optab[LDR_X29].addr + 4, 0, -1);
}


int
backend_test_gadgets(int verbose)
{
    size_t i;
    int missing = 0;
    for (i = 0; i < sizeof(optab) / sizeof(optab[0]); i++) {
        struct R_OPDEF *r = try_solve_op(i);
        if (!r) {
            continue;
        }
        if (!r->addr) {
            printf("WARNING: undefined gadget '%s'\n", r->text);
            missing = 1;
            continue;
        }
        if (verbose) {
            printf("GADGET: 0x%llx: %s\n", r->addr, r->text);
            if (verbose > 1) {
                printf("\tspill: 0x%x, output: 0x%x, auxout: 0x%x, incsp: %d, flags: 0x%x\n", r->spill, r->output, r->auxout, r->incsp, r->flags);
            }
        }
    }
    return missing;
}


const char *
backend_name(void)
{
    return "ARM64";
}
