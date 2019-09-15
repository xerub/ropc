#ifdef ARM64
#define PRINTF_ATTR [[regparm=1]]
#else
#define PRINTF_ATTR
#endif

#if BITS == 32
#define SIZEOF_LONG 4
#define SIGN_BIT 0x80000000
#else
#define SIZEOF_LONG 8
#define SIGN_BIT 0x8000000000000000
#endif

extern printf PRINTF_ATTR;
extern exit [[noreturn]];

/*
void
back(unsigned n, int *sol)
{
    int k;
    init(k = 0, n, sol);
    while (k >= 0) next: {
        if (k == n) {
            retsol(n, sol);
        } else {
            while (sol[k] < n) {
                sol[k]++;
                if (cont(k, sol)) {
                    init(++k, n, sol);
                    goto next;
                }
            }
        }
        k--;
    }
}
*/

#define QUEENS

n = 4;
sol = { 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // must have at least n + 1 elements. first element is already primed

k = 0;
while (k + 1) next: {
    if !(k - n) {
        // solution
        if (i = n) {
            sol_ptr = sol;
            do {
                [[stack=0x400]]printf(" %d", *sol_ptr);
                sol_ptr = sol_ptr + SIZEOF_LONG;
            } while (i = i - 1);
            [[stack=0x400]]printf("\n");
        }
    backtrack:
        k = k - 1;
        continue;
    }
    // check the input domain [1..N]
    dst_ptr = elt_ptr = sol + k * SIZEOF_LONG;
    domain: while (*elt_ptr - n) {
        *dst_ptr = *elt_ptr + 1; // attempt next value
        cur_ptr = sol;
#ifdef QUEENS
        j = 0;
#endif
        while (cur_ptr - elt_ptr) {
#ifdef QUEENS
            dif = *elt_ptr - *cur_ptr;
            if !(dif) goto domain; // (same row) try harder
            if (dif & SIGN_BIT) dif = 0 - dif;
            if !(dif - k + j) goto domain; // (same diag) try harder
            j = j + 1;
#else
            if !(*elt_ptr - *cur_ptr) goto domain; // try harder
#endif
            cur_ptr = cur_ptr + SIZEOF_LONG;
        }
        k = k + 1;
        new_ptr = elt_ptr + SIZEOF_LONG;
        *new_ptr = 0; // prime next element
        goto next;
    }
    goto backtrack;
}
exit(42);
