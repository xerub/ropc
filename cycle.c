#include <assert.h>
#include <stdlib.h>
#include "util.h"
#include "code.h"
#include "cycle.h"


static int index;
static int S_top;
static struct the_node *S[1024];


#define S_PUSH(x) do { assert(S_top < 1024); S[S_top++] = x; (x)->onstack = TRUE; } while (0)
#define S_POP(x) do { assert(S_top); x = S[--S_top]; (x)->onstack = FALSE; } while (0)
#define S_PEEK() S[S_top - 1]


static void
strongconnect(struct the_node *v)
{
    int i;
    v->index = index;
    v->lowlink = index;
    index++;
    S_PUSH(v);
    for (i = 0; i < 2; i++) {
        struct the_node *w = v->edge[i];
        if (w) {
            if (IS_UNDEFINED(w->index)) {
                strongconnect(w);
#if 1
                if (v->lowlink > w->lowlink) {
                    v->lowlink = w->lowlink;
                }
#else
                v->lowlink = min(v->lowlink, w->lowlink);
#endif
            } else if (w->onstack) {
#if 1
                if (v->lowlink > w->index) {
                    v->lowlink = w->index;
                }
#else
                v->lowlink = min(v->lowlink, w->index);
#endif
            }
        }
    }
    if (v->lowlink == v->index) {
        struct the_node *w;
        int self = (S_PEEK() == v && v->edge[0] != v && v->edge[1] != v);
        do {
            S_POP(w);
            w->scc = !self;
        } while (w != v);
    }
}


void
mark_cycles(struct the_node *list)
{
    struct the_node *v;
    index = 0;
    S_top = 0;
    for (v = list; v; v = v->next) {
        if (IS_UNDEFINED(v->index)) {
            strongconnect(v);
        }
    }
}
