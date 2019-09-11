#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "elf.h"
#include "binary.h"


#define printf(...)


/* cache */


struct dyld_cache_header {
    char magic[16];			/* e.g. "dyld_v0     ppc" */
    uint32_t mappingOffset;		/* file offset to first dyld_cache_mapping_info */
    uint32_t mappingCount;		/* number of dyld_cache_mapping_info entries */
    uint32_t imagesOffset;		/* file offset to first dyld_cache_image_info */
    uint32_t imagesCount;		/* number of dyld_cache_image_info entries */
    uint64_t dyldBaseAddress;		/* base address of dyld when cache was built */
    uint64_t codeSignatureOffset;	/* file offset in of code signature blob */
    uint64_t codeSignatureSize;		/* size of code signature blob (zero means to end of file) */
};

struct dyld_cache_mapping_info {
    uint64_t address;
    uint64_t size;
    uint64_t fileOffset;
    uint32_t maxProt;
    uint32_t initProt;
};

struct dyld_cache_image_info {
    uint64_t address;
    uint64_t modTime;
    uint64_t inode;
    uint32_t pathFileOffset;
    uint32_t pad;
};


static uint32_t
find_mapping(uint32_t mappingCount, const struct dyld_cache_mapping_info map[], uint64_t address)
{
    uint32_t j, pos;
    uint64_t max = 0;
    for (pos = -1, j = 0; j < mappingCount; j++) {
        if (map[j].address <= address) {
            if (max < map[j].address) {
                max = map[j].address;
                pos = j;
            }
        }
    }
    return pos;
}


/* ranges */


struct range {
    struct range *next;
    uint64_t offset;
    uint64_t vmaddr;
    uint32_t filesize;
};


static struct range *
add_range(struct range *ranges, uint64_t offset, uint64_t vmaddr, uint32_t filesize)
{
    struct range *r;
    for (r = ranges; r; r = r->next) {
        if (r->vmaddr == vmaddr) {
            return ranges;
        }
    }
    r = malloc(sizeof(struct range));
    assert(r);
    r->offset = offset;
    r->vmaddr = vmaddr;
    r->filesize = filesize;
    r->next = ranges;
    return r;
}


void
delete_ranges(struct range *ranges)
{
    struct range *r;
    for (r = ranges; r; ) {
        struct range *q = r->next;
        free(r);
        r = q;
    }
}


static void *
reverse_list(void *n)
{
    struct range *prev = NULL;
    struct range *node = n;
    while (node) {
        struct range *next = node->next;
        node->next = prev;
        prev = node;
        node = next;
    }
    return prev;
}


static struct range *
parse_macho_ranges(const unsigned char *p, struct range *ranges)
{
    unsigned int i;
    const struct mach_header *hdr = (struct mach_header *)p;
    char *q;

    int is64 = (hdr->magic & 1) * 4;

    if ((hdr->magic & ~1) != 0xfeedface) {
        return ranges;
    }

    q = (char *)(p + sizeof(struct mach_header) + is64);
    for (i = 0; i < hdr->ncmds; i++) {
        const struct load_command *cmd = (struct load_command *)q;
        uint32_t c = cmd->cmd;
        if (c == LC_SEGMENT) {
            const struct segment_command *seg = (struct segment_command *)q;
            if (seg->initprot & 4) {
                ranges = add_range(ranges, seg->fileoff, seg->vmaddr, seg->filesize);
            }
        }
        if (c == LC_SEGMENT_64) {
            const struct segment_command_64 *seg = (struct segment_command_64 *)q;
            if (seg->initprot & 4) {
                ranges = add_range(ranges, seg->fileoff, seg->vmaddr, seg->filesize);
            }
        }
        q = q + cmd->cmdsize;
    }

    return ranges;
}


