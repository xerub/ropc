#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"
#include "parser.h"
#include "lexer.h"
#include "emit.h"
#include "code.h"
#include "backend.h"
#include "symtab.h"


#define IS(t) (token.type == (t))


#ifdef PARSER_DEBUG
#define ENTER() printf("%*c %s(%s)\n", 2 * logindent++, ';', __FUNCTION__, token.sym)
#define LEAVE() --logindent
static int logindent = 0;
#else
#define ENTER()
#define LEAVE()
#endif


static void __attribute__((noreturn))
expect(const char *what)
{
    const char *sym = token.sym;
    if (IS(T_EOI)) {
        sym = "end of input";
    }
    die("expected %s before %s\n", what, sym);
}


static struct node *R_or_exp(struct the_node *the_node);
static struct imm_node *R_immediate_exp(void);


static int
R_attribute(int allow, int *regparm, int *restack)
{
    int attr = 0;
    const char *sym = token.sym;
    ENTER();
    if (!IS(T_ID)) {
        expect("attribute");
    }
    if (!strcmp(sym, "noreturn")) {
        attr = ATTRIB_NORETURN;
    } else if (!strcmp(sym, "stdcall")) {
        attr = ATTRIB_STDCALL;
    } else if (!strcmp(sym, "stack")) {
        if (peek_token() == T_ASSIGN) {
            long n;
            next_token(); /* skip 'stack' */
            next_token(); /* skip '=' */
            if (!IS(T_INT)) {
                expect("integer");
            }
            n = strtol(token.sym, NULL, 0);
            if (n > 0 && n <= 0x2000) {
                *restack = n;
            } else {
                cry("invalid restack value %ld, using defaults\n", n);
            }
        }
        attr = ATTRIB_STACK;
    } else if (!strcmp(sym, "regparm")) {
        long n;
        next_token(); /* skip 'regparm' */
        if (!IS(T_ASSIGN)) {
            expect("'='");
        }
        next_token(); /* skip '=' */
        if (!IS(T_INT)) {
            expect("integer");
        }
        n = strtol(token.sym, NULL, 10);
        if (n >= 0 && n <= 8) {
            *regparm = n;
            attr = ATTRIB_REGPARM;
        }
    } else {
        expect("attribute");
    }
    if (!(allow & attr)) {
        cry("ignoring attribute '%s'\n", sym);
        attr = ATTRIB_UNKNOWN;
    }
    next_token(); /* skip attribute */
    LEAVE();
    return attr;
}


static int
R_attribute_list(int allow, int *regparm, int *restack)
{
    int attr;
    ENTER();
    attr = R_attribute(allow, regparm, restack);
    while (IS(T_COMMA)) {
        next_token(); /* skip ',' */
        attr |= R_attribute(allow, regparm, restack);
    }
    LEAVE();
    return attr;
}


static int
R_attribute_spec(int allow, int *regparm, int *restack)
{
    int attr = 0;
    ENTER();
    if (IS(T_OPENSQUARE) && peek_token() == T_OPENSQUARE) {
        next_token(); /* skip '[' */
        next_token(); /* skip '[' */
        attr = R_attribute_list(allow, regparm, restack);
        if (!IS(T_CLOSESQUARE)) {
            expect("']'");
        }
        next_token(); /* skip ']' */
        if (!IS(T_CLOSESQUARE)) {
            expect("']'");
        }
        next_token(); /* skip ']' */
    }
    LEAVE();
    return attr;
}


static struct node *
R_initializer_list(int *num)
{
    struct node *n, *p;
    ENTER();
    n = (struct node *)R_immediate_exp();
    *num = 1;
    for (p = n; IS(T_COMMA); p = p->next) {
        next_token(); /* skip ',' */
        p->next = (struct node *)R_immediate_exp();
        (*num)++;
    }
    LEAVE();
    return n;
}


