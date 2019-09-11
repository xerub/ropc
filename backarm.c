//#define GOTO_BY_LDMIA
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

typedef char *BRICK;

enum R_OP {
    BAD,
    LABEL,
    LDR_R0,
    LDR_R0R1,
    LDR_R0TO3,
    LDR_R4,
    LDR_R4R5,
    LDR_R7,
    LANDING,
    LDR_R0_R0,
    STR_R0_R4,
    MOV_R1_R0,
    MOV_R0_R1,
    OR_R0_R1,
    XOR_R0_R1,
    AND_R0_R1,
    ADD_R0_R1,
    SUB_R0_R1,
    MUL_R0_R1,
    DIV_R0_R1,
    BLX_R4,
    BLX_R4_1,
    BLX_R4_2,
    BLX_R4_3,
    BLX_R4_4,
    BLX_R4_5,
    BLX_R4_6,
    COMMUTE,
    SELECT,
#ifdef GOTO_BY_LDMIA
    LDMIA_R0,
    LDMIA_R0_C,
#endif
    BX_IMM,
    BX_IMM_1
};

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
    { BAD,          /**/ 0,           /**/ 0,                          /**/ 0,                    /**/  0, 0, 0, NULL },
    { LABEL,        /**/ 0,           /**/ 0,                          /**/ 0,                    /**/  0, 0, 0, NULL },
    { LDR_R0,       /**/ 0,           /**/ R0,                         /**/ 0,                    /**/  0, 0, 0, "POP {R0,PC}" },
    { LDR_R0R1,     /**/ 0,           /**/ R0|R1,                      /**/ 0,                    /**/  0, 0, 0, "POP {R0-R1,PC}" },
    { LDR_R0TO3,    /**/ 0,           /**/ R0|R1|R2|R3,                /**/ 0,                    /**/  0, 0, 0, "POP {R0-R3,PC}" },
    { LDR_R4,       /**/ 0,           /**/             R4,             /**/ 0,                    /**/  0, 0, 0, "POP {R4,PC}" },
    { LDR_R4R5,     /**/ 0,           /**/             R4|R5,          /**/ 0,                    /**/  0, 0, 0, "POP {R4,R5,PC}" },
    { LDR_R7,       /**/ 0,           /**/                   R7,       /**/ 0,                    /**/  0, 0, 0, "POP {R7,PC}" },
    { LANDING,      /**/ 0,           /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "POP {R7,PC}" },
    { LDR_R0_R0,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "LDR R0, [R0] / POP {R7,PC}" },
    { STR_R0_R4,    /**/ 0,           /**/             R4|   R7,       /**/             R4|   R7, /**/  0, 0, 0, "STR R0, [R4] / POP {R4,R7,PC}" },
    { MOV_R1_R0,    /**/    R1,       /**/             R4|   R7,       /**/             R4|   R7, /**/  0, 0, 0, "MOV R1, R0 / POP {R4,R7,PC}" },
    { MOV_R0_R1,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "MOV R0, R1 / POP {R7,PC}" },
    { OR_R0_R1,     /**/ R0,          /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "ORRS R0, R1 / POP {R7,PC}" },
    { XOR_R0_R1,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "EORS R0, R1 / POP {R7,PC}" },
    { AND_R0_R1,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "ANDS R0, R1 / POP {R7,PC}" },
    { ADD_R0_R1,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "ADD R0, R1 / POP {R7,PC}" },
    { SUB_R0_R1,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "SUBS R0, R0, R1 / POP {R7,PC}" },
    { MUL_R0_R1,    /**/ R0,          /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "MULS R0, R1 / POP {R7,PC}" },
    { DIV_R0_R1,    /**/ R0,          /**/             R4|   R7,       /**/             R4|   R7, /**/  0, 0, 0, "UDIV R0, R0, R1 / POP {R4,R7,PC}" },
    { BLX_R4,       /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/  0, 0, 0, "BLX R4 / POP {R4,R7,PC}" },
    { BLX_R4_1,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/  1, 0, 0, "BLX R4 / ADD SP, #4 / POP {R4,R7,PC}" },
    { BLX_R4_2,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/  2, 0, 0, "BLX R4 / ADD SP, #8 / POP {R4,R7,PC}" },
    { BLX_R4_3,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/  3, 0, 0, "BLX R4 / ADD SP, #12 / POP {R4,R7,PC}" },
    { BLX_R4_4,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/  4, 0, 0, "BLX R4 / ADD SP, #16 / POP {R4,R7,PC}" },
    { BLX_R4_5,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/  5, 0, 0, "BLX R4 / ADD SP, #20 / POP {R4,R7,PC}" },
    { BLX_R4_6,     /**/ R0|R1|R2|R3, /**/             R4|   R7,       /**/             R4|   R7, /**/  6, 0, 0, "BLX R4 / ADD SP, #24 / POP {R4,R7,PC}" },
    { COMMUTE,      /**/ 0,           /**/                   R7,       /**/                   R7, /**/  0, 0, 0, "MOV SP, R7 / POP {R7,PC}" },
    { SELECT,       /**/ R0,          /**/             R4|R5|R7,       /**/             R4|R5|R7, /**/  0, 0, 0, "CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}" }, // (R0 = R0 ? R4 : R5)