#ifdef FILTER_DYLIBS
static const char *filter_dylib[] = {
    "/usr/lib/libSystem.B.dylib",
    "/usr/lib/system/libcache.dylib",
    "/usr/lib/system/libcommonCrypto.dylib",
    "/usr/lib/system/libcompiler_rt.dylib",
    "/usr/lib/system/libcopyfile.dylib",
    "/usr/lib/system/libcorecrypto.dylib",
    "/usr/lib/system/libdispatch.dylib",
    "/usr/lib/system/libdyld.dylib",
    "/usr/lib/system/libkeymgr.dylib",
    "/usr/lib/system/liblaunch.dylib",
    "/usr/lib/system/libmacho.dylib",
    "/usr/lib/system/libquarantine.dylib",
    "/usr/lib/system/libremovefile.dylib",
    "/usr/lib/system/libsystem_asl.dylib",
    "/usr/lib/system/libsystem_blocks.dylib",
    "/usr/lib/system/libsystem_c.dylib",
    "/usr/lib/system/libsystem_configuration.dylib",
    "/usr/lib/system/libsystem_coreservices.dylib",
    "/usr/lib/system/libsystem_darwin.dylib",
    "/usr/lib/system/libsystem_dnssd.dylib",
    "/usr/lib/system/libsystem_info.dylib",
    "/usr/lib/system/libsystem_m.dylib",
    "/usr/lib/system/libsystem_malloc.dylib",
    "/usr/lib/system/libsystem_networkextension.dylib",
    "/usr/lib/system/libsystem_notify.dylib",
    "/usr/lib/system/libsystem_sandbox.dylib",
    "/usr/lib/system/libsystem_secinit.dylib",
    "/usr/lib/system/libsystem_kernel.dylib",
    "/usr/lib/system/libsystem_platform.dylib",
    "/usr/lib/system/libsystem_pthread.dylib",
    "/usr/lib/system/libsystem_symptoms.dylib",
    "/usr/lib/system/libsystem_trace.dylib",
    "/usr/lib/system/libunwind.dylib",
    "/usr/lib/system/libxpc.dylib",
    "/usr/lib/libobjc.A.dylib",
    "/usr/lib/libc++abi.dylib",
    "/usr/lib/libc++.1.dylib",
    NULL
};
#endif


static struct range *
parse_cache_ranges(const unsigned char *p, struct range *ranges)
{
    unsigned i;
    const struct dyld_cache_header *hdr = (struct dyld_cache_header *)p;
    const struct dyld_cache_mapping_info *map = (struct dyld_cache_mapping_info *)(p + hdr->mappingOffset);
#ifdef FILTER_DYLIBS
    const struct dyld_cache_image_info *img = (struct dyld_cache_image_info *)(p + hdr->imagesOffset);
    for (i = 0; i < hdr->imagesCount; i++) {
        unsigned k;
        for (k = 0; filter_dylib[k]; k++) {
            if (!strcmp((char *)p + img[i].pathFileOffset, filter_dylib[k])) {
                uint64_t address = img[i].address;
                int j = find_mapping(hdr->mappingCount, map, address);
                if (j != -1) {
                    ranges = parse_macho_ranges(p + address - map[j].address + map[j].fileOffset, ranges);
                }
                break;
            }
        }
    }
#else
    for (i = 0; i < hdr->mappingCount; i++) {
        if (map[i].initProt & 4) {
            ranges = add_range(ranges, map[i].fileOffset, map[i].address, map[i].size);
        }
    }
#endif
    return ranges;
}


static struct range *
parse_elf32_ranges(const unsigned char *p, struct range *ranges)
{
    unsigned i;
    const Elf32_Ehdr *hdr = (Elf32_Ehdr *)p;
    const Elf32_Phdr *phdr = (Elf32_Phdr *)(p + hdr->e_phoff);

    for (i = 0; i < hdr->e_phnum; i++) {
        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_flags & PF_X) {
                ranges = add_range(ranges, phdr->p_offset, phdr->p_vaddr, phdr->p_filesz);
            }
        }
        phdr++;
    }

    return ranges;
}

static struct range *
parse_elf64_ranges(const unsigned char *p, struct range *ranges)
{
    unsigned i;
    const Elf64_Ehdr *hdr = (Elf64_Ehdr *)p;
    const Elf64_Phdr *phdr = (Elf64_Phdr *)(p + hdr->e_phoff);

    for (i = 0; i < hdr->e_phnum; i++) {
        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_flags & PF_X) {
                ranges = add_range(ranges, phdr->p_offset, phdr->p_vaddr, phdr->p_filesz);
            }
        }
        phdr++;
    }

    return ranges;
}


