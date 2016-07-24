/*
 *  ROP Compiler
 *
 *  Copyright (c) 2012 xerub
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "config.h"
#include "util.h"
#include "lexer.h"
#include "parser.h"
#include "code.h"
#include "symtab.h"
#include "backend.h"


int optimize_imm = 0;
int optimize_add = 0;
int optimize_reg = 0;
int optimize_jmp = 0;
int show_reg_set = 0;
int inloop_stack = 256;

static const char *binary = NULL;
const unsigned char *binmap = NULL;


static int
check_args(int argc, char **argv)
{
    int i;
    const char *p;
    for (i = 1; i < argc && *(p = argv[i]) == '-'; i++) {
        const char *q = p + 2;
        if (!strcmp(p, "-h")) {
            printf("usage: %s [-O2] [-O{i|a|r|j}] [-mregparm=N] [-mrestack=S] [-g] [-c cache] file\n", argv[0]);
            exit(0);
        }
        if (!strcmp(p, "-V")) {
            printf("ropc %s v" ROPC_VERSION "\n", backend_name());
            exit(0);
        }
        switch (p[1]) {
            case 'O':
                if (*q == '2') {
                    optimize_imm = 1;
                    optimize_add = 1;
                    optimize_reg = 1;
                    optimize_jmp = 1;
                    q++;
                    break;
                }
                do {
                    switch (*q) {
                        case 'i': optimize_imm = 1; continue;
                        case 'a': optimize_add = 1; continue;
                        case 'r': optimize_reg = 1; continue;
                        case 'j': optimize_jmp = 1; continue;
                    }
                    errx(1, "unrecognized option '%s'", p);
                } while (*++q);
                break;
            case 'g':
                show_reg_set = 1;
                break;
            case 'c':
                if (++i >= argc) {
                    errx(1, "argument to '%s' is missing", p);
                }
                binary = argv[i];
                break;
            case 'm':
                if (!strncmp(q, "regparm=", 8)) {
                    arch_regparm = strtoul(q + 8, (char **)&q, 10);
                    break;
                }
                if (!strncmp(q, "restack=", 8)) {
                    inloop_stack = strtoul(q + 8, (char **)&q, 10);
                    break;
                }
            default:
                q = p;
        }
        if (*q) {
            errx(1, "unrecognized option '%s'", p);
        }
    }
    return i;
}


int
main(int argc, char **argv)
{
    FILE *f;
    char buf[BUFSIZ];
    const char *filename = NULL;
    struct the_node *list = NULL;

    int i = check_args(argc, argv);

    int fd = -1;
    long sz = 0;
    if (binary) {
        fd = open(binary, O_RDONLY);
        if (fd < 0) {
            err(1, "%s", filename);
        }

        sz = lseek(fd, 0, SEEK_END);

        binmap = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
        if (binmap == MAP_FAILED) {
            err(1, "%s", filename);
        }
    }

    if (argc > i) {
        filename = argv[i];
        token.filename = strdup(filename);
        f = fopen(filename, "rt");
        if (!f) {
            err(1, "%s", filename);
        }
    } else {
        token.filename = strdup("<stdin>");
        f = stdin;
    }

    while (fgets(buf, sizeof(buf), f)) {
        struct the_node *n = parse(buf);
        if (n) {
            n->next = list;
            list = n;
        }
    }
    emit_code(list);
    free_symbols();
    free_tokens(TRUE);

    if (filename) {
        fclose(f);
    }

    munmap((char *)binmap, sz);
    close(fd);
    return 0;
}
