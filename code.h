#ifndef CODE_H_
#define CODE_H_

#define UNDEFINED -1
#define IS_UNDEFINED(x) ((x) < 0)

enum cond_t {
    COND_AL, /* always */
    COND_NE, /* Z==0 */
    COND_EQ  /* Z==1 */
};

struct the_node {
    struct the_node *next;

    /* filled in by parser */
    struct label_node *labels; /* list of labels */
    struct node *code;         /* code to emit */
    char *jump;                /* goto target */
    enum cond_t cond;          /* evaluate code and jump */
    int attr;                  /* maybe ATTRIB_NORETURN */
    int lineno;                /* line number, for error reporting */
    char *filename;            /* file name, for error reporting */

    struct the_node *edge[2]; /* edge[0] next in execution flow (may be NULL if this node is a noreturn). edge[1] next in conditional flow (usually NULL if this node is not conditional) */

    int index, lowlink, onstack; /* tarjan */
    BOOL scc;
};

struct label_node *new_label(struct label_node *top, const char *label);
struct the_node *create_the_node(void);

void emit_code(struct the_node *list);

#endif