struct range *
parse_ranges(const unsigned char *p, size_t sz)
{
    struct range *ranges;
    if (!strncmp((char *)p, "dyld_v1 ", 8)) {
        ranges = parse_cache_ranges(p, NULL);
    } else if ((*(unsigned *)p & ~1) == 0xfeedface) {
        ranges = parse_macho_ranges(p, NULL);
    } else if (!memcmp(p, "\177ELF\001", 5)) {
        ranges = parse_elf32_ranges(p, NULL);
    } else if (!memcmp(p, "\177ELF\002", 5)) {
        ranges = parse_elf64_ranges(p, NULL);
    } else {
        ranges = add_range(NULL, 0, 0, sz);
    }
    return reverse_list(ranges);
}


/* symbols */


static uint64_t
parse_macho_symbols(const unsigned char *p, uint64_t address, const char *key)
{
    unsigned int i;
    const struct mach_header *hdr = (struct mach_header *)(p + address);
    char *q;

    uint32_t stroff = 0;
    uint32_t symoff = 0;
    int iextdefsym = -1;
    int nextdefsym = -1;

    int is64 = (hdr->magic & 1) * 4;

    if ((hdr->magic & ~1) != 0xfeedface) {
        return 0;
    }

    q = (char *)(p + address + sizeof(struct mach_header) + is64);
    for (i = 0; i < hdr->ncmds; i++) {
        const struct load_command *cmd = (struct load_command *)q;
        uint32_t c = cmd->cmd;
        if (c == LC_SYMTAB) {
            struct symtab_command *sym = (struct symtab_command *)q;
            symoff = sym->symoff;
            stroff = sym->stroff;
        } else if (c == LC_DYSYMTAB) {
            struct dysymtab_command *sym = (struct dysymtab_command *)q;
            iextdefsym = sym->iextdefsym;
            nextdefsym = sym->nextdefsym;
        }
        q = q + cmd->cmdsize;
    }

    assert(symoff && stroff && iextdefsym >= 0 && nextdefsym >= 0);

if (is64) {
    const struct nlist_64 *base = NULL;
    for (base = (struct nlist_64 *)(p + symoff) + iextdefsym; nextdefsym > 0; nextdefsym /= 2) {
        const struct nlist_64 *pivot = &base[nextdefsym / 2];
        int cmp = strcmp(key, (char *)p + stroff + pivot->n_un.n_strx);
        if (cmp == 0) {
            long long thumb = (pivot->n_desc & N_ARM_THUMB_DEF) != 0;
            fprintf(stderr, "0x%llX: %s\n", thumb + pivot->n_value + thumb, key);
            return pivot->n_value + thumb;
        }
        if (cmp > 0) {
            base = &pivot[1];
            --nextdefsym;
        }
    }
} else {
    const struct nlist *base = NULL;
    for (base = (struct nlist *)(p + symoff) + iextdefsym; nextdefsym > 0; nextdefsym /= 2) {
        const struct nlist *pivot = &base[nextdefsym / 2];
        int cmp = strcmp(key, (char *)p + stroff + pivot->n_un.n_strx);
        if (cmp == 0) {
            int thumb = (pivot->n_desc & N_ARM_THUMB_DEF) != 0;
            fprintf(stderr, "0x%X: %s\n", thumb + pivot->n_value, key);
            return pivot->n_value + thumb;
        }
        if (cmp > 0) {
            base = &pivot[1];
            --nextdefsym;
        }
    }
}

    return 0;
}


