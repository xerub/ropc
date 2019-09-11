#ifdef ARM64
#define PRINTF_ATTR [[regparm=1]]
#else
#define PRINTF_ATTR
#endif

/*
 * If you want to run these against the loader, make sure the imports match:
 * getpid => getpid_
 * ...
 */
extern getpid;
extern printf PRINTF_ATTR;
extern exit [[noreturn]];

/*
volatile a = 0x1FF00;
[[stack=0x400]]printf("a = 0x%lx\n", a);
printf("OR : 0x%x\n", a | 0x5A);
printf("AND: 0x%x\n", a & 0x5A00);	// x64-linux
printf("XOR: 0x%x\n", a ^ 0x15A00);	// x86-linux / x64-linux
printf("ADD: 0x%x\n", a + 3);
printf("SUB: 0x%x\n", a - 1);
printf("MUL: 0x%x\n", a * 16);		// x86-linux / x64-linux
printf("DIV: 0x%x\n", a / 16);

goto skip;
printf("should not be here\n");
skip:
*/

i = 5;
loop:
    /* give printf a hefty stack reserve (-mrestack may vary) */
    [[stack=0x400]]printf("ohai(%d) %d\n", getpid(), i);
    if (i = i - 1) goto loop;
exit(42);
