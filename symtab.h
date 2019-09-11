#ifndef SYMTAB_H_
#define SYMTAB_H_

#define SYMBOL_VECTOR   >0	/* number of elements */
#define SYMBOL_NORMAL   0
#define SYMBOL_STRING   -1
#define SYMBOL_EXTERN   -2
#define SYMBOL_LABEL    -3

#define ATTRIB_CONSTANT (1<<0)
#define ATTRIB_VOLATILE (1<<1)
#define ATTRIB_NORETURN (1<<2)
#define ATTRIB_STDCALL  (1<<3)
#define ATTRIB_STACK    (1<<4)
#define ATTRIB_REGPARM  (1<<5)
#define ATTRIB_UNKNOWN  (1<<31)

enum use_t {
    CLOBBERED = -1,
    UNUSED,
    USED,
    PROTECTED
};

struct SYM {
    struct SYM *next;
    int type;			/* see SYMBOL_* */
    int attr;			/* see ATTRIB_* */
    int regparm;
    int restack;
    char *key;
    char *val;
    enum use_t used;
    int idx;			/* label specific */
    unsigned long long addr;	/* extern specific */
};

const struct SYM *get_symbol(const char *key);

void add_symbol_defined(const char *key, const void *val, int attr);
void add_symbol_forward(const char *key, int attr);

enum use_t get_symbol_used(const char *key);
void make_symbol_used(const char *key);
void mark_all_used(enum use_t u);

BOOL try_symbol_extern(const char *key);
int try_symbol_attr(const char *key);
int try_symbol_regparm(const char *key);

void emit_symbols(void);
void free_symbols(void);

char *add_string(const char *arg);
char *add_vector(const char **args, int narg);
void add_extern(const char *import, unsigned long long addr, int attr, int regparm);
void add_label(const char *label, int idx);

const char *get_label_with_label(const char *target);
void set_label_with_label(const char *label, const char *target);

#endif