static uint64_t
parse_cache_symbols(const unsigned char *p, const char *key)
{
    unsigned i;
    const struct dyld_cache_header *hdr = (struct dyld_cache_header *)p;
    const struct dyld_cache_mapping_info *map = (struct dyld_cache_mapping_info *)(p + hdr->mappingOffset);
    const struct dyld_cache_image_info *img = (struct dyld_cache_image_info *)(p + hdr->imagesOffset);
    printf("magic: \"%.16s\"\n", hdr->magic);
    printf("mappingOffset: 0x%X\n", hdr->mappingOffset);
    printf("mappingCount: %d\n", hdr->mappingCount);
    printf("imagesOffset: 0x%X\n", hdr->imagesOffset);
    printf("imagesCount: %d\n", hdr->imagesCount);
    printf("dyldBaseAddress: 0x%llX\n", hdr->dyldBaseAddress);
    printf("codeSignatureOffset: 0x%llX\n", hdr->codeSignatureOffset);
    printf("codeSignatureSize: %lld\n", hdr->codeSignatureSize);
    for (i = 0; i < hdr->mappingCount; i++) {
        printf("MAPPING#%d\n", i);
        printf("\taddress = 0x%llX\n", map[i].address);
        printf("\tsize = %lld\n", map[i].size);
        printf("\tfileOffset = 0x%llX\n", map[i].fileOffset);
        printf("\tprotection = 0x%X / 0x%X\n", map[i].maxProt, map[i].initProt);
    }
    for (i = 0; i < hdr->imagesCount; i++) {
        uint64_t address = img[i].address;
        int j = find_mapping(hdr->mappingCount, map, address);

        printf("IMAGE#%d\n", i);
        printf("\taddress = 0x%llX\n", img[i].address);
        printf("\tmodTime = %lld\n", img[i].modTime);
        printf("\tinode = %lld\n", img[i].inode);
        printf("\tpathFileOffset = 0x%X\n", img[i].pathFileOffset);
        /*printf("\tpad = 0x%X\n", img[i].pad);*/
        printf("\tNAME = %s\n", p + img[i].pathFileOffset);

        if (j != -1) {
            uint64_t rv = parse_macho_symbols(p, address - map[j].address + map[j].fileOffset, key);
            if (rv) {
                return rv;
            }
        }
    }
    return 0;
}


static uint64_t
parse_elf32_symbols(const unsigned char *p, const char *name)
{
    unsigned i;
    const Elf32_Ehdr *hdr = (Elf32_Ehdr *)p;
    const Elf32_Shdr *shdr = (Elf32_Shdr *)(p + hdr->e_shoff);

    for (i = 0; i < hdr->e_shnum; i++) {
        if (shdr->sh_type == SHT_SYMTAB || shdr->sh_type == SHT_DYNSYM) {
            const Elf32_Sym *sym = (Elf32_Sym *)(p + shdr->sh_offset);
            const char *strtab = (char *)(p + ((Elf32_Shdr *)(p + hdr->e_shoff) + shdr->sh_link)->sh_offset);
            unsigned n = shdr->sh_size / shdr->sh_entsize;
            while (n--) {
                if (sym->st_value && sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS &&
                    ELF32_ST_TYPE(sym->st_info) != STT_SECTION && ELF32_ST_TYPE(sym->st_info) != STT_FILE &&
                    !strcmp(name, strtab + sym->st_name)) {
                    fprintf(stderr, "0x%X: %s\n", sym->st_value, name);
                    return sym->st_value;
                }
                sym++;
            }
        }
        shdr++;
    }

    return 0;
}

static uint64_t
parse_elf64_symbols(const unsigned char *p, const char *name)
{
    unsigned i;
    const Elf64_Ehdr *hdr = (Elf64_Ehdr *)p;
    const Elf64_Shdr *shdr = (Elf64_Shdr *)(p + hdr->e_shoff);

    for (i = 0; i < hdr->e_shnum; i++) {
        if (shdr->sh_type == SHT_SYMTAB || shdr->sh_type == SHT_DYNSYM) {
            const Elf64_Sym *sym = (Elf64_Sym *)(p + shdr->sh_offset);
            const char *strtab = (char *)(p + ((Elf64_Shdr *)(p + hdr->e_shoff) + shdr->sh_link)->sh_offset);
            unsigned n = shdr->sh_size / shdr->sh_entsize;
            while (n--) {
                if (sym->st_value && sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS &&
                    ELF32_ST_TYPE(sym->st_info) != STT_SECTION && ELF32_ST_TYPE(sym->st_info) != STT_FILE &&
                    !strcmp(name, strtab + sym->st_name)) {
                    fprintf(stderr, "0x%llX: %s\n", (unsigned long long)sym->st_value, name);
                    return sym->st_value;
                }
                sym++;
            }
        }
        shdr++;
    }

    return 0;
}


