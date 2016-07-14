#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"
#include "emit.h"
#include "code.h"
#include "symtab.h"
#include "backend.h"


struct call_node *
alloc_call_node(void)
{
    struct call_node *n = xmalloc(sizeof(struct call_node));
    n->next = NULL;
    n->type = NODE_CALL;
    return n;
}


struct imm_node *
alloc_imm_node(void)
{
    struct imm_node *n = xmalloc(sizeof(struct imm_node));
    n->next = NULL;
    n->type = NODE_IMM;
    return n;
}


struct lval_node *
alloc_lval_node(void)
{
    struct lval_node *n = xmalloc(sizeof(struct lval_node));
    n->next = NULL;
    n->type = NODE_LVAL;
    return n;
}


struct add_node *
alloc_add_node(void)
{
    struct add_node *n = xmalloc(sizeof(struct add_node));
    n->next = NULL;
    n->type = NODE_ADD;
    return n;
}


void *
reverse_list(void *n)
{
    struct node *prev = NULL;
    struct node *node = n;
    while (node) {
        struct node *next = node->next;
        node->next = prev;
        prev = node;
        node = next;
    }
    return prev;
}


void
free_nodes(struct node *n)
{
    while (n) {
        struct node *next = n->next;
        switch (n->type) {
            case NODE_IMM: {
                struct imm_node *p = (struct imm_node *)n;
                free(p->value);
                break;
            }
            case NODE_LVAL: {
                struct lval_node *p = (struct lval_node *)n;
                free(p->name);
                break;
            }
            case NODE_CALL: {
                struct call_node *p = (struct call_node *)n;
                free(p->func);
                if (p->parm) {
                    free_nodes(p->parm);
                }
                break;
            }
            case NODE_ADD: {
                struct add_node *p = (struct add_node *)n;
                if (p->list) {
                    free_nodes(p->list);
                }
                break;
            }
        }
        free(n);
        n = next;
    }
}


#ifdef DATA_DEBUG
#include <stdio.h>


#define show(args...) do { if (level) printf("%*c", level * 2, ' '); printf(args); } while (0)


void
walk_nodes(struct node *n, int level)
{
    while (n) {
        switch (n->type) {
            case NODE_IMM: {
                struct imm_node *p = (struct imm_node *)n;
                show(";== imm %s\n", p->value);
                break;
            }
            case NODE_LVAL: {
                struct lval_node *p = (struct lval_node *)n;
                show(";== lval %s%s\n", p->deref ? "*" : "", p->name);
                break;
            }
            case NODE_CALL: {
                struct call_node *p = (struct call_node *)n;
                show(";== %s(\n", p->func);
                if (p->parm) {
                    walk_nodes(p->parm, level + 1);
                }
                show(")\n");
                break;
            }
            case NODE_ADD: {
                struct add_node *p = (struct add_node *)n;
                if (p->list) {
                    show(";== +(\n");
                    walk_nodes(p->list, level + 1);
                    show(")\n");
                }
                break;
            }
        }
        n = n->next;
    }
}
#endif


static BOOL
is_loadable_sym(const char *name)
{
    enum use_t u = get_symbol_used(name);
    if (u != UNUSED) {
        if (u == CLOBBERED) {
            // XXX gets reported twice
            cry("symbol '%s' should be volatile\n", name);
        }
        return FALSE;
    }
    if (try_symbol_attr(name) & ATTRIB_VOLATILE) {
        return FALSE;
    }
    return TRUE;
}


static int
is_loadable(struct node *n, BOOL allow_deref)
{
    struct lval_node *p = (struct lval_node *)n;
    if (!is_loadable_sym(p->name)) {
        return 0;
    }
    if (p->deref && !allow_deref) {
        return 0;
    }
    if (!p->deref && try_symbol_extern(p->name)) {
        cry("symbol '%s' is actually an address\n", p->name);
        return -1;
    }
    return 1;
}


static void
maybe_symbol_forward(const char *arg)
{
    char *tmp = copy_address_sym(arg);
    if (tmp) {
        add_symbol_forward(tmp, 0);
        if (try_symbol_extern(tmp)) {
            die("symbol '%s' is actually an address\n", tmp);
        }
        if (try_symbol_attr(tmp) & ATTRIB_CONSTANT) {
            cry("taking address of constant variable '%s'\n", tmp);
        }
        free(tmp);
    }
}