static struct imm_node *
R_immediate_exp(void)
{
    struct imm_node *n = alloc_imm_node();
    BOOL negative = FALSE;
    ENTER();
    if (IS(T_AND)) {
        next_token(); /* skip '&' */
        if (!IS(T_ID)) {
            expect("identifier");
        }
        n->value = create_address_str(token.sym, 0);
        next_token(); /* skip ID */
        LEAVE();
        return n;
    }
    if (IS(T_AT) && enable_cfstr) {
        char buf[32];
        const char **args;
        next_token(); /* skip '@' */
        if (!IS(T_STRING)) {
            expect("string");
        }
        sprintf(buf, "%zu", strlen(token.sym) - 2);
        args = xmalloc(4 * sizeof(char *));
        if (!try_symbol_extern("__CFConstantStringClassReference")) {
            die("__CFConstantStringClassReference not imported\n");
        }
        args[0] = xstrdup("__CFConstantStringClassReference");
        args[1] = xstrdup("0x7c8");
        args[2] = create_address_str(add_string(token.sym), 0);
        args[3] = xstrdup(buf);
        n->value = create_address_str(add_vector(args, 4), 0);
        next_token(); /* skip STRING */
        LEAVE();
        return n;
    }
    if (IS(T_STRING)) {
        n->value = create_address_str(add_string(token.sym), 0);
        next_token(); /* skip STRING */
        LEAVE();
        return n;
    }
    if (IS(T_OPENCURLY)) {
        int i;
        struct node *p;
        const char **args;
        next_token(); /* skip '{' */
        p = R_initializer_list(&i);
        args = xmalloc(i * sizeof(char *));
        for (i = 0; p; i++) {
            struct node *next = p->next;
            args[i] = AS_IMM(p)->value;
            free(p);
            p = next;
        }
        n->value = create_address_str(add_vector(args, i), 0);
        if (!IS(T_CLOSECURLY)) {
            expect("'}'");
        }
        next_token(); /* skip '}' */
        LEAVE();
        return n;
    }
    if (IS(T_ADD) || IS(T_SUB)) {
        if (IS(T_SUB)) {
            negative = TRUE;
        }
        next_token(); /* skip '+'/'-' */
    }
    if (IS(T_INT)) {
        n->value = create_number_str(negative, token.sym);
        next_token(); /* skip NUMBER */
        LEAVE();
        return n;
    }
    if (IS(T_ID) && try_symbol_extern(token.sym)) {
        n->value = xstrdup(token.sym);
        next_token(); /* skip ID */
        LEAVE();
        return n;
    }
    expect("immediate");
}


static int
R_type_qualifier(void)
{
    int attr = 0;
    ENTER();
    for (;;) {
        if (IS(T_K_CONST)) {
            next_token(); /* skip 'const' */
            attr |= ATTRIB_CONSTANT;
            continue;
        }
        if (IS(T_K_VOLATILE)) {
            next_token(); /* skip 'volatile' */
            attr |= ATTRIB_VOLATILE;
            continue;
        }
        break;
    }
    LEAVE();
    return attr;
}


static struct lval_node *
R_lvalue_exp(void)
{
    struct lval_node *n = alloc_lval_node();
    BOOL deref = FALSE;
    int attr = 0;
    ENTER();
    if (IS(T_MUL)) {
        next_token(); /* skip '*' */
        deref = TRUE;
    } else {
        attr = R_type_qualifier();
    }
    if (!IS(T_ID)) {
        expect("identifier");
    }
    n->name = xstrdup(token.sym);
    n->deref = deref;
    n->attr = attr;
    next_token(); /* skip ID */
    LEAVE();
    return n;
}


static struct node *
R_argument_exp_list(struct the_node *the_node)
{
    struct node *n, *p;
    ENTER();
    n = R_or_exp(the_node);
    for (p = n; IS(T_COMMA); p = p->next) {
        next_token(); /* skip ',' */
        p->next = R_or_exp(the_node);
    }
    LEAVE();
    return n;
}


