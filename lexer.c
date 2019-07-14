#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"
#include "lexer.h"


#define MAXTOK 64

#define IS_STRING(s)  (*(s) == '\"' || *(s) == '\'')

#define isalph_(c) (isalpha(c) || ((c) == '_'))
#define isalnu_(c) (isalnum(c) || ((c) == '_'))


static char **tokens = NULL;
static int ntok, itok;
struct TOKEN token;


static void *
new_token(const char *s, int len)
{
    char *p = malloc(len + 1);
    if (p) {
        memcpy(p, s, len);
        p[len] = '\0';
    }
    return p;
}


/* : , { } [ ] ( ) = ! + - * & "string" 'string' number id */
int
tokenize(const char *s)
{
    const char *last;

    if (!tokens) {
        tokens = malloc(MAXTOK * sizeof(char *));
        if (!tokens) {
            die("out of memory\n");
            return -1;
        }
        token.lineno = 0;
    }
    token.lineno++;

#ifdef LEXER_READ_CPP
    if (s[0] == '#') {
        const char *q = s + 1;
        int lineno;
        char *p;
        if (!strncmp(q, "line", 4)) {
            q += 4;
        }
        lineno = strtoul(q, &p, 10);
        if (p > q && lineno > 0) {
            int stop = ' ';
            while (isspace(*p)) {
                p++;
            }
            if (IS_STRING(p)) {
                stop = *p++;
            }
            for (q = p; *q && *q != stop; q++) {
                if (*q == '\\' && q[1]) {
                    q++;
                }
            }
            free(token.filename);
            token.filename = new_token(p, q - p);
            token.lineno = lineno - 1;
            s = q + strlen(q);
        }
    }
#endif

    for (ntok = 0; *s && *s != ';'; s++) {
        char *p;
        int len;
        if (isspace(*s)) {
            continue;
        }
        last = s;

        len = 0;
        switch (*s) {
            case ':':
            case ',':
            case '{':
            case '}':
            case '[':
            case ']':
            case '(':
            case ')':
            case '=':
            case '!':
            case '+':
            case '-':
            case '*':
            case '&':
                len = 1;
                break;
        }
        if (!len) {
            if (IS_STRING(s)) {
                int quote = *s;
                while (*++s && *s != quote) {
                    if (*s == '\\' && !*++s) {
                        goto err_syntax;
                    }
                }
                if (s[0] != quote) {
                    goto err_syntax;
                }
            } else if (isdigit(*s)) {
                strtoul(s, &p, 0);
                if (isalph_(*p)) {
                    goto err_syntax;
                }
                s = p - 1;
            } else if (isalph_(*s)) {
                while (isalnu_(s[1])) {
                    s++;
                }
            } else {
                goto err_syntax;
            }
            len = 1;
        }
        if (ntok >= MAXTOK) {
            die("too many tokens\n");
            goto err;
        }
        p = new_token(last, s + len - last);
        if (!p) {
            die("out of memory\n");
            goto err;
        }
        tokens[ntok++] = p;
        s += len - 1;
    }
    itok = 0;
    return ntok;
  err_syntax:
    die("invalid token %s\n", last);
  err:
    while (--ntok >= 0) {
        free(tokens[ntok]);
    }
    return -1;
}


void
free_tokens(BOOL full)
{
    while (--ntok >= 0) {
        free(tokens[ntok]);
    }
    if (full) {
        free(tokens);
        tokens = NULL;
        free(token.filename);
        token.filename = NULL;
    }
}


static enum TOKTYPE
eval_token(const char *s)
{
    enum TOKTYPE type;
    if (!strcmp(s, "if")) {
        type = T_K_IF;
    } else if (!strcmp(s, "goto")) {
        type = T_K_GOTO;
    } else if (!strcmp(s, "const")) {
        type = T_K_CONST;
    } else if (!strcmp(s, "extern")) {
        type = T_K_EXTERN;
    } else if (!strcmp(s, "volatile")) {
        type = T_K_VOLATILE;
    } else if (!strcmp(s, ":")) {
        type = T_COLON;
    } else if (!strcmp(s, ",")) {
        type = T_COMMA;
    } else if (!strcmp(s, "{")) {
        type = T_OPENCURLY;
    } else if (!strcmp(s, "}")) {
        type = T_CLOSECURLY;
    } else if (!strcmp(s, "[")) {
        type = T_OPENSQUARE;
    } else if (!strcmp(s, "]")) {
        type = T_CLOSESQUARE;
    } else if (!strcmp(s, "(")) {
        type = T_OPENBRACE;
    } else if (!strcmp(s, ")")) {
        type = T_CLOSEBRACE;
    } else if (!strcmp(s, "=")) {
        type = T_ASSIGN;
    } else if (!strcmp(s, "!")) {
        type = T_LOGICNOT;
    } else if (!strcmp(s, "+")) {
        type = T_ADD;
    } else if (!strcmp(s, "-")) {
        type = T_SUB;
    } else if (!strcmp(s, "*")) {
        type = T_MUL;
    } else if (!strcmp(s, "&")) {
        type = T_AND;
    } else if (IS_STRING(s)) {
        type = T_STRING;
    } else if (isdigit(*s)) {
        type = T_INT;
    } else {
        type = T_ID;
    }
    return type;
}


void
next_token(void)
{
    token.sym = NULL;
    token.type = T_EOI;
    if (itok < ntok) {
        token.sym = tokens[itok++];
        token.type = eval_token(token.sym);
#ifdef LEXER_STR_MERGE
        while (token.type == T_STRING && peek_token() == T_STRING) {
            const char *str1 = token.sym;
            const char *str2 = tokens[itok];
            size_t str1_len = strlen(str1);
            size_t str2_len = strlen(str2);
            char *buf = malloc(str1_len + str2_len - 1);
            if (!buf) {
                cry("out of memory\n");
                break;
            }
            memcpy(buf, str1, str1_len - 1);
            memcpy(buf + str1_len - 1, str2 + 1, str2_len - 2);
            buf[str1_len + str2_len - 3] = *buf;
            buf[str1_len + str2_len - 2] = '\0';
            free(tokens[itok]);
            token.sym = tokens[itok++] = buf;
        }
#endif
#ifdef LEXER_DEBUG
        printf(";// token %s <%d>\n", token.sym, token.type);
#endif
    }
}


enum TOKTYPE
peek_token(void)
{
    enum TOKTYPE type = T_EOI;
    if (itok < ntok) {
        type = eval_token(tokens[itok]);
    }
    return type;
}