#ifdef GOTO_BY_LDMIA
    { LDMIA_R0,     /**/ 0,           /**/ R0|         R4|      SP|PC, /**/ R0|         R4,       /**/  0, 0, 0, "LDMIA R0, {R0,R4,SP,PC}" },
    { LDMIA_R0_C,   /**/ 0,           /**/ R0|         R4|      SP|PC, /**/ R0|         R4,       /**/  0, 0, 0, "LDMIA R0, {R0,R4,SP,PC}" },
#endif
    { BX_IMM,       /**/ 0xFFFFFFFF,  /**/ 0,                          /**/ 0,                    /**/  0, 0, 0, NULL },
    { BX_IMM_1,     /**/ 0xFFFFFFFF,  /**/ 0,                          /**/ 0,                    /**/  1, 0, 0, NULL },
};


static int idx = 0;
static BRICK strip[10240];

static unsigned dirty = 0xFFFFFFFF;
static int pointer[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static struct range *ranges = NULL;

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
build_pop_pc_string(char *text, unsigned pops)
{
    unsigned j, n = 0;
    text[n++] = '{';
    for (j = 0; j < 15; j++) {
        if (pops & (1 << j)) {
            if (j == 13) {
                text[n++] = 'S';
                text[n++] = 'P';
            } else if (j == 14) {
                text[n++] = 'L';
                text[n++] = 'R';
            } else if (j > 9) {
                text[n++] = 'R';
                text[n++] = '1';
                text[n++] = j + '0' - 10;
            } else {
                text[n++] = 'R';
                text[n++] = j + '0';
            }
            text[n++] = ',';
            text[n] = '\0';
        }
    }
    strcpy(text + n, "PC}");
}


static int
solve_mov_rr_01(const unsigned char *p, uint32_t size, uint64_t addr, void *user)
{
    static char text[256];
    struct R_OPDEF *r = (struct R_OPDEF *)user;
    unsigned pops = p[2];
    if (pops & (R0 | R1)) {
        return -1;
    }
    r->output = pops;
    r->auxout = pops;
    r->text = text;
    build_pop_pc_string(text + sprintf(text, "MOV R%d, R%d / POP ", p[0] & 7, (p[0] >> 3) & 7), pops);
    return 0;
}


static int
_is_popw(const unsigned char *buf, int nregs)
{
    // LDR.W R?,[SP],#4
    if (nregs == 1 && buf[0] == 0x5d && buf[1] == 0xf8 && buf[2] == 0x04 && (buf[3] & 0xF) == 0xb) {
        return 1;
    }
    // POP.W {...}
    return (buf[0] == 0xbd && buf[1] == 0xe8 && buf[2] == 0x00 && popcount(buf[3]) == nregs);
}


static int
solve_call_sp(const unsigned char *p, uint32_t size, uint64_t addr, void *user)
{
    static char text[256];
    struct R_OPDEF *r = (struct R_OPDEF *)user;
    unsigned incsp = r->op - BLX_R4;
    unsigned words = p[2];
    unsigned pops;

    while (1) {
        p += 4;
        if (words == incsp && (p[0] & R6) == 0 && p[1] == 0xBD) {
            break;
        }
        if (words >= incsp) {
            return -1;
        }
        pops = _is_popw(p, incsp - words);
        if (!pops) {
            return -1;
        }
        words += pops;
    }

    pops = p[0];
    r->output = pops;
    r->auxout = pops;
    r->incsp = incsp;
    r->text = text;
    build_pop_pc_string(text + sprintf(text, "BLX R4 / ADD SP, #%u / POP ", incsp * 4), pops);
    return 0;
}


#ifdef GOTO_BY_LDMIA
/*
FF FF 96 E9 <- 8 == LDMIA, 9 = LDMIB
   ^  ^^ ^
   |  || +---- cond (0xE == always)
   |  |+------ source register
   |  +------- 1, 3, 5^, 7^, 9, B, D^, F^
   +---------- & 0xA == 0xA (SP+PC)
*/
static int
solve_ldmia(const unsigned char *p, uint32_t size, uint64_t addr, void *user)
{
    static char text[256];
    struct R_OPDEF *r = (struct R_OPDEF *)user;
    if ((p[1] & 0xA0) == 0xA0 &&
        (p[2] & 0x10) == 0x10 && (p[2] & 0x50) != 0x50 &&
        (p[3] & 0xE) == 0x8 && (p[3] & 0xF0) != 0xF0) {
        int pops = ((unsigned short *)p)[0];
        if (r->addr) {
            if (p[1] & 0x40) {
                return -1; // pops {SP,LR,PC}, not sure this is allowed
            }
            if (optimize_reg && popcount(pops & (R0 | R4)) < popcount(r->output & (R0 | R4))) {
                return -1; // fewer desired registers, keep the old one
            }
            if (popcount(pops) > popcount(r->output)) {
                return -1; // more registers, keep the old one
            }
        }
        r->addr = addr;
        r->output = pops;
        r->auxout = pops & ~(SP | PC);
        r->text = text;
        build_pop_pc_string(text + sprintf(text, "LDMIA R%d, ", p[2] & 0xF), pops);
    }
    return -1;
}

/*
94 E8 FF FF
^^ ^     ^
|| |     +---- & 0xA == 0xA (SP+PC)
|| +---------- E8 == LDMIA, E9 = LDMIB (!only opcode E8 was tested!)
|+------------ source register
+------------- 1, 3, 5^, 7^, 9, B, D^, F^ (!only opcode 9 was tested!)
*/
static int
solve_ldmiaw(const unsigned char *p, uint32_t size, uint64_t addr, void *user)
{
    static char text[256];
    struct R_OPDEF *r = (struct R_OPDEF *)user;
    if ((p[3] & 0xA0) == 0xA0 &&
        (p[0] & 0x10) == 0x10 && (p[0] & 0x50) != 0x50 &&
        (p[1] == 0xE8)) {
        int pops = ((unsigned short *)p)[1];
        if (r->addr) {
            if (p[1] & 0x40) {
                return -1; // pops {SP,LR,PC}, not sure this is allowed
            }
            if (optimize_reg && popcount(pops & (R0 | R4)) < popcount(r->output & (R0 | R4))) {
                return -1; // fewer desired registers, keep the old one
            }
            if (popcount(pops) > popcount(r->output)) {
                return -1; // more registers, keep the old one
            }
        }
        r->addr = addr;
        r->output = pops;
        r->auxout = pops & ~(SP | PC);
        r->text = text;
        build_pop_pc_string(text + sprintf(text, "LDMIA.W R%d, ", p[0] & 0xF), pops);
    }
    return -1;
}
#endif


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
        case LDR_R0:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 01 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case LDR_R0R1:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 03 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case LDR_R0TO3:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 0F BD");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 0F 80 BD E8");
            break;
        case LDR_R4:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 10 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case LDR_R4R5:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 30 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case LDR_R7:
        case LANDING:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 80 BD");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "- BD E8 80 80");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "- 80 BC 5D F8 04 EB 70 47");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 80 80 BD E8");
            break;
        case LDR_R0_R0:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 00 68 80 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case STR_R0_R4:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 20 60 90 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case MOV_R1_R0: {
            static char text[256];
            rv = parse_string(ranges, binmap, NULL, NULL, "- 01 46 90 BD");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "- 01 46 08 46 80 BD");
            if (rv) {
                rv++;
                r->output = R7;
                r->auxout = R7;
                break;
            }
            rv = parse_string(ranges, binmap, r, solve_mov_rr_01, "- 01 46 .. BD");
            if (rv) {
                r->text = strcpy(text, r->text);
                rv++;
                break;
            }
            break;
        }
        case MOV_R0_R1: {
            static char text[256];
            rv = parse_string(ranges, binmap, NULL, NULL, "- 08 46 80 BD");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, r, solve_mov_rr_01, "- 08 46 .. BD");
            if (rv) {
                r->text = strcpy(text, r->text);
                rv++;
                break;
            }
            break;
        }
        case OR_R0_R1:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 08 43 80 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case XOR_R0_R1:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 48 40 80 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case AND_R0_R1:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 08 40 80 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case ADD_R0_R1:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 08 44 80 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case SUB_R0_R1:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 40 1A 80 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case MUL_R0_R1:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 48 43 80 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case DIV_R0_R1:
            rv = parse_string(ranges, binmap, NULL, NULL, "- B0 FB F1 F0 90 BD");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "- B0 FB F1 F0 B0 BD");
            if (rv) {
                r->output = R4 | R5 | R7;
                r->auxout = R4 | R5 | R7;
                r->text = "UDIV R0, R0, R1 / POP {R4,R5,R7,PC}";
                rv++;
                break;
            }
            break;
        case BLX_R4:
            rv = parse_string(ranges, binmap, NULL, NULL, "- A0 47 90 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
        case BLX_R4_1: {
            static char text[256];
            rv = parse_string(ranges, binmap, r, solve_call_sp, "- A0 47 .. B0");
            if (rv) {
                r->text = strcpy(text, r->text);
                rv++;
                break;
            }
        }
        case BLX_R4_2: {
            static char text[256];
            rv = parse_string(ranges, binmap, r, solve_call_sp, "- A0 47 .. B0");
            if (rv) {
                r->text = strcpy(text, r->text);
                rv++;
                break;
            }
        }
        case BLX_R4_3: {
            static char text[256];
            rv = parse_string(ranges, binmap, r, solve_call_sp, "- A0 47 .. B0");
            if (rv) {
                r->text = strcpy(text, r->text);
                rv++;
                break;
            }
        }
        case BLX_R4_4: {
            static char text[256];
            rv = parse_string(ranges, binmap, r, solve_call_sp, "- A0 47 .. B0");
            if (rv) {
                r->text = strcpy(text, r->text);
                rv++;
                break;
            }
        }
        case BLX_R4_5: {
            static char text[256];
            rv = parse_string(ranges, binmap, r, solve_call_sp, "- A0 47 .. B0");
            if (rv) {
                r->text = strcpy(text, r->text);
                rv++;
                break;
            }
        }
        case BLX_R4_6: {
            static char text[256];
            rv = parse_string(ranges, binmap, r, solve_call_sp, "- A0 47 .. B0");
            if (rv) {
                r->text = strcpy(text, r->text);
                rv++;
                break;
            }
            break;
        }
        case COMMUTE:
            rv = parse_string(ranges, binmap, NULL, NULL, "- BD 46 80 BD");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "- BD 46 BD E8 80 80");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "- BD 46 80 BC 5D F8 04 EB 70 47");
            if (rv) {
                rv++;
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "+ 07 D0 A0 E1 80 80 BD E8");
            break;
        case SELECT:
            rv = parse_string(ranges, binmap, NULL, NULL, "- 00 28 08 BF 2C 46 20 46 B0 BD");
            if (rv) {
                rv++;
                break;
            }
            break;
