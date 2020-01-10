#ifndef CONFIG_H_
#define CONFIG_H_

#define ROPC_VERSION "1.2"
#undef LEXER_DEBUG
#undef PARSER_DEBUG
#undef CODE_DEBUG
#undef DATA_DEBUG
#define LEXER_READ_CPP
#define LEXER_STR_MERGE
#define LEXER_APOS_INT
#undef SLOW_LOAD_SAVE
#define MAX_FUNC_ARGS 16

extern int optimize_imm;
extern int optimize_add;
extern int optimize_reg;
extern int optimize_jmp;
extern int show_reg_set;
extern int nasm_esc_str;
extern int enable_cfstr;
extern int no_undefined;
extern int inloop_stack;

extern const unsigned char *binmap;
extern size_t binsz;

#endif