static struct node *
R_rvalue_exp(struct the_node *the_node)
{
    struct node *n;
    int attr, regparm = -1, restack = -1;
    ENTER();
    attr = R_attribute_spec(ATTRIB_NORETURN | ATTRIB_STDCALL | ATTRIB_STACK | ATTRIB_REGPARM, &regparm, &restack);
    if (attr) {
        if (IS(T_ID) && peek_token() == T_OPENBRACE) {
            goto funcall;
        }
        expect("function call");
    }
    if (IS(T_ID) && peek_token() == T_OPENBRACE) funcall: {
        struct call_node *f = alloc_call_node();
        attr |= try_symbol_attr(token.sym);
        the_node->attr = attr;
        f->func = xstrdup(token.sym);
        f->parm = NULL;
        f->attr = attr;
        f->regparm = regparm;
        f->restack = restack;
        next_token(); /* skip ID */
        next_token(); /* skip '(' */
        if (!IS(T_CLOSEBRACE)) {
            f->parm = R_argument_exp_list(the_node);
        }
        if (!IS(T_CLOSEBRACE)) {
            expect("')'");
        }
        next_token(); /* skip ')' */
        n = (struct node *)f;
    } else if (IS(T_INT) || IS(T_STRING) || IS(T_AND) || IS(T_AT) || IS(T_ADD) || IS(T_SUB) || IS(T_OPENCURLY)) {
        n = (struct node *)R_immediate_exp();
    } else if (IS(T_OPENBRACE)) {
        next_token(); /* skip '(' */
        n = (struct node *)R_or_exp(the_node);
        if (!IS(T_CLOSEBRACE)) {
            expect("')'");
        }
        next_token(); /* skip ')' */
    } else {
        n = (struct node *)R_lvalue_exp();
    }
    LEAVE();
    return n;
}


static struct node *
R_multiplicative_exp(struct the_node *the_node)
{
    struct mul_node *n;
    struct node *p, *q;
    ENTER();
    n = alloc_mul_node();
    n->list = R_rvalue_exp(the_node);
    for (p = n->list; IS(T_MUL) || IS(T_DIV); ) {
        int negative = IS(T_DIV);
        next_token(); /* skip '*' */
        q = R_rvalue_exp(the_node);
        q->inverse = negative;
        negative ^= p->inverse;
        /* XXX division is not allowed here: x / 3 * 3 would be converted to x / (3 / 3) which is not what we want */
        if (optimize_add && p->type == NODE_IMM && q->type == NODE_IMM && !is_address(AS_IMM(p)->value) && !is_address(AS_IMM(q)->value) && !negative) {
            char *v1 = AS_IMM(p)->value;
            char *v2 = AS_IMM(q)->value;
            AS_IMM(p)->value = create_op_str(v1, v2, negative ? '/' : '*');
            free(v1);
            free(v2);
            free(q);
            continue;
        }
        p->next = q;
        p = p->next;
    }
    if (n->list->next == NULL) {
        /* reduce multiplicative to simple factor if possible */
        p = (struct node *)n;
        n = (struct mul_node *)n->list;
        free(p);
    }
    LEAVE();
    return (struct node *)n;
}