uint64_t
parse_symbols(const unsigned char *p, const char *key)
{
    uint64_t rv;
    if (!strncmp((char *)p, "dyld_v1 ", 8)) {
        rv = parse_cache_symbols(p, key);
    } else if ((*(unsigned *)p & ~1) == 0xfeedface) {
        rv = parse_macho_symbols(p, 0, key);
    } else if (!memcmp(p, "\177ELF\001", 5)) {
        rv = parse_elf32_symbols(p, key + 1);
    } else if (!memcmp(p, "\177ELF\002", 5)) {
        rv = parse_elf64_symbols(p, key + 1);
    } else {
        rv = 0;
    }
    return rv;
}


/* search */


#define UCHAR_MAX 255

static unsigned char *
boyermoore_horspool_memmem(const unsigned char* haystack, size_t hlen,
                           const unsigned char* needle,   size_t nlen)
{
    size_t last, scan = 0;
    size_t bad_char_skip[UCHAR_MAX + 1]; /* Officially called:
                                          * bad character shift */

    /* Sanity checks on the parameters */
    if (nlen <= 0 || !haystack || !needle)
        return NULL;

    /* ---- Preprocess ---- */
    /* Initialize the table to default value */
    /* When a character is encountered that does not occur
     * in the needle, we can safely skip ahead for the whole
     * length of the needle.
     */
    for (scan = 0; scan <= UCHAR_MAX; scan = scan + 1)
        bad_char_skip[scan] = nlen;

    /* C arrays have the first byte at [0], therefore:
     * [nlen - 1] is the last byte of the array. */
    last = nlen - 1;

    /* Then populate it with the analysis of the needle */
    for (scan = 0; scan < last; scan = scan + 1)
        bad_char_skip[needle[scan]] = last - scan;

    /* ---- Do the matching ---- */

    /* Search the haystack, while the needle can still be within it. */
    while (hlen >= nlen)
    {
        /* scan from the end of the needle */
        for (scan = last; haystack[scan] == needle[scan]; scan = scan - 1)
            if (scan == 0) /* If the first byte matches, we've found it. */
                return (void *)haystack;

        /* otherwise, we need to skip some bytes and start again.
           Note that here we are getting the skip value based on the last byte
           of needle, no matter where we didn't match. So if needle is: "abcd"
           then we are skipping based on 'd' and that value will be 4, and
           for "abcdd" we again skip on 'd' but the value will be only 1.
           The alternative of pretending that the mismatched character was
           the last character is slower in the normal case (E.g. finding
           "abcd" in "...azcd..." gives 4 by using 'd' but only
           4-2==2 using 'z'. */
        hlen     -= bad_char_skip[haystack[last]];
        haystack += bad_char_skip[haystack[last]];
    }

    return NULL;
}

static size_t
str2hex(size_t buflen, unsigned char *buf, unsigned char *mask, const char *str)
{
    unsigned char *ptr = buf;
    int seq = -1;
    int m = 0;
    while (buflen > 0) {
        int nibble = *str++;
        if (nibble >= '0' && nibble <= '9') {
            nibble -= '0';
            m |= 0xF;
        } else if (nibble == '.') {
            nibble = 0;
        } else if (nibble == ' ' && seq < 0) {
            continue;
        } else {
            nibble |= 0x20;
            if (nibble < 'a' || nibble > 'f') {
                break;
            }
            nibble -= 'a' - 10;
            m |= 0xF;
        }
        if (seq >= 0) {
            *buf++ = (seq << 4) | nibble;
            *mask++ = m;
            buflen--;
            seq = -1;
            m = 0;
        } else {
            seq = nibble;
            m <<= 4;
        }
    }
    return buf - ptr;
}

static size_t
find_sequence(const unsigned char *mask, size_t n, size_t *len)
{
    size_t i;
    size_t seq_len = 0;
    size_t best_len = 0;
    size_t best_pos = 0;
    for (i = 0; i < n; i++, seq_len++) {
        if (mask[i] != 0xFF) {
            if (best_len < seq_len) {
                best_len = seq_len;
                best_pos = i;
            }
            seq_len = -1;
        }
    }
    if (best_len < seq_len) {
        best_len = seq_len;
        best_pos = i;
    }
    *len = best_len;
    return best_pos - best_len;
}

