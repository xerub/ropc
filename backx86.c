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


#define EAX (1 << 0)
#define ECX (1 << 1)
#define EDX (1 << 2)
#define EBX (1 << 3)
#define ESP (1 << 4)
#define EBP (1 << 5)
#define ESI (1 << 6)
#define EDI (1 << 7)

typedef char *BRICK;

enum R_OP {
    BAD,
    LABEL,
    LDR_EAX,
    LDR_ECX,
    LDR_EDX,
    LDR_EBX,
    LANDING,
    LDR_EBP,
    LDR_ESI,
    LDR_EDI,
    LDR_EAX_EAX,
    STR_EAX_ECX,
    MOV_EDX_EAX,
    MOV_EAX_EDX,
    OR_EAX_reg,
    XOR_EAX_reg,
    AND_EAX_reg,
    ADD_EAX_reg,
    SUB_EAX_reg,
    MUL_EAX_reg,
    DIV_EAX_reg,
    MUL_EAX_TWO,
    BLR_EAX_2,
    BLR_EAX_6,
    BLR_EAX_16,
    COMMUTE,
    SELECT,
    NOT_EAX,
    BX_IMM,
    BX_IMM_1
};

#define GADGET_pop_SDacdbP      0
#define GADGET_pop_acdbSDP      0x80000000
#define GADGET_DIV_null_ecx     0x100

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
    { BAD,          /**/ 0,           /**/ 0,                           /**/ 0,               /**/  0, 0, 0, NULL },
    { LABEL,        /**/ 0,           /**/ 0,                           /**/ 0,               /**/  0, 0, 0, NULL },
    { LDR_EAX,      /**/ 0,           /**/ EAX,                         /**/ 0,               /**/  0, 0, 0, "pop eax / ret" },
    { LDR_ECX,      /**/ 0,           /**/     ECX,                     /**/ 0,               /**/  0, 0, 0, "pop ecx / ret" },
    { LDR_EDX,      /**/ 0,           /**/         EDX,                 /**/ 0,               /**/  0, 0, 0, "pop edx / ret" },
    { LDR_EBX,      /**/ 0,           /**/             EBX,             /**/ 0,               /**/  0, 0, 0, "pop ebx / ret" },
    { LANDING,      /**/ 0,           /**/                 EBP,         /**/     EBP,         /**/  0, 0, 0, "pop ebp / ret" },
    { LDR_EBP,      /**/ 0,           /**/                 EBP,         /**/ 0,               /**/  0, 0, 0, "pop ebp / ret" },
    { LDR_ESI,      /**/ 0,           /**/                     ESI,     /**/ 0,               /**/  0, 0, 0, "pop esi / ret" },
    { LDR_EDI,      /**/ 0,           /**/                         EDI, /**/ 0,               /**/  0, 0, 0, "pop edi / ret" },
    { LDR_EAX_EAX,  /**/ EAX,         /**/ 0,                           /**/ 0,               /**/  0, 0, 0, "mov eax, [eax] / ret" },
    { STR_EAX_ECX,  /**/ 0,           /**/ 0,                           /**/ 0,               /**/  0, 0, 0, "mov [ecx], eax / ret" },
    { MOV_EDX_EAX,  /**/         EDX, /**/ 0,                           /**/ 0,               /**/  0, 0, 0, "mov edx, eax / ret" },
    { MOV_EAX_EDX,  /**/ EAX,         /**/ 0,                           /**/ 0,               /**/  0, 0, 0, "mov eax, edx / ret" },
    { OR_EAX_reg,   /**/ EAX,         /**/                 EBP,         /**/     EBP,         /**/  0, LDR_ECX, 0, "or eax, ecx / pop ebp / ret" },
    { XOR_EAX_reg,  /**/ EAX,         /**/ 0,                           /**/ 0,               /**/  0, LDR_ECX, 0, "xor eax, ecx / ret" },
    { AND_EAX_reg,  /**/ EAX,         /**/ 0,                           /**/ 0,               /**/  0, LDR_ECX, 0, "and eax, ecx / ret" },
    { ADD_EAX_reg,  /**/ EAX,         /**/ 0,                           /**/ 0,               /**/  0, LDR_ECX, 0, "add eax, ecx / ret" },
    { SUB_EAX_reg,  /**/ EAX,         /**/ 0,                           /**/ 0,               /**/  0, LDR_ECX, 0, "sub eax, ecx / ret" },
    { MUL_EAX_reg,  /**/ EAX|    EDX, /**/             EBX,             /**/ EBX,             /**/  0, LDR_EDX, 0, "mul edx / add ebx, ecx / add edx, ebx / pop ebx / ret" },
    { DIV_EAX_reg,  /**/ EAX|    EDX, /**/             EBX,             /**/ EBX,             /**/  0, LDR_ECX, 0, "div ecx / mov edx, ebx / pop ebx / ret" },
    { MUL_EAX_TWO,  /**/ EAX,         /**/ 0,                           /**/ 0,               /**/  0, 0, 0, "lea eax, [eax+eax] / ret" },
    { BLR_EAX_2,    /**/ EAX|ECX|EDX, /**/                 EBP,         /**/     EBP,         /**/  2, 0, 0, "call eax / add esp, 8 / pop ebp / ret" },
    { BLR_EAX_6,    /**/ EAX|ECX|EDX, /**/                 EBP,         /**/     EBP,         /**/  6, 0, 0, "call eax / add esp, 0x18 / pop ebp / ret" },
    { BLR_EAX_16,   /**/ EAX|ECX|EDX, /**/                 EBP|ESI|EDI, /**/     EBP|ESI|EDI, /**/ 16, 0, 0, "call eax / add esp, 0x40 / pop esi / pop edi / pop ebp / ret" },
    { COMMUTE,      /**/ 0,           /**/                 EBP,         /**/     EBP,         /**/  0, 0, 0, "mov esp, ebp / pop ebp / ret" },
    { SELECT,       /**/ EAX|ECX,     /**/             EBX|EBP|ESI|EDI, /**/ EBX|EBP|ESI|EDI, /**/  3, LDR_ECX | (LDR_ESI << 8), 0, "test eax, eax / cmovnz esi, ecx / mov eax, esi / add esp, 0xc / pop esi / pop edi / pop ebx / pop ebp / ret" },
    { NOT_EAX,      /**/ EAX,         /**/ 0,                           /**/ 0,               /**/  0, 0, 0, "test eax,eax / sete al / movzx eax,al / ret" },
    { BX_IMM,       /**/ 0xFFFFFFFF,  /**/ 0,                           /**/ 0,               /**/  0, 0, 0, NULL },
    { BX_IMM_1,     /**/ 0xFFFFFFFF,  /**/ 0,                           /**/ 0,               /**/  1, 0, 0, NULL },
};


