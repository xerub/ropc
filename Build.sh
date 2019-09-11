#!/bin/sh

function build() {
gcc -o "$1" -Wall -W -pedantic -O2 -I. -g -Wno-unused-parameter -Wno-unused-function \
	lexer.c \
	parser.c \
	cycle.c \
	code.c \
	emit.c \
	symtab.c \
	"$2" \
	binary.c \
	util.c \
	ropc.c
}

if [ $# -eq 0 ]; then
    build ropc backend.c
    exit
fi

ARCHS=("$@")

if [ "x$1" = "xall" ]; then
    ARCHS=(arm arm64 x86 x86_64 txt)
fi

for i in "${ARCHS[@]}"; do
    build "ropc-$i" "back$i.c"
done
