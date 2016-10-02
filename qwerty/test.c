#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mach-o/dyld.h>

typedef void __attribute__((noreturn)) (*pivot_t)(void *);

static pivot_t
get_pivot(void)
{
    extern char some_symbol_in_cache[];
    return (pivot_t)(((uintptr_t)some_symbol_in_cache & ~1) + some_offset_to_hit_ldmia); // assume we pivot by LDMIA R0, {SP,PC}
}

#define ROPSTACK_SIZE (1024 * 1024)
#define RESERVE_SPACE 0x10000

static unsigned char *
read_file(const char *filename, off_t off, size_t *size)
{
    int fd;
    size_t rv, sz;
    struct stat st;
    unsigned char *buf;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    rv = fstat(fd, &st);
    if (rv) {
        close(fd);
        return NULL;
    }

    if (off > st.st_size) {
        off = st.st_size;
    }
    sz = st.st_size - off;

    buf = malloc(sz);
    if (buf == NULL) {
        close(fd);
        return NULL;
    }

    rv = read(fd, buf, sz);
    close(fd);

    if (rv != sz) {
        free(buf);
        return NULL;
    }

    if (size != NULL) {
        *size = sz;
    }
    return buf;
}

int
main(void)
{
    int rv;
    size_t sz;
    unsigned char *buf, *stack;
    pivot_t pivot = get_pivot();
    intptr_t slide = _dyld_get_image_vmaddr_slide(1);
    char cmd[BUFSIZ];

    printf("slide: 0x%zx\n", slide);
    printf("pivot: %p\n", (void *)pivot);

    stack = valloc(ROPSTACK_SIZE);
    printf("stack: %p\n", (void *)stack);
    assert(stack);

    stack += RESERVE_SPACE;

    snprintf(cmd, sizeof(cmd), "nasm -o rope.bin -O6 -fbin -DCACHE_SLIDE=0x%zx -DROPE_ADDRESS=%p payload.asm", slide, (void *)stack);
    rv = system(cmd);
    assert(rv == 0);

    buf = read_file("rope.bin", 0, &sz);
    assert(buf && sz + RESERVE_SPACE < ROPSTACK_SIZE);
    memcpy(stack, buf, sz);

    pivot(stack);
    return 0;
}