static unsigned char *
process_pattern(const char *str, size_t *len, unsigned char **out_mask, size_t *seq_pos, size_t *seq_len)
{
    size_t n = strlen(str) / 2;
    unsigned char *pattern, *mask;

    if (!n) {
        return NULL;
    }
    pattern = malloc(n);
    if (!pattern) {
        return NULL;
    }
    mask = malloc(n);
    if (!mask) {
        free(pattern);
        return NULL;
    }

    n = str2hex(n, pattern, mask, str);
    if (!n) {
        free(mask);
        free(pattern);
        return NULL;
    }

    *len = n;
    *out_mask = mask;
    *seq_pos = find_sequence(mask, n, seq_len);
    return pattern;
}

static const unsigned char *
find_string(const unsigned char* haystack, size_t hlen,
            const unsigned char* needle,   size_t nlen,
            const unsigned char* mask, size_t seq_pos, size_t seq_len)
{
    size_t tail = nlen - (seq_pos + seq_len);
    while (hlen >= tail) {
        size_t i;
        const unsigned char *ptr = haystack;
        if (seq_len) {
            ptr = boyermoore_horspool_memmem(haystack + seq_pos, hlen - tail, needle + seq_pos, seq_len);
            if (!ptr) {
                break;
            }
            ptr -= seq_pos;
        }
        for (i = 0; i < seq_pos; i++) {
            if ((ptr[i] & mask[i]) != needle[i]) {
                break;
            }
        }
        if (i < seq_pos) {
            hlen -= ptr - haystack + 1;
            haystack = ptr + 1;
            continue;
        }
        for (i += seq_len; i < nlen; i++) {
            if ((ptr[i] & mask[i]) != needle[i]) {
                break;
            }
        }
        if (i < nlen) {
            hlen -= ptr - haystack + 1;
            haystack = ptr + 1;
            continue;
        }
        return ptr;
    }
    return NULL;
}

uint64_t
parse_string(const struct range *ranges, const unsigned char *p, void *user, callback_t callback, const char *str)
{
    const struct range *r;
    uint64_t found = 0;
    int align = 0;

    size_t len, seq_len, seq_pos;
    unsigned char *pattern, *mask;

    switch (*str) {
        case '+':
            str++;
            align = 3;
            break;
        case '-':
            str++;
            align = 1;
            break;
    }

    pattern = process_pattern(str, &len, &mask, &seq_pos, &seq_len);
    if (!pattern) {
        return 0;
    }

    for (r = ranges; r; r = r->next) {
        const unsigned char *buf = p + r->offset;
        const unsigned char *ptr = buf - 1;
        while (1) {
            unsigned long long addr;
            size_t left = r->filesize - (++ptr - buf);
            ptr = find_string(ptr, left, pattern, len, mask, seq_pos, seq_len);
            if (!ptr) {
                break;
            }
            addr = r->vmaddr + ptr - buf;
            if (addr & align) {
                continue;
            }
            if (callback && callback(ptr, buf + r->filesize - ptr, addr, user)) {
                continue;
            }
            found = addr;
            goto done;
        }
    }

  done:
    free(mask);
    free(pattern);
    return found;
}


#ifdef HAVE_MAIN
int
main(int argc, char **argv)
{
    int fd;
    long sz;
    unsigned char *p;
    struct range *ranges;
    const char *filename = "cache";

    if (argc > 1) {
        filename = *++argv;
        argc--;
    }

    fd = open(filename, O_RDONLY);
    assert(fd >= 0);
    sz = lseek(fd, 0, SEEK_END);
    p = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
    assert(p != MAP_FAILED);
    close(fd);

    ranges = parse_ranges(p, sz);

    parse_string(ranges, p, NULL, NULL, "+ 0a f0 14 e8");

    delete_ranges(ranges);

    while (argc-- > 1) {
        parse_symbols(p, *++argv);
    }

    munmap(p, sz);
    return 0;
}
#endif