#ifdef GOTO_BY_LDMIA
        case LDMIA_R0:
        case LDMIA_R0_C: {
            parse_string(ranges, binmap, r, solve_ldmia, "+ .. .. 90 E8");
            if (r->addr) {
                rv = r->addr;
                break;
            }
            parse_string(ranges, binmap, r, solve_ldmiaw, "- 90 E8 .. ..");
            if (r->addr) {
                rv = r->addr + 1;
                break;
            }
            break;
        }
#endif
    }
    r->addr = rv;
    return r;
}


static void
solve_op(enum R_OP op)
{
    struct R_OPDEF *r = try_solve_op(op);
    if (r && !r->addr) {
        die("undefined gadget '%s'\n", r->text);
    }
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
        case LABEL:
            strip[idx++] = (BRICK)op;
            strip[idx++] = argdup(va_arg(ap, char *));
            spill = va_arg(ap, unsigned);
            strip[idx++] = (BRICK)(unsigned long)spill;
            add_label(strip[idx - 2], idx - 3);
            break;
        case LDR_R0:
            if (optimize_reg && pointer[0]) {
                strip[pointer[0]] = argdup(va_arg(ap, char *));
                pointer[0] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R0R1:
            if (optimize_reg && pointer[0] && pointer[1]) {
                strip[pointer[0]] = argdup(va_arg(ap, char *));
                strip[pointer[1]] = argdup(va_arg(ap, char *));
                pointer[0] = 0;
                pointer[1] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R4:
            if (optimize_reg && pointer[4]) {
                strip[pointer[4]] = argdup(va_arg(ap, char *));
                pointer[4] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R4R5:
            if (optimize_reg && pointer[4] && pointer[5]) {
                strip[pointer[4]] = argdup(va_arg(ap, char *));
                strip[pointer[5]] = argdup(va_arg(ap, char *));
                pointer[4] = 0;
                pointer[5] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R7:
            if (optimize_reg && pointer[7]) {
                strip[pointer[7]] = argdup(va_arg(ap, char *));
                pointer[7] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LDR_R0TO3:
        case LANDING:
        case LDR_R0_R0:
        case STR_R0_R4:
        case MOV_R1_R0:
        case MOV_R0_R1:
        case OR_R0_R1:
        case XOR_R0_R1:
        case AND_R0_R1:
        case ADD_R0_R1:
        case SUB_R0_R1:
        case MUL_R0_R1:
        case DIV_R0_R1:
        case SELECT:
            strip[idx++] = (BRICK)op;
            break;
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
        case COMMUTE:
            strip[idx++] = (BRICK)op;
            strip[idx++] = (BRICK)(long)va_arg(ap, int);
            break;
#ifdef GOTO_BY_LDMIA
        case LDMIA_R0:
        case LDMIA_R0_C:
            strip[idx++] = (BRICK)op;
            strip[idx++] = (BRICK)(long)va_arg(ap, int);
            break;
#endif
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
  done:
    va_end(ap);
    return idx;
}


static void
play_data(const char *arg, int i, int ch)
{
    if (arg) {
        const struct SYM *p;
#ifdef GOTO_BY_LDMIA
        if (i == 13) {
            p = get_symbol(arg);
            if (!p || p->type != SYMBOL_LABEL) {
                die("cannot find label '%s'\n", arg);
            }
            printx("        du    4 + %-26s; -> SP\n", arg);
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
            if (j < BX_IMM) {
                printx("        dg    0x%-28llX; -> PC: %s\n", optab[j].addr, optab[j].text);
                return;
            }
            arg = strip[k + 1];
        }
#endif
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
    const BRICK *p = strip;
    const BRICK *q = p + idx;
    BRICK value[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    dirty = 0xFFFFFFFF;
    while (p < q) {
        enum R_OP op = (enum R_OP)(long)(*p++);
        const struct R_OPDEF *r = &optab[op];
        unsigned spill = r->spill;
#ifdef GOTO_BY_LDMIA
        long ldmia_reuse = 0;
#endif
        if (op == BX_IMM || op == BX_IMM_1) {
            BRICK arg = *p++;
            const struct SYM *p = get_symbol(arg);
            assert(p);
            if (p->type != SYMBOL_EXTERN) {
                printx("%-7s dd    %-30s; -> PC\n", arg, p->val ? p->val : "0");
            } else {
                printx("        dg    0x%-28llX; -> PC: %s\n", p->addr, arg);
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
#ifdef GOTO_BY_LDMIA
        } else if (op == LDMIA_R0_C) {
            // LDMIA_R0_C does not emit PC (a previous LDMIA_R0 did, and stack points here)
#endif
        } else {
            printx("        dg    0x%-28llX; -> PC: %s\n", r->addr, r->text);
        }
        if (op == COMMUTE) {
            long restack = (long)*p++;
            if (restack < 0) {
                restack = inloop_stack;
            }
            if (restack) {
                printx("        times 0x%lx dd 0\n", restack);
            }
        }
        switch (op) {
#ifdef GOTO_BY_LDMIA
            case LDMIA_R0:
            case LDMIA_R0_C:
                ldmia_reuse = (long)*p++;
#endif
            case LABEL:
            case LDR_R0:
            case LDR_R0R1:
            case LDR_R0TO3:
            case LDR_R4:
            case LDR_R4R5:
            case LDR_R7:
            case LANDING:
            case LDR_R0_R0:
            case STR_R0_R4:
            case MOV_R1_R0:
            case MOV_R0_R1:
            case OR_R0_R1:
            case XOR_R0_R1:
            case AND_R0_R1:
            case ADD_R0_R1:
            case SUB_R0_R1:
            case MUL_R0_R1:
            case DIV_R0_R1:
            case BLX_R4:
            case BLX_R4_1:
            case BLX_R4_2:
            case BLX_R4_3:
            case BLX_R4_4:
            case BLX_R4_5:
            case BLX_R4_6:
            case COMMUTE:
            case SELECT:
            case BX_IMM:
            case BX_IMM_1:
                for (i = 0; i < r->incsp; i++) {
                    play_data(*p, i, 'A');
                    free(*p);
                    p++;
                }
                for (i = 0; i < 32; i++) {
                    if (r->output & (1 << i)) {
                        free(value[i]);
                        value[i] = *p++;
#ifdef GOTO_BY_LDMIA
                        if (ldmia_reuse || (op == LDMIA_R0_C && i == 15)) {
                            // skip reused register set (or the second PC for double-LDMIA)
                            continue;
                        }
#endif
                        play_data(value[i], i, 'R');
                    }
                }
                break;
            default:
                assert(0);
        }
        dirty &= ~r->output;
        dirty |= spill;
        if (show_reg_set) {
            printx(";");
            for (i = 0; i < 32; i++) {
                if (!(dirty & (1 << i))) {
                    if (i >= 8) {
                        continue;
                    }
                    printx(" R%d=%s", i, value[i] ? value[i] : "0");
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
emit_or(const char *value, const char *addend, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, addend);
    make1(LDR_R0R1, value, addend);
    while (deref0--) {
        make1(LDR_R0_R0);
    }
    make1(OR_R0_R1);
}


void
emit_xor(const char *value, const char *addend, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, addend);
    make1(LDR_R0R1, value, addend);
    while (deref0--) {
        make1(LDR_R0_R0);
    }
    make1(XOR_R0_R1);
}


void
emit_and(const char *value, const char *addend, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, addend);
    make1(LDR_R0R1, value, addend);
    while (deref0--) {
        make1(LDR_R0_R0);
    }
    make1(AND_R0_R1);
}


void
emit_add(const char *value, const char *addend, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, addend);
    make1(LDR_R0R1, value, addend);
    while (deref0--) {
        make1(LDR_R0_R0);
    }
    make1(ADD_R0_R1);
}


void
emit_sub(const char *value, const char *addend, int deref0)
{
    make1(LDR_R0R1, value, addend);
    while (deref0--) {
        make1(LDR_R0_R0);
    }
    make1(SUB_R0_R1);
}


void
emit_mul(const char *value, const char *multiplier, int deref0, BOOL swap)
{
    SWAP_PTR(swap, value, multiplier);
    make1(LDR_R0R1, value, multiplier);
    while (deref0--) {
        make1(LDR_R0_R0);
    }
    make1(MUL_R0_R1);
}


void
emit_div(const char *value, const char *addend, int deref0)
{
    make1(LDR_R0R1, value, addend);
    while (deref0--) {
        make1(LDR_R0_R0);
    }
    make1(DIV_R0_R1);
}


void
emit_call(const char *func, char **args, int nargs, int deref0, BOOL inloop, BOOL retval, int attr, int regparm, int restack)
{
    char *tmp = NULL;
    enum R_OP op = BLX_R4;
    int rargs = nargs;
    if (args == NULL) {
        rargs = nargs = 0;
    }
    if (rargs > regparm) {
        rargs = regparm;
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
        char *skip;
        tmp = new_name("res");
        skip = create_address_str(tmp, -4);
        make1(LDR_R7, skip);
        make1(COMMUTE, restack);
        make1(LABEL, tmp, R7);
        free(skip);
    }
    if (!(attr & ATTRIB_STDCALL) && nargs > rargs) {
        assert(nargs - rargs <= 6);
        op += nargs - rargs;
    }
    make1(op, args + rargs);
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
#ifndef GOTO_BY_LDMIA
    char *label4 = create_address_str(label, 4);
    make1(LDR_R7, label4);
    make1(COMMUTE, 0);
    free(label4);
#else
    const char *reuse = optimize_jmp ? get_label_with_label(label) : NULL;
    char *set, *tmp = NULL;

    if (reuse) {
        set = create_address_str(reuse, 4);
        make1(LDR_R0, set);
    } else {
        tmp = new_name("stk");
        set = create_address_str(tmp, 4);
        make1(LDR_R0, set);
        make1(LABEL, tmp, 0);
        if (optimize_jmp) {
            set_label_with_label(tmp, label);
        }
    }

    make1(LDMIA_R0, !tmp, label, label); // create_address_str(label, 4)
    free(set);
    free(tmp);
#endif
}


void
emit_cond(const char *label, enum cond_t cond)
{
#ifndef GOTO_BY_LDMIA
    char *dst = new_name("dst");
    char *next = new_name("nxt");
    char *pivot = create_address_str(dst, -4);
    char *next4 = create_address_str(next, -4);
    char *label4 = create_address_str(label, 4);
    BOOL inverse = (cond == COND_EQ);
    assert(cond == COND_NE || cond == COND_EQ);
    if (inverse) {
        make1(LDR_R4R5, next4, label4);
    } else {
        make1(LDR_R4R5, label4, next4);
    }
    make1(SELECT);
    emit_store_direct(pivot);
    if ((optab[STR_R0_R4].output & R7) == 0) {
        make1(LANDING);
    }
    make1(LABEL, dst, 0);
    make1(COMMUTE, 0);
    make1(LABEL, next, 0);
    free(label4);
    free(next4);
    free(pivot);
    free(next);
    free(dst);
#else
    const char *reuse = optimize_jmp ? get_label_with_label(label) : NULL;
    char *set, *tmp = NULL;
    char *next = new_name("nxt");
    char *tmp2 = new_name("stk");
    BOOL inverse = (cond == COND_EQ);
    assert(cond == COND_NE || cond == COND_EQ);

    if (reuse) {
        set = create_address_str(reuse, 4);
        SWAP_PTR(inverse, set, tmp2);
        make1(LDR_R4R5, set, tmp2);
        SWAP_PTR(inverse, set, tmp2);
        make1(SELECT);
    } else {
        tmp = new_name("stk");
        set = create_address_str(tmp, 4);
        SWAP_PTR(inverse, set, tmp2);
        make1(LDR_R4R5, set, tmp2);
        SWAP_PTR(inverse, set, tmp2);
        make1(SELECT);
        make1(LABEL, tmp, 0);
        if (optimize_jmp) {
            set_label_with_label(tmp, label);
        }
    }

    make1(LDMIA_R0, !tmp, label, label); //create_address_str(label, 4);
    make1(LABEL, tmp2, 0);
    make1(LDMIA_R0_C, !tmp2, next, next); // create_address_str(next, 4);
    make1(LABEL, next, 0);
    free(next);
    free(tmp2);
    free(set);
    free(tmp);
#endif
}


void
emit_label(const char *label, BOOL used, BOOL last)
{
    if (!used) {
        make1(LABEL, label, 0);
        cry("unused label '%s'\n", label);
    } else if (last) {
        make1(LABEL, label, 0xFFFFFFFF);
#ifndef GOTO_BY_LDMIA
        make1(LANDING);
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
    return "ARM";
}
