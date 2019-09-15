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


#define RAX (1 << 0)
#define RCX (1 << 1)
#define RDX (1 << 2)
#define RBX (1 << 3)
#define RSP (1 << 4)
#define RBP (1 << 5)
#define RSI (1 << 6)
#define RDI (1 << 7)
#define R8  (1 << 8)
#define R9  (1 << 9)
#define R10 (1 << 10)
#define R11 (1 << 11)
#define R12 (1 << 12)
#define R13 (1 << 13)
#define R14 (1 << 14)
#define R15 (1 << 15)

typedef char *BRICK;

enum R_OP {
    BAD,
    LABEL,
    LDR_RAX,
    LDR_RCX,
    LDR_RDX,
    LDR_RBX,
    LANDING,
    LDR_RBP,
    LDR_RSI,
    LDR_RDI,
    LDR_R8,
    LDR_R9,
    LDR_RAX_RAX,
    STR_RAX_RBX,
    MOV_RDI_RAX,
    MOV_RAX_RDI,
    OR_RAX_reg,
    XOR_RAX_reg,
    AND_RAX_reg,
    ADD_RAX_reg,
    SUB_RAX_reg,
    MUL_RAX_reg,
    DIV_RAX_reg,
    MUL_RAX_TWO,
    BLR_RAX,
    COMMUTE,
    SELECT,
    BX_IMM,
    BX_IMM_1
};

