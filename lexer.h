#ifndef LEXER_H_
#define LEXER_H_

enum TOKTYPE {
    T_COLON,       /* :                      */
    T_COMMA,       /* ,                      */
    T_OPENCURLY,   /* {                      */
    T_CLOSECURLY,  /* }                      */
    T_OPENSQUARE,  /* [                      */
    T_CLOSESQUARE, /* ]                      */
    T_OPENBRACE,   /* (                      */
    T_CLOSEBRACE,  /* )                      */
    T_ASSIGN,      /* =                      */
    T_LOGICNOT,    /* !                      */
    T_ADD,         /* +                      */
    T_SUB,         /* -                      */
    T_MUL,         /* *   alias to pointer   */
    T_DIV,         /* /                      */
    T_OR,          /* |                      */
    T_XOR,         /* ^                      */
    T_AND,         /* &   alias to address   */
    T_AT,          /* @                      */

    T_ID,          /* identifier             */
    T_STRING,      /* string constant        */
    T_INT,         /* integer constant       */

    T_K_IF,
    T_K_GOTO,
    T_K_CONST,
    T_K_EXTERN,
    T_K_VOLATILE,

    T_EOI,         /* end-of-input           */

    T_UNKNOWN
};

struct TOKEN {
    enum TOKTYPE type;
    const char *sym;
    int lineno;
    char *filename;
};

extern struct TOKEN token;

int tokenize(const char *s);
void next_token(void);
enum TOKTYPE peek_token(void);
void free_tokens(BOOL full);

#endif
