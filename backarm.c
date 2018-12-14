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


#define R0 (1 << 0)
#define R1 (1 << 1)
#define R2 (1 << 2)
#define R3 (1 << 3)
#define R4 (1 << 4)
#define R5 (1 << 5)
#define R6 (1 << 6)
#define R7 (1 << 7)
#define R8 (1 << 8)
#define R9 (1 << 9)
#define R10 (1 << 10)
#define R11 (1 << 11)
#define IP (1 << 12)
#define SP (1 << 13)
#define LR (1 << 14)
#define PC (1 << 15)

typedef char *BRICK; // XXX make it elt

enum R_OP {
    NOP,
    LABEL,
    LDR_R4,
    LDR_R0,
    LDR_R0R1,
    LDR_R0TO3,
    LDR_R0_R0,
    ADD_R0_R1,
    BLX_R4,
    BLX_R4_1,
    BLX_R4_2,
    BLX_R4_3,
    BLX_R4_4,
    BLX_R4_5,
    BLX_R4_6,
    STR_R0_R4,
    CMP_R0,
    LDR_R4R5,
    LDMIA_R0,
    LDMIA_R0_C,
    MOV_R1_R0,
    MOV_R0_R1,
    ADD_SP,
    BX_IMM,
    BX_IMM_1
};

struct R_OPDEF {
    enum R_OP op;
    unsigned spill;     // which registers are undefined
    unsigned output;    // which registers will be loaded from stack
    unsigned auxout;    // which registers will be loaded from stack as a side-effect
    int incsp;
    uint64_t addr;
    const char *text;
};


static struct R_OPDEF optab[] = {
    { NOP },
    { LABEL,        /**/ 0xFFFF,      /**/ 0,                          /**/ 0,                    /**/ 0,   0, NULL,                                                                 },
    { LDR_R4,       /**/ 0,           /**/             R4,             /**/ 0,                    /**/ 0,   0, "POP {R4,PC}",                                                        },
    { LDR_R0,       /**/ 0,           /**/ R0,                         /**/ 0,                    /**/ 0,   0, "POP {R0,PC}",                                                        },
    { LDR_R0R1,     /**/ 0,           /**/ R0|R1,                      /**/ 0,                    /**/ 0,   0, "POP {R0-R1,PC}",                                                     },
    { LDR_R0TO3,    /**/ 0,           /**/ R0|R1|R2|R3,                /**/ 0,                    /**/ 0,   0, "POP {R0-R3,PC}",                                                     },
    { LDR_R0_R0,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/ 0,   0, "LDR R0, [R0] / POP {R7,PC}",                                         },
    { ADD_R0_R1,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/ 0,   0, "ADD R0, R1 / POP {R7,PC}",                                           },
    { BLX_R4,       /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/ 0,   0, "BLX R4 / POP {R4,R7,PC}",                                            },
    { BLX_R4_1,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/ 1,   0, "BLX R4 / ADD SP, #4 / POP {...PC}",                                  },
    { BLX_R4_2,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/ 2,   0, "BLX R4 / ADD SP, #8 / POP {...PC}",                                  },
    { BLX_R4_3,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/ 3,   0, "BLX R4 / ADD SP, #12 / POP {...PC}",                                 },
    { BLX_R4_4,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/ 4,   0, "BLX R4 / ADD SP, #16 / POP {...PC}",                                 },
    { BLX_R4_5,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/ 5,   0, "BLX R4 / ADD SP, #20 / POP {...PC}",                                 },
    { BLX_R4_6,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/ 6,   0, "BLX R4 / ADD SP, #24 / POP {...PC}",                                 },
    { STR_R0_R4,    /**/ 0,           /**/             R4|   R7,       /**/             R4|   R7, /**/ 0,   0, "STR R0, [R4] / POP {R4,R7,PC}",                                      },
    { CMP_R0,       /**/ R0,          /**/             R4|R5|R7,       /**/             R4|R5|R7, /**/ 0,   0, "CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}", },    // (R0 = R0 ? R4 : R5)
    { LDR_R4R5,     /**/ 0,           /**/             R4|R5,          /**/ 0,                    /**/ 0,   0, "POP {R4,R5,PC}",                                                     },
    { LDMIA_R0,     /**/ 0,           /**/ R0|         R4|      SP|PC, /**/ R0|         R4,       /**/ 0,   0, "LDMIA R0, {...SP...PC}",                                             },
    { LDMIA_R0_C,   /**/ 0,           /**/ R0|         R4|      SP|PC, /**/ R0|         R4,       /**/ 0,   0, "LDMIA R0, {...SP...PC}",                                             },
    { MOV_R1_R0,    /**/    R1,       /**/             R4|   R7,       /**/             R4|   R7, /**/ 0,   0, "MOV R1, R0 / POP {...PC}",                                           },
    { MOV_R0_R1,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/ 0,   0, "MOV R0, R1 / POP {R7,PC}",                                           },
    { ADD_SP,       /**/ R7,          /**/                   R7,       /**/                   R7, /**/ 256, 0, "ADD SP, #? / POP {R7,PC}",                                           },    // XXX no R0-R4 allowed
    { BX_IMM,       /**/ 0xFFFF,      /**/ 0,                          /**/ 0,                    /**/ 0,   0, NULL,                                                                 },
    { BX_IMM_1,     /**/ 0xFFFF,      /**/ 0,                          /**/ 0,                    /**/ 1,   0, NULL,                                                                 },
};