#define GADGET_MOV_RDI_RAX_rcx1 0x100
#define GADGET_MOV_RDI_RAX_rcx2 0x200
#define GADGET_DIV_one_rdi      0x400

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
    { BAD,          /**/ 0,                             /**/ 0,                                 /**/ 0,       /**/  0, 0, 0, NULL },
    { LABEL,        /**/ 0,                             /**/ 0,                                 /**/ 0,       /**/  0, 0, 0, NULL },
    { LDR_RAX,      /**/ 0,                             /**/ RAX,                               /**/ 0,       /**/  0, 0, 0, "pop rax / ret" },
    { LDR_RCX,      /**/ 0,                             /**/     RCX,                           /**/ 0,       /**/  0, 0, 0, "pop rcx / ret" },
    { LDR_RDX,      /**/ 0,                             /**/         RDX,                       /**/ 0,       /**/  0, 0, 0, "pop rdx / ret" },
    { LDR_RBX,      /**/ 0,                             /**/             RBX,                   /**/ 0,       /**/  0, 0, 0, "pop rbx / ret" },
    { LANDING,      /**/ 0,                             /**/                 RBP,               /**/     RBP, /**/  0, 0, 0, "pop rbp / ret" },
    { LDR_RBP,      /**/ 0,                             /**/                 RBP,               /**/ 0,       /**/  0, 0, 0, "pop rbp / ret" },
    { LDR_RSI,      /**/ 0,                             /**/                     RSI,           /**/ 0,       /**/  0, 0, 0, "pop rsi / ret" },
    { LDR_RDI,      /**/ 0,                             /**/                         RDI,       /**/ 0,       /**/  0, 0, 0, "pop rdi / ret" },
    { LDR_R8,       /**/ 0,                             /**/                             R8,    /**/ 0,       /**/  0, 0, 0, "pop r8 / ret" },
    { LDR_R9,       /**/ 0,                             /**/                                R9, /**/ 0,       /**/  0, 0, 0, "pop r9 / ret" },
    { LDR_RAX_RAX,  /**/ RAX,                           /**/ 0,                                 /**/ 0,       /**/  0, 0, 0, "mov rax, [rax] / ret" },
    { STR_RAX_RBX,  /**/ 0,                             /**/             RBX,                   /**/ RBX,     /**/  0, 0, 0, "mov [rbx], rax / pop rbx / ret" },
    { MOV_RDI_RAX,  /**/                     RDI,       /**/                 RBP,               /**/     RBP, /**/  0, 0, 0, "mov rdi, rax / mov rax, rdi / pop rbp / ret" },
    { MOV_RAX_RDI,  /**/ RAX,                           /**/ 0,                                 /**/ 0,       /**/  0, 0, 0, "mov rax, rdi / ret" },
    { OR_RAX_reg,   /**/ RAX,                           /**/ 0,                                 /**/ 0,       /**/  0, LDR_RDX, 0, "or rax, rdx / ret" },
    { XOR_RAX_reg,  /**/ RAX,                           /**/ 0,                                 /**/ 0,       /**/  0, LDR_RDX, 0, "xor rax, rdx / ret" },
    { AND_RAX_reg,  /**/ RAX,                           /**/                 RBP,               /**/     RBP, /**/  0, LDR_RSI, 0, "and rax, rsi / pop rbp / ret" },
    { ADD_RAX_reg,  /**/ RAX,                           /**/ 0,                                 /**/ 0,       /**/  0, LDR_RCX, 0, "add rax, rcx / ret" },
    { SUB_RAX_reg,  /**/ RAX,                           /**/ 0,                                 /**/ 0,       /**/  0, LDR_RDX, 0, "sub rax, rdx / ret" },
    { MUL_RAX_reg,  /**/ RAX|    RDX,                   /**/ 0,                                 /**/ 0,       /**/  0, LDR_RSI, 0, "mul rsi / add rdx, r8 / ret" },
    { DIV_RAX_reg,  /**/ RAX|    RDX,                   /**/             RBX|RBP,               /**/ RBX|RBP, /**/  0, LDR_RBX, 0, "div rbx / pop rbx / pop rbp / ret" },
    { MUL_RAX_TWO,  /**/ RAX,                           /**/ 0,                                 /**/ 0,       /**/  0, 0, 0, "lea rax, [rax+rax] / ret" },
    { BLR_RAX,      /**/ RAX|RCX|RDX|    RSI|RDI|R8|R9, /**/                 RBP,               /**/     RBP, /**/  4, 0, 0, "call rax / add rsp, 0x20 / pop rbp / ret" },
    { COMMUTE,      /**/ 0,                             /**/                 RBP,               /**/     RBP, /**/  0, 0, 0, "mov rsp, rbp / pop rbp / ret" },
    { SELECT,       /**/ RAX,                           /**/             RBX,                   /**/ RBX,     /**/  0, LDR_RDX | (LDR_RBX << 8), 0, "test rax, rax / mov rax, rdx / cmovz rax, rbx / pop rbx / ret" },
    { BX_IMM,       /**/ 0xFFFFFFFF,                    /**/ 0,                                 /**/ 0,       /**/  0, 0, 0, NULL },
    { BX_IMM_1,     /**/ 0xFFFFFFFF,                    /**/ 0,                                 /**/ 0,       /**/  1, 0, 0, NULL },
};


static int idx = 0;
static BRICK strip[10240];