static int idx = 0;
static BRICK strip[10240];

static unsigned dirty = 0xFFFFFFFF;
static int pointer[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static struct range *ranges = NULL;

const int arch_regparm = 0;


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
solve_mul_eax_two(const unsigned char *p, uint32_t size, uint64_t addr, void *user)
{
    static char text[256];
    struct R_OPDEF *r = (struct R_OPDEF *)user;
    if (p[4] == 0xC3) {
        r->flags = p[3];
        r->text = text;
        sprintf(text, "lea eax, [eax+eax%+d] / pop ebp / ret", (char)p[3]);
        return 0;
    }
    if (p[4] == 0x5D && p[5] == 0xC3) {
        r->output = EBP;
        r->auxout = EBP;
        r->flags = p[3];
        r->text = text;
        sprintf(text, "lea eax, [eax+eax%+d] / pop ebp / ret", (char)p[3]);
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
        case LDR_EAX:
            rv = parse_string(ranges, binmap, NULL, NULL, "58 C3");
            break;
        case LDR_ECX:
            rv = parse_string(ranges, binmap, NULL, NULL, "59 C3");
            break;
        case LDR_EDX:
            rv = parse_string(ranges, binmap, NULL, NULL, "5A C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "5A 5D C3");
            if (rv) {
                r->output = EBP | EDX;
                r->auxout = EBP;
                r->text = "pop edx / pop ebp / ret";
                break;
            }
            break;
        case LDR_EBX:
            rv = parse_string(ranges, binmap, NULL, NULL, "5B C3");
            break;
        case LDR_EBP:
        case LANDING:
            rv = parse_string(ranges, binmap, NULL, NULL, "5D C3");
            break;
        case LDR_ESI:
            rv = parse_string(ranges, binmap, NULL, NULL, "5E C3");
            break;
        case LDR_EDI:
            rv = parse_string(ranges, binmap, NULL, NULL, "5F C3");
            break;
        case LDR_EAX_EAX:
            rv = parse_string(ranges, binmap, NULL, NULL, "8B 00 C3");
            break;
        case STR_EAX_ECX:
            rv = parse_string(ranges, binmap, NULL, NULL, "89 01 C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "89 01 5B C3");
            if (rv) {
                r->output = EBX;
                r->auxout = EBX;
                r->text = "mov [ecx], eax / pop ebx / ret";
                break;
            }
            break;
        case MOV_EDX_EAX:
            rv = parse_string(ranges, binmap, NULL, NULL, "89 C2 C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "89 C2 5B 89 D0 5E 5F 5D C3");
            if (rv) {
                r->output = EBX | ESI | EDI | EBP;
                r->auxout = EBX | ESI | EDI | EBP;
                r->flags = GADGET_pop_acdbSDP;
                r->text = "mov edx, eax / pop ebx / mov eax, edx / pop esi / pop edi / pop ebp / ret";
                break;
            }
            break;
        case MOV_EAX_EDX:
            rv = parse_string(ranges, binmap, NULL, NULL, "89 D0 C3");
            break;
        case OR_EAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "09 C8 5D C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "09 C8 5B C3");
            if (rv) {
                r->output = EBX;
                r->auxout = EBX;
                r->text = "or eax, ecx / pop ebx / ret";
                break;
            }
            break;
        case XOR_EAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "31 C8 C3");
            break;
        case AND_EAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "21 C8 C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "21 C8 5D C3");
            if (rv) {
                r->output = EBP;
                r->auxout = EBP;
                r->text = "and eax, ecx / pop ebp / ret";
                break;
            }
            break;
        case ADD_EAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "01 C8 C3");
            break;
        case SUB_EAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "29 C8 C3");
            break;
        case MUL_EAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "F7 E2 01 CB 01 DA 5B C3");
            break;
        case DIV_EAX_reg:
            rv = parse_string(ranges, binmap, NULL, NULL, "F7 F1 89 DA 5B C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "F7 F6 5B 5E 5F 01 C8 C3");
            if (rv) {
                r->output = EBX | ESI | EDI;
                r->auxout = EBX | ESI | EDI;
                r->flags = LDR_ESI | GADGET_DIV_null_ecx | GADGET_pop_acdbSDP;
                r->text = "div esi / pop ebx / pop esi / pop edi / add eax, ecx / ret";
                break;
            }
            break;
        case MUL_EAX_TWO:
            rv = parse_string(ranges, binmap, r, solve_mul_eax_two, "8D 44 00");
            break;
        case BLR_EAX_2:
            rv = parse_string(ranges, binmap, NULL, NULL, "FF D0 83 C4 08 5D C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "FF D0 83 C4 0C C3");
            if (rv) {
                r->output = 0;
                r->auxout = 0;
                r->incsp = 3;
                r->text = "call eax / add esp, 0xc / ret";
                break;
            }
            break;
        case BLR_EAX_6:
            rv = parse_string(ranges, binmap, NULL, NULL, "FF D0 83 C4 18 5D C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "FF D0 83 C4 1C C3");
            if (rv) {
                r->output = 0;
                r->auxout = 0;
                r->incsp = 7;
                r->text = "call eax / add esp, 0x1c / ret";
                break;
            }
            break;
        case BLR_EAX_16:
            rv = parse_string(ranges, binmap, NULL, NULL, "FF D0 83 C4 40 5E 5F 5D C3");
            break;
        case COMMUTE:
            rv = parse_string(ranges, binmap, NULL, NULL, "89 EC 5D C3");
            break;
        case SELECT:
            rv = parse_string(ranges, binmap, NULL, NULL, "85 C0 0F 45 F1 89 F0 83 C4 0C 5E 5F 5B 5D C3");
            if (rv) {
                break;
            }
            rv = parse_string(ranges, binmap, NULL, NULL, "8B 04 90 C3");
            if (rv) {
                r->spill = EAX;
                r->output = 0;
                r->auxout = 0;
                r->incsp = 0;
                r->flags = 0;
                r->text = "mov eax, [eax+edx*4] / ret";
                break;
            }
            break;
        case NOT_EAX:
            rv = parse_string(ranges, binmap, NULL, NULL, "85 C0 0F 94 C0 0F B6 C0 C3");
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
        case LDR_EAX:
        case LDR_ECX:
        case LDR_EDX:
        case LDR_EBX:
        case LDR_EBP:
        case LDR_ESI:
        case LDR_EDI:
            i = op - LDR_EAX;
            if (optimize_reg && pointer[i]) {
                strip[pointer[i]] = argdup(va_arg(ap, char *));
                pointer[i] = 0;
                goto done;
            }
            strip[idx++] = (BRICK)op;
            break;
        case LANDING:
        case LDR_EAX_EAX:
        case STR_EAX_ECX:
        case MOV_EAX_EDX:
        case MOV_EDX_EAX:
        case OR_EAX_reg:
        case XOR_EAX_reg:
        case AND_EAX_reg:
        case ADD_EAX_reg:
        case SUB_EAX_reg:
        case MUL_EAX_reg:
        case DIV_EAX_reg:
        case MUL_EAX_TWO:
        case SELECT:
        case NOT_EAX:
            strip[idx++] = (BRICK)op;
            for (i = 0; i < r->incsp; i++) {
                strip[idx++] = NULL;
            }
            break;
        case BLR_EAX_2:
        case BLR_EAX_6:
        case BLR_EAX_16: {
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
        case BX_IMM:
        case BX_IMM_1: {
            char *func = argdup(va_arg(ap, char *));
            char **args = va_arg(ap, char **);
            strip[idx++] = (BRICK)op;
            strip[idx++] = func;
            strip[idx++] = NULL;
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
    "EAX", "ECX", "EDX", "EBX", "esp", "EBP", "ESI", "EDI",
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
        sprintf(namebuf, "r%u", i);
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
    static const int pop_order_acdbSDP[] = { 0, 1, 2, 3, 6, 7, 4, 5 };
    static const int pop_order_SDacdbP[] = { 6, 7, 0, 1, 2, 3, 4, 5 };
    const int *pop_order;
    dirty = 0xFFFFFFFF;
    while (p < q) {
        enum R_OP op = (enum R_OP)(long)(*p++);
        const struct R_OPDEF *r = &optab[op];
        unsigned spill = r->spill;
        if (op == BX_IMM || op == BX_IMM_1) {
            BRICK arg = *p++;
            BRICK ret = *p++;
            const struct SYM *p = get_symbol(arg);
            assert(p);
            if (p->type != SYMBOL_EXTERN) {
                printx("%-7s dd    %-30s; -> PC\n", arg, p->val ? p->val : "0");
            } else {
                printx("        dg    0x%-28llX; -> PC: %s\n", p->addr, arg);
            }
            free(arg);
            printx("        dd    0x%-28X; -> noreturn\n", 0xbad);
            free(ret);
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
            if (restack < 0) {
                restack = inloop_stack;
            }
            if (restack) {
                printx("        times 0x%lx dd 0\n", restack);
            }
        }
        switch (op) {
            case LABEL:
            case LDR_EAX:
            case LDR_ECX:
            case LDR_EDX:
            case LDR_EBX:
            case LANDING:
            case LDR_EBP:
            case LDR_ESI:
            case LDR_EDI:
            case LDR_EAX_EAX:
            case STR_EAX_ECX:
            case MOV_EDX_EAX:
            case MOV_EAX_EDX:
            case OR_EAX_reg:
            case XOR_EAX_reg:
            case AND_EAX_reg:
            case ADD_EAX_reg:
            case SUB_EAX_reg:
            case MUL_EAX_reg:
            case DIV_EAX_reg:
            case MUL_EAX_TWO:
            case BLR_EAX_2:
            case BLR_EAX_6:
            case BLR_EAX_16:
            case COMMUTE:
            case SELECT:
            case NOT_EAX:
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
                pop_order = pop_order_SDacdbP;
                if (r->flags & GADGET_pop_acdbSDP) {
                    pop_order = pop_order_acdbSDP;
                }
                for (i = 0; i < 8; i++) {
                    int j = pop_order[i];
                    if (r->output & (1 << j)) {
                        play_data(value[j], j);
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
}


void
emit_load_direct(const char *value, BOOL deref0)
{
    make1(LDR_EAX, value);
    if (deref0) {
        make1(LDR_EAX_EAX);
    }
}


void
emit_load_indirect(const char *lvalue, BOOL deref0)
{
    char *tmp = create_address_str(lvalue, 0);
    make1(LDR_EAX, tmp);
    make1(LDR_EAX_EAX);
    if (deref0) {
        make1(LDR_EAX_EAX);
    }
    free(tmp);
}


void
emit_store_indirect(const char *lvalue)
{
    char *tmp = create_address_str(lvalue, 0);
    make1(LDR_ECX, tmp);
    make1(STR_EAX_ECX);
    free(tmp);
}


void
emit_store_direct(const char *lvalue)
{
    make1(LDR_ECX, lvalue);
    make1(STR_EAX_ECX);
}


void
emit_or(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(OR_EAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_EAX, value);
    while (deref0--) {
        make1(LDR_EAX_EAX);
    }
    make1(r->op);
}


void
emit_xor(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(XOR_EAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_EAX, value);
    while (deref0--) {
        make1(LDR_EAX_EAX);
    }
    make1(r->op);
}


void
emit_and(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(AND_EAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_EAX, value);
    while (deref0--) {
        make1(LDR_EAX_EAX);
    }
    make1(r->op);
}


void
emit_add(const char *value, const char *addend, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, addend);
    r = solve_op(ADD_EAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_EAX, value);
    while (deref0--) {
        make1(LDR_EAX_EAX);
    }
    make1(r->op);
}


void
emit_sub(const char *value, const char *addend, int deref0)
{
    const struct R_OPDEF *r;
    r = solve_op(SUB_EAX_reg);
    make1(r->flags & 0xFF, addend);
    make1(LDR_EAX, value);
    while (deref0--) {
        make1(LDR_EAX_EAX);
    }
    make1(r->op);
}


void
emit_mul(const char *value, const char *multiplier, int deref0, BOOL swap)
{
    const struct R_OPDEF *r;
    SWAP_PTR(swap, value, multiplier);
#if 1
    r = try_solve_op(MUL_EAX_reg);
    if (!r->addr) {
        int pot = is_pot_str(multiplier); // XXX requires in-house arithmetic
        if (pot > 1) {
            int q = 1;
            char addend;
            solve_op(MUL_EAX_TWO);
            addend = optab[MUL_EAX_TWO].flags;
            make1(LDR_EAX, value);
            while (deref0--) {
                make1(LDR_EAX_EAX);
            }
            while (--pot) {
                make1(MUL_EAX_TWO);
                q *= 2;
            }
            if (addend) {
                char str[16];
                sprintf(str, "%d", (1 - q) * addend);
                r = solve_op(ADD_EAX_reg);
                make1(r->flags & 0xFF, str);
                make1(r->op);
            }
            return;
        } else if (pot == 1) {
            make1(LDR_EAX, value);
            while (deref0--) {
                make1(LDR_EAX_EAX);
            }
            return;
        }
    }
#endif
    r = solve_op(MUL_EAX_reg);
    make1(r->flags & 0xFF, multiplier);
    make1(LDR_EAX, value);
    while (deref0--) {
        make1(LDR_EAX_EAX);
    }
    make1(r->op);
}


void
emit_div(const char *value, const char *multiplier, int deref0)
{
    // XXX we can detect POT and do shift here (would help with Montgomery reduction)
    const struct R_OPDEF *r;
    r = solve_op(DIV_EAX_reg);
    if (r->flags & GADGET_DIV_null_ecx) {
        make1(LDR_ECX, "0");
    }
    make1(r->flags & 0xFF, multiplier);
    make1(LDR_EDX, "0");
    make1(LDR_EAX, value);
    while (deref0--) {
        make1(LDR_EAX_EAX);
    }
    make1(r->op);
}


void
emit_call(const char *func, char **args, int nargs, int deref0, BOOL inloop, BOOL retval, int attr, int regparm, int restack)
{
    char *tmp = NULL;
    enum R_OP op = BLR_EAX_2;
    int rargs = nargs;
    if (rargs > regparm) {
        rargs = regparm;
    }
    assert(rargs == 0 && nargs - rargs <= 16);
    if (attr & ATTRIB_NORETURN) {
        assert(nargs - rargs <= 1);
        make1(BX_IMM + nargs - rargs, func, args + rargs);
        return;
    }
    make1(LDR_EAX, func);
    if ((attr & ATTRIB_STACK) || inloop) {
        char *skip;
        tmp = new_name("res");
        skip = create_address_str(tmp, -4);
        make1(LDR_EBP, skip);
        make1(COMMUTE, restack);
        make1(LABEL, tmp, EBP);
        free(skip);
    }
    if (attr & ATTRIB_STDCALL) {
        cry("attribute stdcall not implemented\n");
    }
    if (nargs - rargs > 6) {
        op = BLR_EAX_16;
    } else if (nargs - rargs > 2) {
        op = BLR_EAX_6;
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
    make1(MOV_EDX_EAX);
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
    make1(MOV_EAX_EDX);
    (void)scratch;
#endif
}


void
emit_goto(const char *label)
{
    char *label4 = create_address_str(label, 4);
    make1(LDR_EBP, label4);
    make1(COMMUTE, 0);
    free(label4);
}


void
emit_cond(const char *label, enum cond_t cond)
{
    char *dst = new_name("dst");
    char *next = new_name("nxt");
    char *pivot = create_address_str(dst, -4);
    char *next4 = create_address_str(next, -4);
    char *label4 = create_address_str(label, 4);
    BOOL inverse = (cond == COND_EQ);
    const struct R_OPDEF *r = solve_op(SELECT);
    assert(cond == COND_NE || cond == COND_EQ);
    int reg_one = r->flags & 0xFF;
    int reg_two = (r->flags >> 8) & 0xFF;
    if (reg_one == reg_two) {
        char *tab;
        const char **sel = xmalloc(2 * sizeof(char *));
        sel[inverse ^ 1] = xstrdup(next4);
        sel[inverse] = xstrdup(label4);
        tab = create_address_str(add_vector(sel, 2), 0);
        make1(NOT_EAX);
        make1(MOV_EDX_EAX); // XXX need this, regardless of emit_save/emit_restore
        make1(LDR_EAX, tab);
        free(tab);
        goto done;
    }
    if (inverse) {
        int reg = reg_one;
        reg_one = reg_two;
        reg_two = reg;
    }
    make1(reg_one, label4);
    make1(reg_two, next4);
  done:
    make1(SELECT);
    emit_store_direct(pivot);
    if ((optab[STR_EAX_ECX].output & EBP) == 0) {
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
    return "x86";
}