static struct node *
R_additive_exp(struct the_node *the_node)
{
    struct add_node *n;
    struct node *p, *q;
    ENTER();
    n = alloc_add_node();
    n->list = R_multiplicative_exp(the_node);
    for (p = n->list; IS(T_ADD) || IS(T_SUB); ) {
        int negative = IS(T_SUB);
        next_token(); /* skip '+' */
        q = R_multiplicative_exp(the_node);
        q->inverse = negative;
        negative ^= p->inverse;
        /* XXX TODO a proper constant folding pass */
        if (optimize_add && p->type == NODE_IMM && q->type == NODE_IMM && ((!is_address(AS_IMM(p)->value) ^ negative) | !is_address(AS_IMM(q)->value))) {
            char *v1 = AS_IMM(p)->value;
            char *v2 = AS_IMM(q)->value;
            if (is_address(v1) && is_address(v2)) {
                char *c1 = curate_address(v1);
                char *c2 = curate_address(v2);
                free(v1);
                free(v2);
                v1 = c1;
                v2 = c2;
            }
            AS_IMM(p)->value = create_op_str(v1, v2, negative ? '-' : '+');
            free(v1);
            free(v2);
            free(q);
            continue;
        }
        p->next = q;
        p = p->next;
    }
    if (n->list->next == NULL) {
        /* reduce additive to simple term if possible */
        p = (struct node *)n;
        n = (struct add_node *)n->list;
        free(p);
    }
    LEAVE();
    return (struct node *)n;
}


static struct node *
R_and_exp(struct the_node *the_node)
{
    struct and_node *n;
    struct node *p, *q;
    ENTER();
    n = alloc_and_node();
    n->list = R_additive_exp(the_node);
    for (p = n->list; IS(T_AND); ) {
        next_token(); /* skip '&' */
        q = R_additive_exp(the_node);
        if (optimize_add && p->type == NODE_IMM && q->type == NODE_IMM && !is_address(AS_IMM(p)->value) && !is_address(AS_IMM(q)->value)) {
            char *v1 = AS_IMM(p)->value;
            char *v2 = AS_IMM(q)->value;
            AS_IMM(p)->value = create_op_str(v1, v2, '&');
            free(v1);
            free(v2);
            free(q);
            continue;
        }
        p->next = q;
        p = p->next;
    }
    if (n->list->next == NULL) {
        /* reduce and to simple factor if possible */
        p = (struct node *)n;
        n = (struct and_node *)n->list;
        free(p);
    }
    LEAVE();
    return (struct node *)n;
}


static struct node *
R_xor_exp(struct the_node *the_node)
{
    struct xor_node *n;
    struct node *p, *q;
    ENTER();
    n = alloc_xor_node();
    n->list = R_and_exp(the_node);
    for (p = n->list; IS(T_XOR); ) {
        next_token(); /* skip '^' */
        q = R_and_exp(the_node);
        if (optimize_add && p->type == NODE_IMM && q->type == NODE_IMM && !is_address(AS_IMM(p)->value) && !is_address(AS_IMM(q)->value)) {
            char *v1 = AS_IMM(p)->value;
            char *v2 = AS_IMM(q)->value;
            AS_IMM(p)->value = create_op_str(v1, v2, '^');
            free(v1);
            free(v2);
            free(q);
            continue;
        }
        p->next = q;
        p = p->next;
    }
    if (n->list->next == NULL) {
        /* reduce xor to simple factor if possible */
        p = (struct node *)n;
        n = (struct xor_node *)n->list;
        free(p);
    }
    LEAVE();
    return (struct node *)n;
}


static struct node *
R_or_exp(struct the_node *the_node)
{
    struct or_node *n;
    struct node *p, *q;
    ENTER();
    n = alloc_or_node();
    n->list = R_xor_exp(the_node);
    for (p = n->list; IS(T_OR); ) {
        next_token(); /* skip '|' */
        q = R_xor_exp(the_node);
        if (optimize_add && p->type == NODE_IMM && q->type == NODE_IMM && !is_address(AS_IMM(p)->value) && !is_address(AS_IMM(q)->value)) {
            char *v1 = AS_IMM(p)->value;
            char *v2 = AS_IMM(q)->value;
            AS_IMM(p)->value = create_op_str(v1, v2, '|');
            free(v1);
            free(v2);
            free(q);
            continue;
        }
        p->next = q;
        p = p->next;
    }
    if (n->list->next == NULL) {
        /* reduce or to simple factor if possible */
        p = (struct node *)n;
        n = (struct or_node *)n->list;
        free(p);
    }
    LEAVE();
    return (struct node *)n;
}