static unsigned dirty = 0xFFFFFFFF;
static int pointer[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static struct range *ranges = NULL;

static char *trampoline_rdi;

const int arch_regparm = 6; /* GNU (RDI, RSI, RDX, RCX, R8, R9); MSC (RCX, RDX, R8, R9) */


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
solve_mov_rdi_rax(const unsigned char *p, uint32_t size, uint64_t addr, void *user)
{
    static char text[256];
    struct R_OPDEF *r = (struct R_OPDEF *)user;
    r->spill = RCX;
    r->output = 0;
    r->auxout = 0;
    r->flags = GADGET_MOV_RDI_RAX_rcx2 + p[6];
    r->text = text;
    sprintf(text, "mov rdi, rax / mov rcx, [rcx%+d] / jmp rcx", (char)p[6]);
    return 0;
}


static int
solve_mul_rax_two(const unsigned char *p, uint32_t size, uint64_t addr, void *user)
{
    static char text[256];
    struct R_OPDEF *r = (struct R_OPDEF *)user;
    if (p[5] == 0xC3) {
        r->flags = p[4];
        r->text = text;
        sprintf(text, "lea rax, [rax+rax%+d] / pop ebp / ret", (char)p[4]);
        return 0;
    }
    if (p[5] == 0x5D && p[6] == 0xC3) {
        r->output = RBP;
        r->auxout = RBP;
        r->flags = p[4];
        r->text = text;
        sprintf(text, "lea rax, [rax+rax%+d] / pop ebp / ret", (char)p[4]);
        return 0;
    }
    return 1;
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
        case LDR_RAX:
            rv = parse_string(ranges, binmap, NULL, NULL, "58 C3");
            break;
        case LDR_RCX:
            rv = parse_string(ranges, binmap, NULL, NULL, "59 C3");
            break;
        case LDR_RDX:
            rv = parse_string(ranges, binmap, NULL, NULL, "5A C3");
            break;
        case LDR_RBX:
            rv = parse_string(ranges, binmap, NULL, NULL, "5B C3");
            break;
        case LDR_RBP:
        case LANDING:
            rv = parse_string(ranges, binmap, NULL, NULL, "5D C3");
            break;
        case LDR_RSI:
            rv = parse_string(ranges, binmap, NULL, NULL, "5E C3");
            break;
        case LDR_RDI:
            rv = parse_string(ranges, binmap, NULL, NULL, "5F C3");
            break;
        case LDR_R8:
            rv = parse_string(ranges, binmap, NULL, NULL, "41 58 C3");
            break;
        case LDR_R9:
            rv = parse_string(ranges, binmap, NULL, NULL, "41 59 C3");
            break;
        case LDR_RAX_RAX:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 8B 00 C3");
            break;
        case STR_RAX_RBX:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 89 03 5B C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "48 89 03 48 83 C4 08 5B 5D C3");
            if (rv) {
                r->output = RBX | RBP;
                r->auxout = RBX | RBP;
                r->incsp = 1;
                r->text = "mov [rbx], rax / add rsp, 8 / pop rbx / ret";
                break;
            }
            break;
        case MOV_RDI_RAX:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 89 C7 48 89 F8 5D C3");
            if (rv) {
                break;
            }
            /* XXX this will have to sacrifice RBP because pop order
            rv = parse_string(ranges, binmap, NULL, NULL, "48 89 C7 5B 5D 41 5C FF E1");
            if (rv) {
                r->spill = RDI | RBP;
                r->output = RBX | R11 | R12;
                r->auxout = RBX | R11 | R12;
                r->flags = GADGET_MOV_RDI_RAX_rcx1;
                r->text = "mov rdi, rax / pop rbx / pop rbp / pop r12 / jmp rcx";
                break;
            }
            */
            rv = parse_string(ranges, binmap, r, solve_mov_rdi_rax, "48 89 C7 48 8B 49 .. FF E1");
            if (rv) {
                break;
            }
            // XXX if not found, fix emit_save/emit_restore and emit_call(arg0)
            break;
        case MOV_RAX_RDI:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 89 F8 C3");
            break;
        case OR_RAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 09 D0 C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "48 09 D0 5D C3");
            if (rv) {
                r->output = RBP;
                r->auxout = RBP;
                r->text = "or rax, rdx / pop rbp / ret";
                break;
            }
            break;
        case XOR_RAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 31 D0 C3");
            break;
        case AND_RAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 21 F0 5D C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "48 21 D0 66 48 0F 6E C0 C3");
            if (rv) {
                r->output = 0;
                r->auxout = 0;
                r->flags = LDR_RDX;
                r->text = "and rax, rdx / mov xmm0, rax / ret";
                break;
            }
            break;
        case ADD_RAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 01 C8 C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "48 01 D0 5D C3");
            if (rv) {
                r->output = RBP;
                r->auxout = RBP;
                r->flags = LDR_RDX;
                r->text = "add rax, rdx / pop rbp / ret";
                break;
            }
            break;
        case SUB_RAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 29 D0 C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "48 29 C8 5D C3");
            if (rv) {
                r->output = RBP;
                r->auxout = RBP;
                r->flags = LDR_RCX;
                r->text = "sub rax, rcx / pop rbp / ret";
                break;
            }
            break;
        case MUL_RAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 F7 E6 4C 01 C2 C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "48 F7 E2 4C 01 C2 48 01 FA C3");
            if (rv) {
                r->flags = LDR_RDX;
                r->text = "mul rdx / add rdx, r8 / add rdx, rdi / ret";
                break;
            }
            break;
        case DIV_RAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 F7 F3 5B 5D C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "48 F7 F1 48 0F AF C7 EB 02 31 C0 5D C3");
            if (rv) {
                r->output = RBP;
                r->auxout = RBP;
                r->flags = LDR_RCX | GADGET_DIV_one_rdi;
                r->text = "div rcx / imul rax, rdi / jmp $ + 4 / xor eax, eax / pop rbp / ret";
                break;
            }
            break;
        case MUL_RAX_TWO:
            rv = parse_string(ranges, binmap, r, solve_mul_rax_two, "48 8D 44 00");
            break;
        case BLR_RAX:
            rv = parse_string(ranges, binmap, NULL, NULL, "FF D0 48 83 C4 20 5D C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "FF D0 48 83 C4 38 C3");
            if (rv) {
                r->text = "call rax / add rsp, 0x38 / ret";
                r->output = 0;
                r->auxout = 0;
                r->incsp = 7;
                break;
            }
            break;
        case COMMUTE:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 89 EC 5D C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "C9 C3");
            break;
        case SELECT:
            rv = parse_string(ranges, binmap, NULL, NULL, "48 85 C0 48 89 D0 48 0F 44 C3 5B C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "48 85 C0 48 0F 44 CA 48 89 C8 5B 41 5C 41 5E 41 5F 5D C3");
            if (rv) {
                r->spill = RAX | RCX;
                r->output = RBX | R12 | R14 | R15 | RBP;
                r->auxout = RBX | R12 | R14 | R15 | RBP;
                r->flags = LDR_RCX | (LDR_RDX << 8);
                r->text = "test rax, rax / cmovz rcx, rdx / mov rax, rcx / pop rbx / pop r12 / pop r14 / pop r15 / pop rbp / ret";
                break;
            }
            break;
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
    switch (op) {
        case LABEL:
            strip[idx++] = (BRICK)op;
            strip[idx++] = argdup(va_arg(ap, char *));
            spill = va_arg(ap, unsigned);
            strip[idx++] = (BRICK)(unsigned long)spill;
            add_label(strip[idx - 2], idx - 3);
            break;
        case LDR_RAX:
        case LDR_RCX:
        case LDR_RDX:
        case LDR_RBX:
        case LDR_RBP:
        case LDR_RSI:
        case LDR_RDI:
        case LDR_R8:
        case LDR_R9:
            i = op - LDR_RAX;
            if (optimize_reg && pointer[i]) {
                strip[pointer[i]] = argdup(va_arg(ap, char *));
                pointer[i] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case MOV_RDI_RAX:
            if (r->flags & GADGET_MOV_RDI_RAX_rcx2) {
                if (!trampoline_rdi) {
                    char *scratch = new_name("ret");
                    add_symbol_defined(scratch, "__gadget_ret", 0);
                    trampoline_rdi = create_address_str(scratch, -(r->flags & 0xFF));
                    free(scratch);
                }
                make1(LDR_RCX, trampoline_rdi);
            }
        case LANDING:
        case LDR_RAX_RAX:
        case STR_RAX_RBX:
        case MOV_RAX_RDI:
        case OR_RAX_reg:
        case XOR_RAX_reg:
        case AND_RAX_reg:
        case ADD_RAX_reg:
        case SUB_RAX_reg:
        case MUL_RAX_reg:
        case DIV_RAX_reg:
        case MUL_RAX_TWO:
        case SELECT:
            strip[idx++] = (BRICK)op;
            for (i = 0; i < r->incsp; i++) {
                strip[idx++] = NULL;
            }
            break;
        case BLR_RAX: {
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


static const char *x86regs[] = {
    "RAX", "RCX", "RDX", "RBX", "rsp", "RBP", "RSI", "RDI",
};


static void
play_data(const char *arg, int i)
{
    char namebuf[32];
    const char *regname;
    if (i >= 32) {
        sprintf(namebuf, "A%u", i - 32);
        regname = namebuf;
    } else if (i < 8) {
        assert(i != 4);
        regname = x86regs[i];
    } else {
        sprintf(namebuf, "R%u", i);
        regname = namebuf;
    }
    if (arg) {
        const struct SYM *p;
        if (is_address(arg)) {
            char *na = curate_address(arg);
            printx("        du    %-30s; -> %s\n", na, regname);
            free(na);
            return;
        }
        p = get_symbol(arg);
        if (p) {
            if (p->type == SYMBOL_EXTERN) {
                printx("        dg    0x%-28llX; -> %s: %s\n", p->addr, regname, arg);
            } else if (p->type == SYMBOL_LABEL) {
                printx("        du    %-30s; -> %s\n", p->key, regname);
            } else {
                assert(p->type == SYMBOL_NORMAL);
                if (p->val && is_address(p->val)) {
                    char *na = curate_address(p->val);
                    printx("%-7s du    %-30s; -> %s\n", arg, na, regname);
                    free(na);
                } else if (p->val && try_symbol_extern(p->val)) {
                    printx("%-7s dg    0x%-28llX; -> %s\n", arg, get_symbol(p->val)->addr, p->val);
                } else {
                    printx("%-7s dd    %-30s; -> %s\n", arg, p->val ? p->val : "0", regname);
                }
            }
            return;
        }
    }
    printx("        dd    %-30s; -> %s\n", arg ? arg : "0", regname);
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
        } else {
            printx("        dg    0x%-28llX; -> PC: %s\n", r->addr, r->text);
        }
        if (op == COMMUTE) {
            long restack = (long)*p++;
            long stalign = (long)*p++;
            if (restack < 0) {
                restack = inloop_stack;
            }
            if (restack) {
                printx("        times 0x%lx dd 0\n", restack);
            }
            if (stalign) {
                printx("        align %ld\n", stalign);
            }
        }
        switch (op) {
            case LABEL:
            case LDR_RAX:
            case LDR_RCX:
            case LDR_RDX:
            case LDR_RBX:
            case LANDING:
            case LDR_RBP:
            case LDR_RSI:
            case LDR_RDI:
            case LDR_R8:
            case LDR_R9:
            case LDR_RAX_RAX:
            case STR_RAX_RBX:
            case MOV_RDI_RAX:
            case MOV_RAX_RDI:
            case OR_RAX_reg:
            case XOR_RAX_reg:
            case AND_RAX_reg:
            case ADD_RAX_reg:
            case SUB_RAX_reg:
            case MUL_RAX_reg:
            case DIV_RAX_reg:
            case MUL_RAX_TWO:
            case BLR_RAX:
            case COMMUTE:
            case SELECT:
            case BX_IMM:
            case BX_IMM_1:
                for (i = 0; i < r->incsp; i++) {
                    play_data(*p, i + 32);
                    free(*p);
                    p++;
                }
                for (i = 0; i < 32; i++) {
                    if (r->output & (1 << i)) {
                        free(value[i]);
                        value[i] = *p++;
                    }
                }
                for (i = 0; i < 32; i++) {
                    if (i == 4 || i == 5) {
                        continue;
                    }
                    if (r->output & (1 << i)) {
                        play_data(value[i], i);
                    }
                }
                for (i = 4; i < 6; i++) {
                    if (r->output & (1 << i)) {
                        play_data(value[i], i);
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
                    if (i > 9) {
                        continue;
                    }
                    printx(" %s=%s", x86regs[i], value[i] ? value[i] : "0");
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
    free(trampoline_rdi);
}


void
emit_load_direct(const char *value, BOOL deref0)
{
    make1(LDR_RAX, value);
    if (deref0) {
        make1(LDR_RAX_RAX);
    }
}


void
emit_load_indirect(const char *lvalue, BOOL deref0)
{
    char *tmp = create_address_str(lvalue, 0);
    make1(LDR_RAX, tmp);
    make1(LDR_RAX_RAX);
    if (deref0) {
        make1(LDR_RAX_RAX);
    }
    free(tmp);
}


void
emit_store_indirect(const char *lvalue)
{
    char *tmp = create_address_str(lvalue, 0);
    make1(LDR_RBX, tmp);
    make1(STR_RAX_RBX);
    free(tmp);
}


void
emit_store_direct(const char *lvalue)
{
    make1(LDR_RBX, lvalue);
    make1(STR_RAX_RBX);
}


void
emit_or(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(OR_RAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_RAX, value);
    while (deref0--) {
        make1(LDR_RAX_RAX);
    }
    make1(r->op);
}


void
emit_xor(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(XOR_RAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_RAX, value);
    while (deref0--) {
        make1(LDR_RAX_RAX);
    }
    make1(r->op);
}


void
emit_and(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(AND_RAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_RAX, value);
    while (deref0--) {
        make1(LDR_RAX_RAX);
    }
    make1(r->op);
}


void
emit_add(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(ADD_RAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_RAX, value);
    while (deref0--) {
        make1(LDR_RAX_RAX);
    }
    make1(r->op);
}


void
emit_sub(const char *value, const char *addend, int deref0)
{
    const struct R_OPDEF *r;
    r = solve_op(SUB_RAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_RAX, value);
    while (deref0--) {
        make1(LDR_RAX_RAX);
    }
    make1(r->op);
}


void
emit_mul(const char *value, const char *multiplier, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, multiplier);
#if 1
    r = try_solve_op(MUL_RAX_reg);
    if (!r->addr) {
        int pot = is_pot_str(multiplier); // XXX requires in-house arithmetic
        if (pot > 1) {
            int q = 1;
            char addend;
            solve_op(MUL_RAX_TWO);
            addend = optab[MUL_RAX_TWO].flags;
            make1(LDR_RAX, value);
            while (deref0--) {
                make1(LDR_RAX_RAX);
            }
            while (--pot) {
                make1(MUL_RAX_TWO);
                q *= 2;
            }
            if (addend) {
                char str[16];
                sprintf(str, "%d", (1 - q) * addend);
                r = solve_op(ADD_RAX_reg);
                make1(r->flags & 0xFF, str);
                make1(r->op);
            }
            return;
        } else if (pot == 1) {
            make1(LDR_RAX, value);
            while (deref0--) {
                make1(LDR_RAX_RAX);
            }
            return;
        }
    }
#endif
    r = solve_op(MUL_RAX_reg);
    make1(r->flags & 0xFF, multiplier);
    make1(LDR_RAX, value);
    while (deref0--) {
        make1(LDR_RAX_RAX);
    }
    make1(r->op);
}


void
emit_div(const char *value, const char *multiplier, int deref0)
{
    // XXX we can detect POT and do shift here (would help with Montgomery reduction)
    const struct R_OPDEF *r;
    r = solve_op(DIV_RAX_reg);
    if (r->flags & GADGET_DIV_one_rdi) {
        make1(LDR_RDI, "1");
    }
    make1(r->flags & 0xFF, multiplier);
    make1(LDR_RDX, "0");
    make1(LDR_RAX, value);
    while (deref0--) {
        make1(LDR_RAX_RAX);
    }
    make1(r->op);
}


void
emit_call(const char *func, char **args, int nargs, int deref0, BOOL inloop, BOOL retval, int attr, int regparm, int restack)
{
    char *tmp = NULL;
    enum R_OP op = BLR_RAX;
    int rargs = nargs;
    char *noargs[MAX_FUNC_ARGS];
    if (rargs > regparm) {
        rargs = regparm;
    }
    assert(rargs <= 6 && nargs - rargs <= 4);
    if (args == NULL) {
        assert(nargs == 1);
        args = noargs;
        memset(noargs, 0, sizeof(noargs));
        rargs = nargs = 0;
        make1(MOV_RDI_RAX);
    } else if (rargs > 0 && deref0) {
        make1(LDR_RAX, args[0]);
        while (deref0--) {
            make1(LDR_RAX_RAX);
        }
        make1(MOV_RDI_RAX);// XXX need this, regardless of emit_save/emit_restore
    } else if (rargs > 0) {
        make1(LDR_RDI, args[0]);
    }
    switch (rargs) {
        case 6:
            make1(LDR_R9, args[5]);
        case 5:
            make1(LDR_R8, args[4]);
        case 4:
            make1(LDR_RCX, args[3]);
        case 3:
            make1(LDR_RDX, args[2]);
        case 2:
            make1(LDR_RSI, args[1]);
    }
    if (attr & ATTRIB_NORETURN) {
        assert(nargs - rargs <= 1);
        make1(BX_IMM + nargs - rargs, func, args + rargs); // XXX align stack
        return;
    }
    make1(LDR_RAX, func);
    if (1 || (attr & ATTRIB_STACK) || inloop) {
        char *skip;
        tmp = new_name("res");
        skip = create_address_str(tmp, -8);
        make1(LDR_RBP, skip);
        make1(COMMUTE, ((attr & ATTRIB_STACK) || inloop) ? restack : 0, 16);
        make1(LABEL, tmp, RBP);
        free(skip);
    }
    if (attr & ATTRIB_STDCALL) {
        cry("attribute stdcall not implemented\n");
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
    make1(MOV_RDI_RAX);
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
    make1(MOV_RAX_RDI);
    (void)scratch;
#endif
}


void
emit_goto(const char *label)
{
    char *label8 = create_address_str(label, 8);
    make1(LDR_RBP, label8);
    make1(COMMUTE, 0, 0);
    free(label8);
}


void
emit_cond(const char *label, enum cond_t cond)
{
    char *dst = new_name("dst");
    char *next = new_name("nxt");
    char *pivot = create_address_str(dst, -8);
    char *next8 = create_address_str(next, -8);
    char *label8 = create_address_str(label, 8);
    BOOL inverse = (cond == COND_EQ);
    const struct R_OPDEF *r = solve_op(SELECT);
    assert(cond == COND_NE || cond == COND_EQ);
    int reg_one = r->flags & 0xFF;
    int reg_two = (r->flags >> 8) & 0xFF;
    if (inverse) {
        int reg = reg_one;
        reg_one = reg_two;
        reg_two = reg;
    }
    make1(reg_one, label8);
    make1(reg_two, next8);
    make1(SELECT);
    emit_store_direct(pivot);
    if ((optab[STR_RAX_RBX].output & RBP) == 0) {
        make1(LANDING);
    }
    make1(LABEL, dst, 0);
    make1(COMMUTE, 0, 0);
    make1(LABEL, next, 0);
    free(label8);
    free(next8);
    free(pivot);
    free(next);
    free(dst);
}


void
emit_label(const char *label, int used, BOOL last)
{
    if (!used) {
        make1(LABEL, label, 0);
        cry("unused label '%s'\n", label);
    } else if (last) {
        make1(LABEL, label, 0xFFFFFFFF);
        make1(LANDING);
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
    solve_op(LDR_RBP);
    add_extern("__gadget_ret", optab[LDR_RBP].addr + 1, 0, -1);
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
    return "x86-64";
}
