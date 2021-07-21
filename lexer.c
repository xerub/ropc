#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"
#include "lexer.h"


#define MAXTOK 128

#define IS_STRING(s)  (*(s) == '\"' || *(s) == '\'')

#define isalph_(c) (isalpha(c) || ((c) == '_'))
#define isalnu_(c) (isalnum(c) || ((c) == '_'))


static FILE *source;
static char *saved = NULL;
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

    for (ntok = 0; *s; s++) {
        char *p;
        if (*s == '/' && s[1] == '/') {
            break;
        }
        if (isspace(*s)) {
            continue;
        }
        last = s;

        switch (*s) {
            case ':':
            case ';':
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
            case '/':
            case '|':
            case '^':
            case '&':
            case '@':
                break;
            default:
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
        }
        if (ntok >= MAXTOK) {
            die("too many tokens\n");
            goto err;
        }
        p = new_token(last, s + 1 - last);
        if (!p) {
            die("out of memory\n");
            goto err;
        }
        tokens[ntok++] = p;
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
        free(saved);
        saved = NULL;
        free(tokens);
        tokens = NULL;
        free(token.filename);
        token.filename = NULL;
    }
}


void
init_tokens(void *p)
{
    source = p;
}


static void
fetch_tokens(void)
{
    char buf[BUFSIZ];
    if (!source || itok < ntok) {
        return;
    }
    if (token.sym && (!saved || strcmp(token.sym, saved))) {
        token.sym = strdup(token.sym);
        if (!token.sym) {
            die("out of memory\n");
        }
        free(saved);
        saved = (char *)token.sym;
    }
    free_tokens(FALSE);
    while (fgets(buf, sizeof(buf), source) && tokenize(buf) == 0) {
    }
}


static enum TOKTYPE
eval_token(const char *s)
{
    enum TOKTYPE type;
    if (!strcmp(s, "if")) {
        type = T_K_IF;
    } else if (!strcmp(s, "else")) {
        type = T_K_ELSE;
    } else if (!strcmp(s, "do")) {
        type = T_K_DO;
    } else if (!strcmp(s, "while")) {
        type = T_K_WHILE;
    } else if (!strcmp(s, "break")) {
        type = T_K_BREAK;
    } else if (!strcmp(s, "continue")) {
        type = T_K_CONTINUE;
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
    } else if (!strcmp(s, ";")) {
        type = T_SEMICOLON;
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
    } else if (!strcmp(s, "/")) {
        type = T_DIV;
    } else if (!strcmp(s, "|")) {
        type = T_OR;
    } else if (!strcmp(s, "^")) {
        type = T_XOR;
    } else if (!strcmp(s, "&")) {
        type = T_AND;
    } else if (!strcmp(s, "@")) {
        type = T_AT;
#ifdef LEXER_APOS_INT
    } else if (*s == '\'') {
        type = T_INT;
#endif
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
    fetch_tokens();
    if (itok < ntok) {
        token.sym = tokens[itok++];
        token.type = eval_token(token.sym);
#ifdef LEXER_APOS_INT
        if (token.type == T_INT && token.sym[0] == '\'') {
            char buf[32];
            unsigned i, j;
            const char *s = token.sym;
            unsigned long long val = 0;
            for (i = 1, j = 0; s[i] != '\''; i++, j++) {
                int ch = s[i];
                if (ch == '\\') {
                    switch (s[++i]) {
                        case '0': ch = 0; break;
                        case 'n': ch = '\n'; break;
                        case 't': ch = '\t'; break;
                        case '\\': break;
                        default:
                            j = 8;
                    }
                }
                val = (val << 8) | ch;
            }
            if (j == 0 || j > 8) {
                die("bad multichar: %s\n", token.sym);
            }
            sprintf(buf, "0x%llx", val);
            token.sym = strdup(buf);
            if (!token.sym) {
                die("out of memory\n");
            }
            free(tokens[--itok]);
            tokens[itok++] = (char *)token.sym;
        }
#endif
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
    fetch_tokens();
    if (itok < ntok) {
        type = eval_token(tokens[itok]);
    }
    return type;
}