static struct node *
R_assignment_exp(struct the_node *the_node)
{
    struct node *n;
    ENTER();
    /* we cannot tell here if we have lvalue or rvalue, so use magic */
    n = R_or_exp(the_node);
    if (n->type == NODE_LVAL && IS(T_ASSIGN)) {
        next_token(); /* skip '=' */
        n->next = R_assignment_exp(the_node);
    }
    LEAVE();
    return n;
}


static char *
R_jump_stat(void)
{
    char *label;
    ENTER();
    next_token(); /* skip 'goto' */
    if (!IS(T_ID)) {
        expect("label");
    }
    label = xstrdup(token.sym);
    next_token(); /* skip label */
    LEAVE();
    return label;
}


static void
R_selection_stat(struct the_node *the_node)
{
    struct node *n;
    char *label;
    enum cond_t cond = COND_NE;
    ENTER();
    next_token(); /* skip 'if' */
    if (IS(T_LOGICNOT)) {
        cond = COND_EQ;
        next_token(); /* skip '!' */
    }
    if (!IS(T_OPENBRACE)) {
        expect("'('");
    }
    next_token(); /* skip '(' */
    n = R_assignment_exp(the_node);
    if (!IS(T_CLOSEBRACE)) {
        expect("')'");
    }
    next_token(); /* skip ')' */
    if (!IS(T_K_GOTO)) {
        expect("'goto'");
    }
    label = R_jump_stat();
#ifdef DATA_DEBUG
    walk_nodes(n, 0);
#endif
    the_node->code = n;
    the_node->cond = cond;
    the_node->jump = label;
    LEAVE();
}


static void
R_stat(struct the_node *the_node)
{
    struct node *n;
    ENTER();
    if (IS(T_K_IF)) {
        R_selection_stat(the_node);
        LEAVE();
        return;
    }
    if (IS(T_K_GOTO)) {
        char *label;
        label = R_jump_stat();
        the_node->jump = label;
        LEAVE();
        return;
    }
    n = R_assignment_exp(the_node);
#ifdef DATA_DEBUG
    walk_nodes(n, 0);
#endif
    the_node->code = n;
    LEAVE();
}


static void
R_labeled_stat(struct the_node *the_node)
{
    struct label_node *p = NULL;
    ENTER();
    while (IS(T_ID) && peek_token() == T_COLON) {
        p = new_label(p, token.sym);
        next_token(); /* skip ID */
        next_token(); /* skip ':' */
    }
    the_node->labels = p;
    if (!IS(T_EOI)) {
        R_stat(the_node);
    }
    LEAVE();
}


static void
R_external_decl(struct the_node *the_node)
{
    ENTER();
    if (IS(T_K_EXTERN)) {
        const char *import;
        int attr, regparm = -1, restack = -1;
        unsigned long long val = -1;
        next_token(); /* skip 'extern' */
        if (!IS(T_ID)) {
            expect("identifier");
        }
        import = token.sym;
        next_token(); /* skip ID */
        attr = R_attribute_spec(ATTRIB_NORETURN | ATTRIB_STDCALL | ATTRIB_REGPARM, &regparm, &restack);
        if (IS(T_ASSIGN)) {
            next_token(); /* skip '=' */
            if (!IS(T_INT)) {
                expect("integer");
            }
            val = strtoull(token.sym, NULL, 0);
            next_token(); /* skip NUMBER */
        }
        emit_extern(import, val, attr, regparm);
    } else {
        R_labeled_stat(the_node);
    }
    LEAVE();
}


struct the_node *
parse(const char *s)
{
    struct the_node *the_node = NULL;
    int n = tokenize(s);
    if (n > 0) {
        next_token(); /* pre-prime the first token */
        the_node = create_the_node();
        R_external_decl(the_node);
        if (!IS(T_EOI)) {
            die("junk at %s\n", token.sym);
        }
    }
    free_tokens(FALSE);
    return the_node;
}