void
emit_nodes(struct node *n, const char *assignto, BOOL force, BOOL inloop)
{
    char *fast = NULL;

    assert(n);

    if (n->next == NULL && n->type != NODE_CALL && !assignto && !force) {
        /* do not emit single node, unless it is a call or we really need it */
        cry("statement with no effect\n");
        return;
    }
    switch (n->type) {
        case NODE_IMM: {
            fast = AS_IMM(n)->value;
            maybe_symbol_forward(fast);
            if (force) {
                cry("constant expression '%s' in conditional\n", fast);
            }
            assert(!assignto);
            break;
        }
        case NODE_LVAL: {
            struct lval_node *p = AS_LVAL(n);
            int loadable = is_loadable(n, TRUE);
            if (loadable < 0) {
                fast = p->name;
            } else if (loadable > 0) {
                make_symbol_used(p->name);
                emit_load_direct(p->name, p->deref);
            } else {
                emit_load_indirect(p->name, p->deref);
            }
            break;
        }
        case NODE_CALL: {
            struct call_node *p = AS_CALL(n);
            struct node *parm;
            int deref0 = 0;
            char *args[MAX_FUNC_ARGS];
            char *func;
            int i;
            BOOL retval = (n->next != NULL) || force || assignto;
            BOOL direct = FALSE;
            for (i = 0, parm = p->parm; parm; parm = parm->next, i++) {
                BOOL r0 = (i == 0 && arch_regparm);
                assert(i < MAX_FUNC_ARGS);
                if (parm->type == NODE_IMM) {
                    args[i] = xstrdup(AS_IMM(parm)->value);
                    maybe_symbol_forward(args[i]);
                } else if (parm->type == NODE_LVAL && is_loadable(parm, r0)) {
                    struct lval_node *q = AS_LVAL(parm);
                    args[i] = xstrdup(q->name);
                    make_symbol_used(args[i]);
                    if (q->deref) {
                        deref0 = 1;
                    }
                } else if (r0 && parm->next == NULL) {
                    args[i] = NULL;
                    direct = TRUE;
                    emit_nodes(parm, NULL, TRUE, inloop);
                } else if (r0 && parm->type == NODE_LVAL) {
                    struct lval_node *q = AS_LVAL(parm);
                    args[i] = create_address_str(q->name);
                    deref0 = 1;
                    if (q->deref) {
                        deref0++;
                    }
                } else {
                    args[i] = new_name("var");
                    emit_nodes(parm, args[i], FALSE, inloop);
                    make_symbol_used(args[i]);
                }
            }
            func = p->func;
            if (retval && (p->attr & ATTRIB_NORETURN)) {
                cry("function '%s' does not return\n", func);
            }
            if (!is_loadable_sym(func)) {
                char *ptr = new_name("ptr");
                emit_load_indirect(func, FALSE);
                add_symbol_forward(ptr, 0);
                emit_store_indirect(ptr);
                func = ptr;
            } else {
                func = xstrdup(func);
            }
            make_symbol_used(func);
            if (!(p->attr & ATTRIB_NORETURN)) {
                if ((p->attr & ATTRIB_STACK) || inloop) {
                    if (!(p->attr & ATTRIB_STACK)) {
                        cry("reserved [[stack]] for '%s' because of loop\n", func);
                    }
                    mark_all_used(PROTECTED);
                } else {
                    mark_all_used(CLOBBERED);
                }
            }
            if (direct) {
                emit_call(func, NULL, 0, deref0, inloop, retval, p->attr);
            } else {
                emit_call(func, args, i, deref0, inloop, retval, p->attr);
            }
            free(func);
            while (--i >= 0) {
                free(args[i]);
            }
            break;
        }
        case NODE_ADD: {
            struct node *term;
            struct node *prev;
            int deref0 = 0;
            char *prev_tmp;
            prev = AS_ADD(n)->list;
            if (prev->type == NODE_IMM) {
                prev_tmp = xstrdup(AS_IMM(prev)->value);
                maybe_symbol_forward(prev_tmp);
            } else if (prev->type == NODE_LVAL && is_loadable(prev, TRUE)) {
                prev_tmp = xstrdup(AS_LVAL(prev)->name);
                make_symbol_used(prev_tmp);
                if (AS_LVAL(prev)->deref) {
                    deref0 = TRUE;
                }
            } else if (prev->type == NODE_LVAL) {
                prev_tmp = create_address_str(AS_LVAL(prev)->name);
                deref0 = 1;
                if (AS_LVAL(prev)->deref) {
                    deref0++;
                }
            } else {
                prev_tmp = new_name("var");
                emit_nodes(prev, prev_tmp, FALSE, inloop);
                make_symbol_used(prev_tmp);
            }
            for (term = prev->next; term; term = term->next) {
                BOOL swap = FALSE;
                char *tmp;
                char *sum = new_name("sum");
                if (term->type == NODE_IMM) {
                    tmp = xstrdup(AS_IMM(term)->value);
                    maybe_symbol_forward(tmp);
                } else if (term->type == NODE_LVAL && is_loadable(term, !deref0)) {
                    tmp = xstrdup(AS_LVAL(term)->name);
                    make_symbol_used(tmp);
                    if (AS_LVAL(term)->deref) {
                        swap = TRUE;
                        deref0 = 1;
                    }
                } else if (term->type == NODE_LVAL && !deref0) {
                    tmp = create_address_str(AS_LVAL(term)->name);
                    deref0 = 1;
                    if (AS_LVAL(term)->deref) {
                        swap = TRUE;
                        deref0++;
                    }
                } else {
                    tmp = new_name("var");
                    emit_nodes(term, tmp, FALSE, inloop);
                    make_symbol_used(tmp);
                }
                emit_add(prev_tmp, tmp, deref0, swap);
                deref0 = 0;
                if (term->next) {
                    emit_store_indirect(sum);
                }
                free(prev_tmp);
                prev_tmp = sum;
                free(tmp);
            }
            free(prev_tmp);
            break;
        }
    }
    if (assignto) {
        add_symbol_forward(assignto, 0);
        emit_store_indirect(assignto);
    } else {
        BOOL loaded = FALSE;
        for (n = n->next; n; n = n->next) {
            BOOL later = FALSE;
            struct lval_node *p = AS_LVAL(n);
            assert(n->type == NODE_LVAL);
            if (fast) {
                if (optimize_imm && !p->deref && !get_symbol(p->name) && ((p->attr & ATTRIB_CONSTANT) || !inloop)) {
                    emit_fast(p->name, fast);
                    add_symbol_defined(p->name, fast, p->attr);
                    continue;
                }
                if (!loaded) {
                    loaded = TRUE;
                    if (p->deref && !is_loadable_sym(p->name)) {
                        later = TRUE;
                    } else {
                        emit_load_direct(fast, FALSE);
                    }
                }
            }
            if (p->attr & ATTRIB_CONSTANT) {
                cry("useless const for '%s'\n", p->name);
            }
            if (p->deref) {
                /* XXX only addresses (imports/vectors) can be derefed */
                if (!is_loadable_sym(p->name)) {
                    /* XXX ok, this is very very shitty
                     * tip1: store value to tmp_N for each future p->deref at once
                     * tip2: calculate in advance how many derefs we will need and store pointers before calculating r0 (see above)
                     */
                    char *ptr = new_name("ptr");
                    char *tmp;
                    if (!later) {
                        tmp = emit_save();
                    }
                    emit_load_indirect(p->name, FALSE);
                    emit_store_indirect(ptr);
                    if (!later) {
                        emit_restore(tmp);
                    } else {
                        emit_load_direct(fast, FALSE);
                    }
                    add_symbol_forward(ptr, 0);
                    make_symbol_used(ptr);
                    emit_store_direct(ptr);
                    free(ptr);
                } else {
                    make_symbol_used(p->name);
                    emit_store_direct(p->name);
                }
            } else {
                add_symbol_forward(p->name, p->attr);
                if (try_symbol_extern(p->name)) {
                    die("cannot assign to import address '%s'\n", p->name);
                }
                if (optimize_imm && (try_symbol_attr(p->name) & ATTRIB_CONSTANT)) {
                    die("'%s' was declared constant\n", p->name);
                }
                emit_store_indirect(p->name);
            }
        }
        if (force && fast && !loaded) {
            emit_load_direct(fast, FALSE);
        }
    }
}
