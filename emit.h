#ifndef EMIT_H_
#define EMIT_H_

#define AS_CALL(n) ((struct call_node *)(n))
#define AS_LVAL(n) ((struct lval_node *)(n))
#define AS_IMM(n)  ((struct imm_node *)(n))
#define AS_ADD(n)  ((struct add_node *)(n))
#define AS_MUL(n)  ((struct mul_node *)(n))

enum node_type {
    NODE_IMM,
    NODE_LVAL,
    NODE_CALL,
    NODE_ADD,
    NODE_MUL
};

struct node {
    struct node *next;
    enum node_type type;
    int inverse;
};

struct call_node {
    struct node *next;
    enum node_type type;
    int inverse;
    char *func;
    struct node *parm;
    int attr, regparm;
};

struct lval_node {
    struct node *next;
    enum node_type type;
    int inverse;
    char *name;
    BOOL deref;
    int attr;
};

struct imm_node {
    struct node *next;
    enum node_type type;
    int inverse;
    char *value;
};

struct add_node {
    struct node *next;
    enum node_type type;
    int inverse;
    struct node *list;
};

struct mul_node {
    struct node *next;
    enum node_type type;
    int inverse;
    struct node *list;
};

struct call_node *alloc_call_node(void);
struct imm_node *alloc_imm_node(void);
struct lval_node *alloc_lval_node(void);
struct add_node *alloc_add_node(void);
struct mul_node *alloc_mul_node(void);
void *reverse_list(void *n);
void free_nodes(struct node *n);

void walk_nodes(struct node *n, int level);
void emit_nodes(struct node *n, const char *assignto, BOOL force, BOOL inloop);

#endif
