extern exit [[noreturn]];
extern printf [[regparm = 1]];

volatile i = -5;
again:
    printf("OHAI %d\n", i);
if (i = i + 1) goto again;
exit(42);
