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
static struct imm_node *R_immediate_exp(const char *death);
static void R_stat(struct the_node *the_node, struct the_node *ante, struct the_node *post);


static int
R_attribute(int allow, int *regparm, int *restack)
{
    int attr = 0;
    char *sym = xstrdup(token.sym);
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
    free(sym);
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
    n = (struct node *)R_immediate_exp("immediate");
    *num = 1;
    for (p = n; IS(T_COMMA); p = p->next) {
        next_token(); /* skip ',' */
        p->next = (struct node *)R_immediate_exp("immediate");
        (*num)++;
    }
    LEAVE();
    return n;
}


static struct imm_node *
R_immediate_exp(const char *death)
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
        if (!IS(T_INT)) {
            expect("number");
        }
    }
    if (IS(T_INT)) {
        n->value = create_number_str(negative, token.sym);
        next_token(); /* skip NUMBER */
        LEAVE();
        return n;
    }
    if (IS(T_ID) && try_symbol_extern(token.sym)) {
        /* XXX this should be kept as an address, but we don't keep it as such...
         * for this reason, we must deal with externs later when emitting code :\
         * if we ever decide to treat it as an address, care must be taken during
         * constant folding phase: local and extern addresses cannot be mixed etc.
         */
        n->value = xstrdup(token.sym);
        next_token(); /* skip ID */
        LEAVE();
        return n;
    }
    expect(death);
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
    struct lval_node *n;
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
        if (!attr && !deref) {
            LEAVE();
            return NULL;
        }
        expect("identifier");
    }
    n = alloc_lval_node();
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
    if (IS(T_ID) && peek_token() == T_OPENBRACE) {
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
    } else if (attr) {
        expect("function call");
    } else if (IS(T_OPENBRACE)) {
        next_token(); /* skip '(' */
        n = (struct node *)R_or_exp(the_node);
        if (!IS(T_CLOSEBRACE)) {
            expect("')'");
        }
        next_token(); /* skip ')' */
    } else {
        /* XXX consume any extern now, to prevent constant folding */
        n = (struct node *)R_lvalue_exp();
        if (!n) {
            n = (struct node *)R_immediate_exp("expression");
        }
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


static BOOL
R_jump_stat(struct the_node *the_node, struct the_node *ante, struct the_node *post)
{
    ENTER();
    if (IS(T_K_BREAK)) {
        if (!post) {
            die("'break' not in loop\n");
        }
        next_token(); /* skip 'break' */
        the_node->jump = new_name("ops");
        post->labels = new_label(post->labels, the_node->jump);
    } else if (IS(T_K_CONTINUE)) {
        if (!ante) {
            die("'continue' not in loop\n");
        }
        next_token(); /* skip 'continue' */
        the_node->jump = new_name("ops");
        ante->labels = new_label(ante->labels, the_node->jump);
    } else if (IS(T_K_GOTO)) {
        next_token(); /* skip 'goto' */
        if (!IS(T_ID)) {
            expect("label");
        }
        the_node->jump = xstrdup(token.sym);
        next_token(); /* skip label */
    } else {
        LEAVE();
        return FALSE;
    }
    if (!IS(T_SEMICOLON)) {
        expect(";");
    }
    next_token(); /* skip ';' */
    LEAVE();
    return TRUE;
}


static struct node *
R_conditional_exp(struct the_node *the_node, enum cond_t *cond)
{
    struct node *n = NULL;
    ENTER();
    *cond = COND_NE;
    if (IS(T_LOGICNOT)) {
        *cond = COND_EQ;
        next_token(); /* skip '!' */
    }
    if (!IS(T_OPENBRACE)) {
        expect("'('");
    }
    next_token(); /* skip '(' */
    if (IS(T_INT) && !strcmp(token.sym, "1")) {
        next_token(); /* skip 'true' */
    } else if (IS(T_INT) && !strcmp(token.sym, "0")) {
        *cond = COND_FLIP(*cond);
        next_token(); /* skip 'false' */
    } else {
        n = R_assignment_exp(the_node);
    }
#ifdef DATA_DEBUG
    walk_nodes(n, 0);
#endif
    if (!IS(T_CLOSEBRACE)) {
        expect("')'");
    }
    next_token(); /* skip ')' */
    LEAVE();
    return n;
}


static void
R_selection_stat_if(struct the_node *the_node, struct the_node *ante, struct the_node *post)
{
    struct the_node *body;
    struct node *n;
    char *label = NULL;
    enum cond_t cond;
    ENTER();
    next_token(); /* skip 'if' */
    n = R_conditional_exp(the_node, &cond);
    body = create_the_node();
    R_stat(body, ante, post);
    if (IS(T_K_ELSE)) {
        struct the_node *last;
        struct the_node *elze;
        struct the_node *skip = create_the_node();
        skip->lineno = -1; /* XXX silence this node */
        skip->jump = new_name("ops");
        append_list(body, skip);
        next_token(); /* skip 'else' */
        elze = create_the_node();
        R_stat(elze, ante, post);
        if (n || cond == COND_EQ) {
            label = new_name("ops");
            elze->labels = new_label(elze->labels, label);
        }
        last = create_the_node();
        last->labels = new_label(NULL, skip->jump);
        append_list(skip, elze);
        append_list(elze, last);
    } else {
        if (!optimize_jmp) {
            /* XXX hack alert: if the first instruction is a "goto", don't do the inversion + skip over
             * this is to maintain compatibility with old code generation when 'optimize_jmp' is not enabled
             */
            struct the_node *next;
            for (next = body; next && next->cond == COND_AL && !next->labels && !next->code; next = next->next) {
                if (next->jump) {
                    if (n || cond == COND_NE) {
                        label = next->jump;
                    } else {
                        free(next->jump);
                    }
                    next->jump = NULL;
                    goto okay;
                }
            }
        }
        if (n || cond == COND_EQ) {
            struct the_node *skip = create_the_node();
            label = new_name("ops");
            skip->labels = new_label(NULL, label);
            append_list(body, skip);
        }
    }
    cond = COND_FLIP(cond);
  okay:
    append_list(the_node, body);
    the_node->code = n;
    the_node->cond = n ? cond : COND_AL;
    the_node->jump = label;
    LEAVE();
}


static void
R_selection_stat_do(struct the_node *the_node)
{
    struct the_node *body = the_node;
    struct the_node *eval;
    struct the_node *skip;
    struct node *n;
    char *label = NULL;
    enum cond_t cond;
    ENTER();
    next_token(); /* skip 'do' */
    eval = create_the_node();
    skip = create_the_node();
    R_stat(body, eval, skip);
    if (!IS(T_K_WHILE)) {
        expect("while");
    }
    next_token(); /* skip 'while' */
    eval->lineno = token.lineno; /* XXX fix error reporting */
    n = R_conditional_exp(eval, &cond);
    if (!IS(T_SEMICOLON)) {
        expect(";");
    }
    next_token(); /* skip ';' */
    append_list(eval, skip);
    append_list(body, eval);
    if (n || cond != COND_EQ) {
        label = new_name("ops");
        body->labels = new_label(body->labels, label);
    }
    eval->code = n;
    eval->cond = n ? cond : COND_AL;
    eval->jump = label;
    LEAVE();
}


static void
R_selection_stat_while(struct the_node *the_node)
{
    struct the_node *body;
    struct the_node *skip;
    struct node *n;
    char *label = NULL;
    enum cond_t cond;
    ENTER();
    next_token(); /* skip 'while' */
    n = R_conditional_exp(the_node, &cond);
    body = create_the_node();
    skip = create_the_node();
    R_stat(body, the_node, skip);
    if (n || cond == COND_NE) {
        struct the_node *back = create_the_node();
        back->lineno = -1; /* XXX silence this node */
        back->jump = new_name("ops");
        the_node->labels = new_label(the_node->labels, back->jump);
        append_list(body, back);
    }
    if (n || cond == COND_EQ) {
        label = new_name("ops");
        skip->labels = new_label(skip->labels, label);
        append_list(body, skip);
    } else if (skip->labels) {
        append_list(body, skip);
    }
    cond = COND_FLIP(cond);
    append_list(the_node, body);
    the_node->code = n;
    the_node->cond = n ? cond : COND_AL;
    the_node->jump = label;
    LEAVE();
}


static BOOL
R_selection_stat(struct the_node *the_node, struct the_node *ante, struct the_node *post)
{
    int match = TRUE;
    ENTER();
    if (IS(T_K_IF)) {
        R_selection_stat_if(the_node, ante, post);
    } else if (IS(T_K_DO)) {
        R_selection_stat_do(the_node);
    } else if (IS(T_K_WHILE)) {
        R_selection_stat_while(the_node);
    } else {
        match = FALSE;
    }
    LEAVE();
    return match;
}


static void
R_external_decl(void)
{
    char *import;
    int attr, regparm = -1, restack = -1;
    unsigned long long val = -1;
    ENTER();
    next_token(); /* skip 'extern' */
    if (!IS(T_ID)) {
        expect("identifier");
    }
    import = xstrdup(token.sym);
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
    free(import);
    LEAVE();
}


static struct the_node *
R_stat_or_decl_list(enum TOKTYPE terminator, struct the_node *ante, struct the_node *post)
{
    struct the_node *new_node;
    struct the_node *the_node = NULL;
    ENTER();
    while (!IS(terminator)) {
        if (IS(T_K_EXTERN)) {
            R_external_decl();
            if (!IS(T_SEMICOLON)) {
                expect(";");
            }
            next_token(); /* skip ';' */
            continue;
        }
        new_node = create_the_node();
        R_stat(new_node, ante, post);
        the_node = append_list(the_node, new_node);
    }
    LEAVE();
    return the_node;
}


static void
R_labeled_stat(struct the_node *the_node)
{
    ENTER();
    while (IS(T_ID) && peek_token() == T_COLON) {
        the_node->labels = new_label(the_node->labels, token.sym);
        next_token(); /* skip ID */
        next_token(); /* skip ':' */
    }
    LEAVE();
}


static void
R_stat(struct the_node *the_node, struct the_node *ante, struct the_node *post)
{
    struct node *n;
    ENTER();
    R_labeled_stat(the_node);
    if (IS(T_SEMICOLON)) {
        next_token(); /* skip ';' */
        LEAVE();
        return;
    }
    if (IS(T_OPENCURLY)) {
        struct the_node *new_node;
        next_token(); /* skip '{' */
        new_node = R_stat_or_decl_list(T_CLOSECURLY, ante, post);
        append_list(the_node, new_node);
        if (!IS(T_CLOSECURLY)) {
            expect("}");
        }
        next_token(); /* skip '}' */
        LEAVE();
        return;
    }
    if (R_jump_stat(the_node, ante, post)) {
        LEAVE();
        return;
    }
    if (R_selection_stat(the_node, ante, post)) {
        LEAVE();
        return;
    }
    n = R_assignment_exp(the_node);
#ifdef DATA_DEBUG
    walk_nodes(n, 0);
#endif
    the_node->code = n;
    if (!IS(T_SEMICOLON)) {
        expect(";");
    }
    next_token(); /* skip ';' */
    LEAVE();
}


struct the_node *
parse(void)
{
    next_token(); /* pre-prime the first token */
    return R_stat_or_decl_list(T_EOI, NULL, NULL);
}
