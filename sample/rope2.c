#ifdef ARM64
#define PRINTF_ATTR [[regparm=1]]
#else
#define PRINTF_ATTR
#endif

extern printf PRINTF_ATTR;
extern exit [[noreturn]];

a = 5830;
b = 23958233;
;//[[stack=0x400]]printf("prod = %lu\n", a * b);

P = 0;
M = 1;
mul:
    if !(a & M) goto skip;
    P = P + b;
skip:
    M = M * 2;
    b = b * 2;
    if (M) goto mul;
printf("prod = %lu\n", P);
exit(42);
