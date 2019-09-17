#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "util.h"
#include "lexer.h"
#include "parser.h"
#include "symtab.h"
#include "cycle.h"
#include "emit.h"
#include "code.h"
#include "backend.h"


struct label_node {
    struct label_node *next;
    char *label;
    int used;
};


#ifdef CODE_DEBUG
#include <stdio.h>


static void
print_ops(struct the_node *list)
{
    struct the_node *n;
    for (n = list; n; n = n->next) {
        struct label_node *l;
        for (l = n->labels; l; l = l->next) {
            printf("%s: [%d]\n", l->label, l->used);
        }
        printf("%c   ", n->reachable ? 'x' : ' ');
        if (n->cond != COND_AL) {
            printf("if %scond goto %s", (n->cond == COND_EQ) ? "!" : "", n->jump);
        } else if (n->jump) {
            printf("goto %s", n->jump);
        } else if (n->code) {
            printf("code");
        } else {
            printf("<empty>");
        }
        printf("\n");
    }
}
#endif


struct label_node *
new_label(struct label_node *top, const char *label)
{
    struct label_node *n = xmalloc(sizeof(struct label_node));
    n->label = xstrdup(label);
    n->next = top;
    n->used = 0;
    return n;
}


struct the_node *
create_the_node(void)
{
    struct the_node *n = xmalloc(sizeof(struct the_node));
    n->next = NULL;

    n->labels = NULL;
    n->code = NULL;
    n->jump = NULL;
    n->cond = COND_AL;
    n->attr = 0;

    n->lineno = token.lineno;
    n->filename = strdup(token.filename);
    n->reachable = FALSE;

    n->edge[0] = n->edge[1] = NULL;
    n->index = UNDEFINED;
    n->onstack = FALSE;
    n->scc = FALSE;
    return n;
}


static void
delete_the_node(struct the_node *n)
{
    struct label_node *l;
    for (l = n->labels; l; ) {
        struct label_node *q = l->next;
        free(l->label);
        free(l);
        l = q;
    }
    free_nodes(n->code);
    free(n->jump);
    free(n->filename);
    free(n);
}


static void
move_labels(struct the_node *from, struct the_node *to)
{
    if (from->labels) {
        struct label_node *l = from->labels;
        while (l->next) {
            l = l->next;
        }
        l->next = to->labels;
        to->labels = from->labels;
        from->labels = NULL;
    }
}


static void
merge_labels(struct the_node *list)
{
    struct the_node *n;
    for (n = list; n; n = n->next) {
        if (n->code == NULL && n->jump == NULL && n->next) {
            move_labels(n, n->next);
        }
    }
}


static void
ref_label(struct the_node *n, const char *label)
{
    if (n) {
        struct label_node *l;
        for (l = n->labels; l; l = l->next) {
            if (!strcmp(label, l->label)) {
                l->used++;
                break;
            }
        }
    }
}


static void
unref_label(struct the_node *n, const char *label)
{
    if (n) {
        struct label_node *l, *lp = NULL;
        for (l = n->labels; l; lp = l, l = l->next) {
            if (!strcmp(label, l->label)) {
                l->used--;
                if (!l->used) {
                    if (lp) {
                        lp->next = l->next;
                    } else {
                        n->labels = l->next;
                    }
                    free(l->label);
                    free(l);
                }
                break;
            }
        }
    }
}


static void
update_edges(struct the_node *head, struct the_node *from, struct the_node *to)
{
    /* XXX wow, this is god damn slow */
    while (head) {
        if (head->edge[0] == from) {
            head->edge[0] = to;
        }
        if (head->edge[1] == from) {
            head->edge[1] = to;
        }
        head = head->next;
    }
}


static void
mark_reachables(struct the_node *list)
{
    struct the_node *n = list;
    while (n && !n->reachable) {
        n->reachable = TRUE;
        mark_reachables(n->edge[1]);
        n = n->edge[0];
    }
}


static void
unmark_reachables(struct the_node *list)
{
    struct the_node *n = list;
    for (n = list; n; n = n->next) {
        n->reachable = FALSE;
    }
}


