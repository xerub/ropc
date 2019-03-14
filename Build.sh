#!/bin/sh

ln -s backarm64.c backend.c
gcc -o ropc -Wall -W -pedantic -O2 -I. -g \
	lexer.c \
	parser.c \
	cycle.c \
	code.c \
	emit.c \
	symtab.c \
	backend.c \
	binary.c \
	util.c \
	ropc.c
