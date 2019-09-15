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

    n->edge[0] = n->edge[1] = NULL;
    n->index = UNDEFINED;
    n->onstack = FALSE;
    n->scc = FALSE;
    return n;
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