static int idx = 0;
static BRICK strip[1024];

static unsigned dirty = 0xFFFF;
static int pointer[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

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


static int
solve_MOV_Rx_R0(const unsigned char *p, uint32_t size, va_list ap, uint64_t addr, void *user)
{
    uint64_t pp[1];
    int rv = is_MOV_Rx_R0(p, size, ap, addr, pp);
    if (rv && user) {
        const unsigned char *ptr = (unsigned char *)(uintptr_t)pp[0];
        assert(rv & 1);
        if (rv < 0) {
            int reg = popcount(*ptr);
            int min_reg = ((uint64_t *)user)[2];
            if (min_reg <= reg) {
                return rv;
            }
            ((uint64_t *)user)[2] = reg;
        }
        ((uint64_t *)user)[0] = *ptr;
        ((uint64_t *)user)[1] = addr + (rv & 1);
    }
    return rv;
}


static int
solve_ADD_SP(const unsigned char *p, uint32_t size, va_list ap, uint64_t addr, void *user)
{
    uint64_t pp[2];
    int rv = is_ADD_SP(p, size, ap, addr, pp);
    if (rv && user) {
        const unsigned char *ptr = (unsigned char *)(uintptr_t)pp[0];
        assert(rv & 1);
        if (rv < 0) {
            int adj = pp[1];
            int min_adj = ((uint64_t *)user)[3];
            if (min_adj <= adj) {
                return rv;
            }
            ((uint64_t *)user)[3] = adj;
        }
        ((uint64_t *)user)[0] = *ptr;
        ((uint64_t *)user)[1] = addr + (rv & 1);
        ((uint64_t *)user)[2] = pp[1];
    }
    return rv;
}


static int
solve_BLX_R4_SP(const unsigned char *p, uint32_t size, va_list ap, uint64_t addr, void *user)
{
    uint64_t pp[1];
    int rv = is_BLX_R4_SP(p, size, ap, addr, pp);
    if (rv && user) {
        const unsigned char *ptr = (unsigned char *)(uintptr_t)pp[0];
        assert(rv & 1);
        if (rv < 0) {
            int reg = popcount(*ptr);
            int min_reg = ((uint64_t *)user)[2];
            if (min_reg <= reg) {
                return rv;
            }
            ((uint64_t *)user)[2] = reg;
        }
        ((uint64_t *)user)[0] = *ptr;
        ((uint64_t *)user)[1] = addr + (rv & 1);
    }
    return rv;
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
        case BX_IMM:
        case BX_IMM_1:
            return;
        case LDR_R4:
            rv = parse_gadgets(ranges, binmap, NULL, is_LOAD_R4);
            break;
        case LDR_R0:
            rv = parse_gadgets(ranges, binmap, NULL, is_LOAD_R0);
            break;
        case LDR_R0R1:
            rv = parse_gadgets(ranges, binmap, NULL, is_LOAD_R0R1);
            break;
        case LDR_R0TO3:
            rv = parse_gadgets(ranges, binmap, NULL, is_LOAD_R0R3);
            break;
        case LDR_R0_R0:
            rv = parse_gadgets(ranges, binmap, NULL, is_LDR_R0_R0);
            break;
        case ADD_R0_R1:
            rv = parse_gadgets(ranges, binmap, NULL, is_ADD_R0_R1);
            break;
        case MOV_R1_R0: {
            uint64_t pp[3] = { 0, 0, 8 + 1 };
            rv = parse_gadgets(ranges, binmap, pp, solve_MOV_Rx_R0, 1);
            if (pp[1]) {
                rv = pp[1];
                r->output = pp[0];
                r->auxout = r->output;
            }
            break;
        }
        case MOV_R0_R1:
            rv = parse_gadgets(ranges, binmap, NULL, is_MOV_R0_Rx, 1);
            break;
        case ADD_SP: {
            uint64_t pp[4] = { 0, 0, 0, 65536 };
            rv = parse_gadgets(ranges, binmap, pp, solve_ADD_SP, inloop_stack);
            if (pp[1]) {
                rv = pp[1];
                r->output = pp[0];
                r->auxout = r->output;
                r->spill = r->output;
                r->incsp = pp[2];
            }
            break;
        }
        case BLX_R4:
            rv = parse_gadgets(ranges, binmap, NULL, is_BLX_R4);
            break;
        case BLX_R4_1:
        case BLX_R4_2:
        case BLX_R4_3:
        case BLX_R4_4:
        case BLX_R4_5:
        case BLX_R4_6: {
            uint64_t pp[3] = { 0, 0, 8 + 1 };
            rv = parse_gadgets(ranges, binmap, pp, solve_BLX_R4_SP, r->incsp);
            if (pp[1]) {
                rv = pp[1];
                r->output = pp[0];
                r->auxout = r->output;
            }
            break;
        }
        case STR_R0_R4:
            rv = parse_gadgets(ranges, binmap, NULL, is_STR_R0_R4);
            break;
        case CMP_R0:
            rv = parse_gadgets(ranges, binmap, NULL, is_COMPARE);
            break;
        case LDR_R4R5:
            rv = parse_gadgets(ranges, binmap, NULL, is_LOAD_R4R5);
            break;
        case LDMIA_R0:
        case LDMIA_R0_C: {
            uint64_t pp[1];
            rv = parse_gadgets(ranges, binmap, pp, is_ldmia, 0, -1, R0|R4);
            if (rv) {
                const unsigned char *ptr = (unsigned char *)(uintptr_t)pp[0];
                r->output = (ptr[1] << 8) | ptr[0];
                r->auxout = r->output & ~(SP|PC);
            } else {
                rv = parse_gadgets(ranges, binmap, pp, is_ldmiaw, 0, -1, 0);
                if (rv) {
                    const unsigned char *ptr = (unsigned char *)(uintptr_t)pp[0];
                    r->output = (ptr[3] << 8) | ptr[2];
                    r->auxout = r->output & ~(SP|PC);
                }
            }
            break;
        }
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
    assert(idx < 1000);
    solve_op(op);
    switch (op) {
        case LDR_R0:
            if (optimize_reg && pointer[0]) { // XXX && !dirty(0)
                strip[pointer[0]] = argdup(va_arg(ap, char *));
                pointer[0] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R0R1:
            if (optimize_reg && pointer[0] && pointer[1]) { // XXX && !dirty(0) && !dirty(1)
                strip[pointer[0]] = argdup(va_arg(ap, char *));
                strip[pointer[1]] = argdup(va_arg(ap, char *));
                pointer[0] = 0;
                pointer[1] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R0TO3:
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R0_R0:
            strip[idx++] = (BRICK)op;
            break;
        case ADD_R0_R1:
        case MOV_R1_R0:
        case MOV_R0_R1:
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R4:
            if (optimize_reg && pointer[4]) { // XXX && !dirty(4)
                strip[pointer[4]] = argdup(va_arg(ap, char *));
                pointer[4] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R4R5:
            if (optimize_reg && pointer[4] && pointer[5]) { // XXX && !dirty(4) && !dirty(5)
                strip[pointer[4]] = argdup(va_arg(ap, char *));
                strip[pointer[5]] = argdup(va_arg(ap, char *));
                pointer[4] = 0;
                pointer[5] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case ADD_SP:
            strip[idx++] = (BRICK)op;
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
        case BLX_R4:
        case BLX_R4_1:
        case BLX_R4_2:
        case BLX_R4_3:
        case BLX_R4_4:
        case BLX_R4_5:
        case BLX_R4_6: {
            char **args = va_arg(ap, char **);
            strip[idx++] = (BRICK)op;
            for (i = 0; i < r->incsp; i++) {
                strip[idx++] = argdup(args[i]);
            }
            break;
        }
        case STR_R0_R4:
            strip[idx++] = (BRICK)op;
            break;
        case LABEL:
            strip[idx++] = (BRICK)op;
            strip[idx++] = argdup(va_arg(ap, char *));
            spill = va_arg(ap, unsigned);
            strip[idx++] = (BRICK)(unsigned long)spill;
            break;
        case CMP_R0:
            strip[idx++] = (BRICK)op;
            break;
        case LDMIA_R0:
            strip[idx++] = (BRICK)op;
            strip[idx++] = argdup(va_arg(ap, char *));
            break;
        case LDMIA_R0_C:
            strip[idx++] = (BRICK)op;
            strip[idx++] = argdup(va_arg(ap, char *));
            for (i = 0; i < 16; i++) {
                if (r->output & (1 << i)) {
                    if (r->auxout & (1 << i)) {
                        strip[idx] = NULL;
                    } else {
                        strip[idx] = argdup(va_arg(ap, char *));
                    }
                    idx++;
                }
            }
            strip[idx++] = argdup(va_arg(ap, char *));
            break;
        default:
            assert(0);
    }
    dirty &= ~r->output;
    dirty |= spill;
    for (i = 0; i < 16; i++) {
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
  done:
    va_end(ap);
    return idx;
}


static void
play_data(const char *arg, int i, int ch)
{
    if (arg) {
        const struct SYM *p;
        if (i == 13) {
            p = get_symbol(arg);
            if (!p || p->type != SYMBOL_LABEL) {
                die("cannot find label '%s'\n", arg);
            }
            printf("        du    4 + %-26s; -> SP\n", arg);
            return;
        }
        if (i == 15) {
            int j, k;
            p = get_symbol(arg);
            if (!p || p->type != SYMBOL_LABEL) {
                die("cannot find label '%s'\n", arg);
            }
            k = p->idx;
            do {
                k += 3;
                assert(k < idx);
                j = (int)(long)strip[k];
            } while (j == LABEL);
            printf("        dg    0x%-28llX; -> PC: %s\n", optab[j].addr, optab[j].text);
            return;
        }
        if (IS_ADDRESS(arg)) {
            printf("        du    %-30s; -> %c%d\n", arg + 1, ch, i);
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
                if (p->val && IS_ADDRESS(p->val)) {
                    printf("%-7s du    %-30s; -> %c%d\n", arg, p->val + 1, ch, i);
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
    BRICK value[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    dirty = 0xFFFF;
    while (p < q) {
        enum R_OP op = (enum R_OP)(long)(*p++);
        const struct R_OPDEF *r = &optab[op];
        int ldmia_reuse = 0;
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
        } else {
            printf("        dg    0x%-28llX; -> PC: %s\n", r->addr, r->text);
        }
        if (op == ADD_SP) {
            printf("        times 0x%x dd 0\n", r->incsp);
        }
        if (op == LDMIA_R0 || op == LDMIA_R0_C) {
            // print internal label for first register set
            BRICK arg = *p++;
            if (arg) {
                printf("%s:\n", arg);
                free(arg);
            } else {
                ldmia_reuse = 1;
            }
        }
        switch (op) {
            case LDMIA_R0_C: {
                BRICK arg;
                for (i = 0; i < 16; i++) {
                    if (r->output & (1 << i)) {
                        free(value[i]);
                        value[i] = *p++;
                        if (ldmia_reuse) {
                            continue;
                        }
                        play_data(value[i], i, 'R');
                    }
                }
                ldmia_reuse = 0;
                // print internal label for second register set
                arg = *p++;
                printf("%s:\n", arg);
                free(arg);
            }
            case BX_IMM:
            case BX_IMM_1:
            case BLX_R4:
            case BLX_R4_1:
            case BLX_R4_2:
            case BLX_R4_3:
            case BLX_R4_4:
            case BLX_R4_5:
            case BLX_R4_6:
                for (i = 0; i < r->incsp; i++) {
                    play_data(*p, i, 'A');
                    free(*p);
                    p++;
                }
            case ADD_SP:
            case LDR_R4:
            case LDR_R0:
            case LDR_R0R1:
            case LDR_R0TO3:
            case LDR_R0_R0:
            case ADD_R0_R1:
            case MOV_R1_R0:
            case MOV_R0_R1:
            case STR_R0_R4:
            case LDR_R4R5:
            case CMP_R0:
            case LDMIA_R0:
                for (i = 0; i < 16; i++) {
                    if (r->output & (1 << i)) {
                        free(value[i]);
                        value[i] = *p++;
                        if (ldmia_reuse) {
                            continue;
                        }
                        if (op != LDMIA_R0_C || i != 15) {
                            // do not show second PC for double-LDMIA
                            play_data(value[i], i, 'R');
                        }
                    }
                }
                break;
            default:
                assert(0);
        }
        if (op == LDMIA_R0_C) {
            assert(value[15]);
            // internal label follows a double-LDMIA, print it now
            printf("%s:\n", value[15]);
        }
      cont:
        dirty &= ~r->output;
        dirty |= r->spill;
        if (show_reg_set) {
            printf(";");
            for (i = 0; i < 8; i++) { /* XXX we do not care about high-order registers */
                if (!(dirty & (1 << i))) {
                    printf(" R%d=%s", i, value[i] ? value[i] : "0");
                } else {
                    free(value[i]);
                    value[i] = NULL;
                }
            }
            printf("\n");
        }
    }
    // XXX free all values
    for (i = 0; i < 16; i++) {
        free(value[i]);
        value[i] = NULL;
        pointer[i] = 0;
    }
    dirty = 0xFFFF;
    idx = 0;
    delete_ranges(ranges);
}


void
emit_load_direct(const char *value, BOOL deref0)
{
    make1(LDR_R0, value);
    if (deref0) {
        make1(LDR_R0_R0);
    }
}


void
emit_load_indirect(const char *lvalue, BOOL deref0)
{
    char *tmp = create_address_str(lvalue, 0);
    make1(LDR_R0, tmp);
    make1(LDR_R0_R0);
    if (deref0) {
        make1(LDR_R0_R0);
    }
    free(tmp);
}


void
emit_store_indirect(const char *lvalue)
{
    char *tmp = create_address_str(lvalue, 0);
    make1(LDR_R4, tmp);
    make1(STR_R0_R4);
    free(tmp);
}


void
emit_store_direct(const char *lvalue)
{
    make1(LDR_R4, lvalue);
    make1(STR_R0_R4);
}


void
emit_add(const char *value, const char *addend, int deref0, BOOL swap)
{
    if (swap) {
        const char *tmp = value;
        value = addend;
        addend = tmp;
    }
    make1(LDR_R0R1, value, addend);
    while (deref0--) {
        make1(LDR_R0_R0);
    }
    make1(ADD_R0_R1);
}


void
emit_call(const char *func, char **args, int nargs, int deref0, BOOL inloop, BOOL retval, int attr)
{
    char *tmp = NULL;
    enum R_OP op = BLX_R4;
    int rargs = nargs;
    if (rargs > arch_regparm) {
        rargs = arch_regparm;
    }
    if (rargs) {
        assert(rargs <= 4);
        if (rargs > 3) {
            make1(LDR_R0TO3, args[0], args[1], args[2], args[3]);
        } else if (rargs > 2) {
            make1(LDR_R0TO3, args[0], args[1], args[2], NULL);
        } else if (rargs > 1) {
            make1(LDR_R0R1, args[0], args[1]);
        } else {
            make1(LDR_R0, args[0]);
        }
    }
    while (deref0--) {
        make1(LDR_R0_R0);
    }
    if (attr & ATTRIB_NORETURN) {
        assert(nargs - rargs <= 1);
        make1(BX_IMM + nargs - rargs, func, args + rargs);
        return;
    }
    make1(LDR_R4, func);
    if ((attr & ATTRIB_STACK) || inloop) {
        make1(ADD_SP);
    }
    if (inloop) {
        tmp = new_name("res");
        add_label(tmp, -1);
        make1(LABEL, tmp, 0);
    }
    if (!(attr & ATTRIB_STDCALL) && nargs > rargs) {
        assert(nargs - rargs <= 6);
        op += nargs - rargs;
    }
    make1(op, args + rargs);
    if (tmp) {
        char *sav;
        char *gdg = new_name("gdg");
        if (retval) {
            sav = emit_save();
        }
        add_extern(gdg, optab[op].addr, FALSE);
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
    make1(MOV_R1_R0);
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
    make1(MOV_R0_R1);
    (void)scratch;
#endif
}


void
emit_goto(const char *label)
{
    const char *reuse = optimize_jmp ? get_label_with_label(label) : NULL;
    char *tmp = NULL;

    if (reuse) {
        make1(LDR_R0, reuse);
    } else {
        tmp = new_name("stk");
        make1(LDR_R0, tmp);
    }
    make1(LDMIA_R0, tmp, label, label);
    if (tmp) {
        add_label_with_label(tmp, label);
        free(tmp);
    }
}


void
emit_cond(const char *label, enum cond_t cond)
{
    const char *reuse = optimize_jmp ? get_label_with_label(label) : NULL;
    char *next = new_name("nxt");
    char *tmp1 = NULL;
    char *tmp2 = new_name("stk");
    BOOL inverse = (cond == COND_EQ);
    assert(cond == COND_NE || cond == COND_EQ);

    if (reuse) {
        if (inverse) {
            make1(LDR_R4R5, tmp2, reuse);
        } else {
            make1(LDR_R4R5, reuse, tmp2);
        }
    } else {
        tmp1 = new_name("stk");
        if (inverse) {
            make1(LDR_R4R5, tmp2, tmp1);
        } else {
            make1(LDR_R4R5, tmp1, tmp2);
        }
    }
    make1(CMP_R0);
    make1(LDMIA_R0_C, tmp1, label, label, tmp2, next, next);
    if (tmp1) {
        add_label_with_label(tmp1, label);
        free(tmp1);
    }
    add_label(tmp2, -1); // XXX labels with -1 do not interfere with our any_label() mechanism
    add_label(next, idx);

    free(tmp2);
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
    make1(LABEL, label, 0xFFFF);
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
    return "ARM";
}


int arch_regparm = 4;