static struct the_node *
prune_nodes(struct the_node *list)
{
    int pass;
    BOOL dirty = TRUE;
    int save_lineno = token.lineno;
    char *save_filename = token.filename;
    struct the_node *n, *next, *prev;
    for (pass = 0; pass < 10 && dirty; pass++) {
        dirty = FALSE;
        mark_reachables(list);
#ifdef CODE_DEBUG
        printf("--- pass %d init ---------------\n", pass);
        print_ops(list);
#endif
        // prune empty and dead nodes
        for (prev = NULL, n = list; n; n = next) {
            next = n->next;
            assert(n->jump || n->cond == COND_AL);
            if (n->jump || n->code || n->labels) {
                if (n->reachable) {
                    prev = n;
                    continue;
                }
                if (pass == 0 && n->lineno > 0) {
                    token.lineno = n->lineno;
                    token.filename = n->filename;
                    cry("unreachable code\n");
                }
            }
            dirty = TRUE;
            if (prev) {
                prev->next = next;
            } else {
                list = next;
            }
            if (n->jump) {
                unref_label(n->edge[n->cond != COND_AL], n->jump);
            }
            update_edges(list, n, n->edge[0]);
            delete_the_node(n);
        }
        // follow goto chains
        for (n = list; n; n = n->next) {
            if (n->jump) {
                struct the_node *other = n->edge[n->cond != COND_AL];
                unmark_reachables(list);
                n->reachable = TRUE;
                next = NULL;
                while (other && !other->reachable && other->jump && other->cond == COND_AL) {
                    next = other;
                    other->reachable = TRUE;
                    other = other->edge[0];
                }
                if (next) {
                    dirty = TRUE;
                    unref_label(n->edge[n->cond != COND_AL], n->jump);
                    free(n->jump);
                    n->jump = xstrdup(next->jump);
                    n->edge[n->cond != COND_AL] = next->edge[0];
                    ref_label(next->edge[0], n->jump);
                }
            }
        }
        // convert "if (COND) goto one; goto two; one:" => "if !(COND) goto two;"
        for (n = list; n; n = n->next) {
            next = n->next;
            if (!n->jump || n->cond == COND_AL || !next || !next->jump || next->cond != COND_AL || next->labels || !next->next || n->edge[1] != next->next) {
                continue;
            }
            assert(n->code && !next->code);
            dirty = TRUE;
            unref_label(next->next, n->jump);
            free(n->jump);
            n->jump = next->jump;
            n->edge[0] = next->next;
            n->edge[1] = next->edge[0];
            n->next = next->next;
            n->cond = COND_FLIP(n->cond);
            next->jump = NULL;
            delete_the_node(next);
        }
        // convert "[if (COND)] goto one; one:" => "[COND;] one:"
        for (prev = NULL, n = list; n; n = next) {
            next = n->next;
            if (!n->jump || !next || n->edge[n->cond != COND_AL] != next) {
                prev = n;
                continue;
            }
            dirty = TRUE;
            unref_label(next, n->jump);
            free(n->jump);
            n->jump = NULL;
            if (n->code) {
                n->cond = COND_AL;
                n->edge[0] = next;
                n->edge[1] = NULL;
                continue;
            }
            if (prev) {
                prev->next = next;
            } else {
                list = next;
            }
            move_labels(n, next);
            update_edges(list, n, next);
            delete_the_node(n);
        }
        // convert "if (COND) goto one; goto one" => "COND; goto one;"
        for (n = list; n; n = n->next) {
            next = n->next;
            if (!n->jump || n->cond == COND_AL || !next || !next->jump || next->cond != COND_AL || strcmp(n->jump, next->jump)) {
                continue;
            }
            assert(n->edge[1] == next->edge[0]);
            dirty = TRUE;
            unref_label(n->edge[1], n->jump);
            free(n->jump);
            n->jump = NULL;
            n->cond = COND_AL;
        }
        unmark_reachables(list);
#ifdef CODE_DEBUG
        printf("--- pass %d done ---------------\n", pass);
        print_ops(list);
#endif
    }
#ifdef CODE_DEBUG
    printf("-------------------------------\n");
#endif
    token.lineno = save_lineno;
    token.filename = save_filename;
    return list;
}


static struct the_node *
find_node_with_label(struct the_node *head, const char *label)
{
    /* XXX wow, this is god damn slow */
    while (head) {
        struct label_node *l;
        for (l = head->labels; l; l = l->next) {
            if (!strcmp(l->label, label)) {
                l->used++;
                return head;
            }
        }
        head = head->next;
    }
    return NULL;
}


static void
link_graph(struct the_node *list)
{
    /* XXX check labels using symtab */
    struct the_node *n;
    for (n = list; n; n = n->next) {
        if (n->attr & ATTRIB_NORETURN) {
            continue;
        }
        n->edge[0] = n->next;
        if (n->cond != COND_AL) {
            n->edge[1] = find_node_with_label(list, n->jump);
        } else if (n->jump) {
            n->edge[0] = find_node_with_label(list, n->jump);
        }
    }
}


void
emit_code(struct the_node *list)
{
    struct the_node *n = list;
    if (optimize_jmp) {
        merge_labels(n);
    }
    link_graph(n);
    if (optimize_jmp) {
        n = prune_nodes(n);
    }
    mark_cycles(n);
    emit_initialize();
    while (n) {
        struct label_node *l;
        struct the_node *p = n->next;
        token.lineno = n->lineno;
        free(token.filename);
        token.filename = n->filename;
        for (l = n->labels; l; ) {
            struct label_node *q = l->next;
            emit_label(l->label, l->used, l->next == NULL);
            free(l->label);
            free(l);
            l = q;
        }
        if (n->cond != COND_AL) {
            n->code = reverse_list(n->code);
            emit_nodes(n->code, NULL, TRUE, n->scc);
            emit_cond(n->jump, n->cond);
            free_nodes(n->code);
            free(n->jump);
        } else if (n->jump) {
            emit_goto(n->jump);
            free(n->jump);
        } else if (n->code) {
            n->code = reverse_list(n->code);
            emit_nodes(n->code, NULL, FALSE, n->scc);
            free_nodes(n->code);
        }
        free(n);
        n = p;
    }
    emit_finalize();
}
