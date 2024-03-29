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


int test_gadgets = 0;
int optimize_imm = 0;
int optimize_add = 0;
int optimize_reg = 0;
int optimize_jmp = 0;
int show_reg_set = 0;
int nasm_esc_str = 0;
int enable_cfstr = 0;
int no_undefined = 0;
int all_volatile = 0;
int new_name_off = 0;
int inloop_stack = 256;
extern unsigned long gadget_limit;

static const char *outfile = NULL;
static const char *binary = NULL;
const unsigned char *binmap = NULL;
size_t binsz = 0;


static int
check_args(int argc, char **argv)
{
    int i;
    const char *p;
    for (i = 1; i < argc && *(p = argv[i]) == '-'; i++) {
        const char *q = p + 2;
        if (!strcmp(p, "-h")) {
            printf("usage: %s [-O2] [-O{i|a|r|j}] [-mrestack=S] [-g] [-n] [a] [-c cache [-l limit]] [-t] [-o output] file\n"
                "    -mrestack   number of words to reserve on the stack prior to calls\n"
                "    -Oi         optimize immediate assignment\n"
                "    -Oa         optimize simple arithmetic\n"
                "    -Or         optimize register usage\n"
                "    -Oj         optimize jumps\n"
                "    -O2         all of the above\n"
                "    -o          write output to file\n"
                "    -c          file to link against: gadgets, imports\n"
                "    -l          limit the size of the file for gadgets\n"
                "    -t          test gadgets (used with -c)\n"
                "    -g          print detailed register usage\n"
                "    -n          emit NASM escaped strings: `hello\\n`\n"
                "    -a          accept Apple NSString-like constructs: @\"Hello\"\n"
                "    -fvolatile  force all unqualified vars to be volatile\n"
                "    -mgenstart  start internal name numbering\n"
                "    -u          emit undefined symbols\n"
                "    -V          print version and exit\n"
                , argv[0]);
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
            case 'n':
                nasm_esc_str = 1;
                break;
            case 'a':
                enable_cfstr = 1;
                break;
            case 'u':
                no_undefined = 1;
                break;
            case 't':
                test_gadgets++;
                break;
            case 'c':
                if (++i >= argc) {
                    errx(1, "argument to '%s' is missing", p);
                }
                binary = argv[i];
                break;
            case 'l':
                if (++i >= argc) {
                    errx(1, "argument to '%s' is missing", p);
                }
                gadget_limit = strtoul(argv[i], (char **)&q, 0);
                break;
            case 'o':
                if (++i >= argc) {
                    errx(1, "argument to '%s' is missing", p);
                }
                outfile = argv[i];
                break;
            case 'f':
                if (!strcmp(q, "volatile")) {
                    all_volatile = 1;
                    q += 8;
                    break;
                }
                q = p;
                break;
            case 'm':
                if (!strncmp(q, "restack=", 8)) {
                    inloop_stack = strtoul(q + 8, (char **)&q, 0);
                    break;
                }
                if (!strncmp(q, "genstart=", 9)) {
                    new_name_off = strtoul(q + 9, (char **)&q, 10);
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
    const char *filename = NULL;
    struct the_node *list = NULL;

    int i = check_args(argc, argv);

    if (binary) {
        int fd = -1;
        long sz = 0;
        fd = open(binary, O_RDONLY);
        if (fd < 0) {
            err(1, "%s", filename);
        }

        sz = lseek(fd, 0, SEEK_END);

        binmap = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
        if (binmap == MAP_FAILED) {
            err(1, "%s", filename);
        }
        binsz = sz;
        close(fd);

        if (test_gadgets > 0) {
            int rv = backend_test_gadgets(test_gadgets - 1);
            munmap((char *)binmap, binsz);
            return rv;
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
    init_tokens(f);

    new_printer(outfile);

    list = parse();
    emit_code(list);
    emit_symbols();
    free_symbols();
    free_tokens(TRUE);

    new_printer(NULL);

    if (filename) {
        fclose(f);
    }

    munmap((char *)binmap, binsz);
    return 0;
}
