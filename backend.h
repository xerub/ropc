#ifndef BACKEND_H_
#define BACKEND_H_

void emit_initialize(void);
void emit_finalize(void);

void emit_load_direct(const char *value, BOOL deref0);
void emit_load_indirect(const char *lvalue, BOOL deref0);
void emit_store_indirect(const char *lvalue);
void emit_store_direct(const char *lvalue);
void emit_add(const char *value, const char *addend, int deref0, BOOL swap);
void emit_call(const char *func, char **args, int nargs, int deref0, BOOL reserve, BOOL retval, int attr);

char *emit_save(void);
void emit_restore(char *scratch);

void emit_goto(const char *label);
void emit_cond(const char *label, enum cond_t cond);
void emit_label(const char *label, BOOL used);
void emit_extern(const char *import, int attr);
void emit_fast(const char *var, const char *val);

const char *backend_name(void);

#endif
