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


/* parser */


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


/* parser(gadgets) */


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
parse_macho_ranges(const unsigned char *p, uint64_t address, struct range *ranges)
{
    unsigned int i;
    const struct mach_header *hdr = (struct mach_header *)(p + address);
    char *q;

    int is64 = (hdr->magic & 1) * 4;

    if ((hdr->magic & ~1) != 0xfeedface) {
        return ranges;
    }

    q = (char *)(p + address + sizeof(struct mach_header) + is64);
    for (i = 0; i < hdr->ncmds; i++) {
        const struct load_command *cmd = (struct load_command *)q;
        uint32_t c = cmd->cmd;
        if (c == LC_SEGMENT) {
            const struct segment_command *seg = (struct segment_command *)q;
            if (seg->initprot & 4) {
                ranges = add_range(ranges, address + seg->fileoff, seg->vmaddr, seg->filesize);
            }
        }
        if (c == LC_SEGMENT_64) {
            const struct segment_command_64 *seg = (struct segment_command_64 *)q;
            if (seg->initprot & 4) {
                ranges = add_range(ranges, address /*+ seg->fileoff*/, seg->vmaddr, seg->filesize);
            }
        }
        q = q + cmd->cmdsize;
    }

    return ranges;
}


static struct range *
parse_cache_ranges(const unsigned char *p, struct range *ranges)
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
            ranges = parse_macho_ranges(p, address - map[j].address + map[j].fileOffset, ranges);
        }
    }
    return ranges;
}


struct range *
parse_ranges(const unsigned char *p)
{
    struct range *ranges;
    if (!strncmp((char *)p, "dyld_v1   arm", 13) || !strncmp((char *)p, "dyld_v1  arm", 12)) {
        ranges = parse_cache_ranges(p, NULL);
    } else {
        ranges = parse_macho_ranges(p, 0, NULL);
    }
    return reverse_list(ranges);
}


uint64_t
parse_gadgets(const struct range *ranges, const unsigned char *p, void *user, callback_t callback, ...)
{
    const struct range *r;
    uint64_t found = 0;
    va_list ap;
    va_start(ap, callback);
    for (r = ranges; r; r = r->next) {
        uint32_t i;
        const unsigned char *buf = p + r->offset;
        uint64_t addr = r->vmaddr;
        uint32_t sz = r->filesize;
        for (i = 0; i < sz; i += 2) {
            int rv = callback(buf + i, sz - i, ap, i + addr, user);
            if (rv) {
                int k;
                int thumb = (rv & 1);
                fprintf(stderr, "FOUND = 0x%08llX:", addr + thumb + i);
                for (k = 0; k < 10; k++) {
                    fprintf(stderr, " %02x", buf[i + k]);
                }
                fprintf(stderr, "\n");
                if (rv > 0) {
                    found = addr + thumb + i;
                    goto done;
                }
            }
        }
    }
  done:
    va_end(ap);
    return found;
}


/* parser(symbols) */


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
            int thumb = (pivot->n_desc & N_ARM_THUMB_DEF) != 0;
            fprintf(stderr, "0x%llX: %s\n", pivot->n_value + thumb, key);
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
            fprintf(stderr, "0x%X: %s\n", pivot->n_value + thumb, key);
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


uint64_t
parse_symbols(const unsigned char *p, const char *key)
{
    uint64_t rv;
    if (!strncmp((char *)p, "dyld_v1   arm", 13) || !strncmp((char *)p, "dyld_v1  arm", 12)) {
        rv = parse_cache_symbols(p, key);
    } else {
        rv = parse_macho_symbols(p, 0, key);
    }
    return rv;
}


/* search */


static int
_is_bx_lr(const unsigned char *buf)
{
    return (buf[1] == 0x47 && (buf[0] & 0xF8) == 0x70);
}


static int
_is_pop_pc(const unsigned char *buf, int nregs)
{
    if (buf[1] == 0xBD) {
        if (nregs < 0 || __builtin_popcount(buf[0]) == nregs) {
            return nregs;
        }
    }
    return 0;
}


static int
_is_add(const unsigned char *buf, int r1, int r2, int mask)
{
    int op = buf[0];
    int src1 = (op >> 6) & 3;
    int src2 = (op >> 3) & 7;
    if (buf[1] & 1) {
        src1 += 4;
    }
    if ((src1 == r1 && src2 == r2)
        ||
        (src2 == r1 && src1 == r2)) {
        int dst = op & 7;
        if (mask & (1 << dst)) {	// 2, 4, 5, 6, 7
            return 1;
        }
    }
    return 0;
}


static int
_is_popw(const unsigned char *buf, int nregs)
{
    // LDR.W R?,[SP],#4
    if (nregs == 1 && buf[0] == 0x5d && buf[1] == 0xf8 && buf[2] == 0x04 && (buf[3] & 0xF) == 0xb) {
        return 1;
    }
    // POP.W {...}
    return (buf[0] == 0xbd && buf[1] == 0xe8 && buf[2] == 0x00 && __builtin_popcount(buf[3]) == nregs);
}


int
is_LOAD_R4(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // POP {R4,PC}
    return (buf[0] == 0x10 && buf[1] == 0xbd);
}


int
is_LOAD_R0(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // POP {R0,PC}
    return (buf[0] == 0x01 && buf[1] == 0xbd);
}


int
is_LOAD_R0R1(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // POP {R0-R1,PC}
    return (buf[0] == 0x03 && buf[1] == 0xbd);
}


int
is_LOAD_R0R3(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // LDMFD SP!, {R0-R3,PC}
    if (buf[0] == 0x0f && buf[1] == 0x80 && buf[2] == 0xbd && buf[3] == 0xe8) {
        return 2;
    }
    // POP {R0-R3,PC}
    if (buf[0] == 0x0F && buf[1] == 0xbd) {
        return 1;
    }
    return 0;
}


int
is_LOAD_R4R5(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // POP {R4,R5,PC}
    return (buf[0] == 0x30 && buf[1] == 0xbd);
}


int
is_LDR_R0_R0(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // LDR R0,[R0] / POP {R7,PC}
    return (buf[0] == 0x00 && buf[1] == 0x68 && buf[2] == 0x80 && buf[3] == 0xbd);
}


int
is_STR_R0_R4(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // STR R0,[R4] / POP {R4,R7,PC}
    return (buf[0] == 0x20 && buf[1] == 0x60 && buf[2] == 0x90 && buf[3] == 0xbd);
}


int
is_ADD_R0_R1(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // ADD R0,R1 / POP {R7,PC}
    return (buf[0] == 0x08 && buf[1] == 0x44 && buf[2] == 0x80 && buf[3] == 0xbd);
}


int
is_SUB_R0_R1(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // SUBS R0,R0,R1 / POP {R7,PC}
    return (buf[0] == 0x40 && buf[1] == 0x1A && buf[2] == 0x80 && buf[3] == 0xbd);
}


int
is_MUL_R0_R1(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // MULS R0,R1 / POP {R7,PC}
    return (buf[0] == 0x48 && buf[1] == 0x43 && buf[2] == 0x80 && buf[3] == 0xbd);
}


int
is_BLX_R4(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // BLX R4 / POP {R4,R7,PC}
    return (buf[0] == 0xa0 && buf[1] == 0x47 && buf[2] == 0x90 && buf[3] == 0xbd);
}


int
is_BLX_R4_SP(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // BLX R4 / adjust stack / POP {R7,PC}
    if (buf[0] == 0xa0 && buf[1] == 0x47) {
        int i, add;
        va_list aq;
        va_copy(aq, ap);
        add = va_arg(aq, int);	// number of dwords to pop off stack before return
        va_end(aq);
        assert(add >= 0 && add < 128);
        for (i = 0; i <= add; i++) {
            int sum = 0;
            const unsigned char *ptr = buf + 2;
            // ADD SP, #?
            if (i != add && ptr[0] == add - i && ptr[1] == 0xB0) {
                sum += add - i;
                ptr += 2;
            }
            if (i && _is_popw(ptr, i)) {
                sum += i;
                ptr += 4;
            }
            if (sum == add && _is_pop_pc(ptr, -1)) {
                if (user) {
                    ((uint64_t *)user)[0] = (uintptr_t)ptr;
                }
                return -1;
            }
            if (i == 4) {
                break; // there is no point in continuing, since popw will not pop more than 4
            }
        }
    }
    return 0;
}


int
is_RET_0(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // MOVS R0, #0 / BX LR
    return (buf[0] == 0x00 && buf[1] == 0x20 && buf[2] == 0x70 && buf[3] == 0x47);
}


int
is_ADD_SP(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // ADD SP, #? / POP {R7,PC}
    const unsigned char *ptr = NULL;
    unsigned int sum = 0;
    if (buf[1] == 0xB0) {
        sum = buf[0];
        ptr = buf + 2;
    }
    if (buf[0] == 0x0D && buf[1] == 0xF5 && (buf[3] & 0x8F) == 0x0D) {
        unsigned int x = buf[2];
        unsigned int y = (buf[3] >> 4);
        sum = ((x < 0x80) ? (0x100 + x * 2) : x) << (15 - 2 * y);
        if (sum & 3) {
            return 0;
        }
        sum /= 4;
        ptr = buf + 4;
    }
    if (ptr) {
        int i;
        for (i = 1; i <= 4; i++) {
            if (_is_popw(ptr, i)) {
                sum += i;
                ptr += 4;
            }
        }
        if (_is_pop_pc(ptr, 1)) {
            unsigned add;
            va_list aq;
            va_copy(aq, ap);
            add = va_arg(aq, unsigned);	// number of dwords to pop off stack before return
            va_end(aq);
            if (sum >= add) {
                if (user) {
                    ((uint64_t *)user)[0] = (uintptr_t)ptr;
                    ((uint64_t *)user)[1] = sum;
                }
                return (sum == add) ? 1 : -1;
            }
        }
    }
    return 0;
}


int
is_MOV_Rx_R0(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // MOV Rx, R0 / POP {...PC}
    if (buf[1] == 0x46 && _is_pop_pc(buf + 2, -1)) {
        if ((buf[0] & 0x78) == 0) { // MOV Rx, R0
            int reg;
            va_list aq;
            va_copy(aq, ap);
            reg = va_arg(aq, int);	// register
            va_end(aq);
            if ((buf[0] & 0x87) == (((reg & 8) << 4) | (reg & 7))) {
                ((uint64_t *)user)[0] = (uintptr_t)buf + 2;
                return -1;
            }
        }
    }
    return 0;
}


int
is_MOV_R0_Rx(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // MOV R0, Rx / POP {...PC}
    if (buf[1] == 0x46 && _is_pop_pc(buf + 2, 1)) {
        if ((buf[0] & 0x87) == 0) { // MOV R0, Rx
            int reg;
            va_list aq;
            va_copy(aq, ap);
            reg = va_arg(aq, int);	// register
            va_end(aq);
            if ((buf[0] & 0x78) == (reg << 3)) {
                return 1;
            }
        }
    }
    return 0;
}


int
is_COMPARE(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    // CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}
    if (buf[0] == 0x00 && buf[1] == 0x28 &&
            buf[2] == 0x08 && buf[3] == 0xBF &&
            buf[4] == 0x2C && buf[5] == 0x46 &&
            buf[6] == 0x20 && buf[7] == 0x46 &&
            buf[9] == 0xbd) {
        return (buf[8] == 0xb0) ? 1 : -1;
    }
    return 0;
}


static int
is_reg_pivot(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    int next = 0;
    if ((buf[1] & 0xFE) == 0x18) {
        if (_is_bx_lr(buf + 2)) {
            next = 2;
        } else if (_is_bx_lr(buf + 4)) {
            next = 4;
        }
        if (next) {
            int src1;
            int src2;
            int mask;
            va_list aq;
            va_copy(aq, ap);
            src1 = va_arg(aq, int);	// source register
            src2 = va_arg(aq, int);	// source register
            mask = va_arg(aq, int);	// destination register mask
            va_end(aq);
            if (_is_add(buf, src1, src2, mask)) {
                return (next > 2) ? -1 : 1;
            }
        }
    }
    return 0;
}


static int
is_reg_pivot_alt(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    if ((buf[1] & 0xFE) == 0x18) {
        int src1;
        int src2;
        int mask;
        int nregs;
        va_list aq;
        va_copy(aq, ap);
        src1 = va_arg(aq, int);	// source register
        src2 = va_arg(aq, int);	// source register
        mask = va_arg(aq, int);	// destination register mask
        nregs = va_arg(aq, int);// number of register popped off stack
        va_end(aq);
        return (_is_add(buf, src1, src2, mask) && _is_pop_pc(buf + 2, nregs));
    }
    return 0;
}


int
is_ldmia(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
/*
FF FF 96 E9 <- 8 == LDMIA, 9 = LDMIB
   ^  ^^ ^
   |  || +---- cond (0xE == always)
   |  |+------ source register
   |  +------- 1, 3, 5^, 7^, 9, B, D^, F^
   +---------- & 0xA == 0xA (SP+PC)
*/
    if ((buf[1] & 0xA0) == 0xA0 &&
        (buf[2] & 0x10) == 0x10 && (buf[2] & 0x50) != 0x50 &&
        (buf[3] & 0xE) == 0x8 && (buf[3] & 0xF0) != 0xF0 &&
        (addr & 2) == 0) {
        int reg;
        int nregs;
        int mask;
        va_list aq;
        va_copy(aq, ap);
        reg = va_arg(aq, int);	// source register
        nregs = va_arg(aq, int);// number of register slots before SP
        mask = va_arg(aq, int);	// mask of registers we really want (besides SP+PC)
        va_end(aq);
        if ((buf[2] & 0xF) == reg &&
            (nregs < 0 || (__builtin_popcount(buf[0]) + __builtin_popcount(buf[1] & 0x1F)) == nregs)) {
            if (user) {
                ((uint64_t *)user)[0] = (uintptr_t)buf;
            }
#if 1
            if ((buf[2] & 0xF0) == 0x90 && buf[3] == 0xE8) {
                mask &= 0x5FFF;
                if ((((buf[1] << 8) | buf[0]) & mask) == mask) {
                    return 2;
                }
            }
#endif
            return -2;
        }
    }
    return 0;
}


int
is_ldmiaw(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
/*
94 E8 FF 80 <- & 0xA == 0xA (SP+PC)
^^ ^
|| +---------- E8 == LDMIA, E9 = LDMIB (!only opcode E8 was tested!)
|+------------ source register
+------------- 1, 3, 5^, 7^, 9, B, D^, F^ (!only opcode 9 was tested!)
*/
    if ((buf[3] & 0xA0) == 0xA0 &&
        (buf[0] & 0x10) == 0x10 && (buf[0] & 0x50) != 0x50 &&
        (buf[1] == 0xE8)) {
        int reg;
        int nregs;
        int mask;
        va_list aq;
        va_copy(aq, ap);
        reg = va_arg(aq, int);	// source register
        nregs = va_arg(aq, int);// number of register slots before SP
        mask = va_arg(aq, int);	// mask of registers we really want (besides SP+PC)
        va_end(aq);
        if ((buf[0] & 0xF) == reg &&
            (nregs < 0 || (__builtin_popcount(buf[2]) + __builtin_popcount(buf[3] & 0x1F)) == nregs)) {
            if (user) {
                ((uint64_t *)user)[0] = (uintptr_t)buf;
            }
#if 1
            if ((buf[0] & 0xF0) == 0x90) {
                mask &= 0x5FFF;
                if ((((buf[3] << 8) | buf[2]) & mask) == mask) {
                    return 1;
                }
            }
#endif
            return -1;
        }
    }
    return 0;
}


int
is_string(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user)
{
    int rv = 1;
    int nibble;
    const char *str;
    // XXX this is awfully slow: we should not va_copy for each byte
    va_list aq;
    va_copy(aq, ap);
    str = va_arg(aq, char *);
    va_end(aq);
    if (*str == '+') {	// a pattern starting with '+' is supposed to be arm
        if (addr & 2) {
            return 0;
        }
        rv = 2;
        str++;
    }
    // XXX this is awfully slow: better build a byte array and a mask array and compare byte by byte
    for (nibble = 4; *str; str++) {
        int ch = *str;
        if (ch == ' ') {
            continue;
        }
        if (ch != '.') {
            static const int tab[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, -1, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };
            int val = -1;
            if (ch >= 'a' && ch <= 'z') {
                ch &= 0xDF;
            }
            if (ch >= '0' && ch <= 'F') {
                val = tab[ch - '0'];
            }
            assert(val != -1);
            if (((buf[0] >> nibble) & 0xF) != val) {
                return 0;
            }
        }
        nibble ^= 4;
        if (nibble) {
            buf++;
        }
    }
    return rv;
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

    ranges = parse_ranges(p);

#if 0
    // POP {R4,PC}
    parse_gadgets(ranges, p, NULL, is_LOAD_R4);
#elif 0
    // MOV Rx, R0 / POP {...PC}
    parse_gadgets(ranges, p, NULL, is_MOV_Rx_R0, 1);
#elif 0
    // MOV R0, Rx / POP {...PC}
    parse_gadgets(ranges, p, NULL, is_MOV_R0_Rx, 1);
#elif 1
    // ADD SP, #? / POP {R7,PC}
    parse_gadgets(ranges, p, NULL, is_ADD_SP, 0x100);
#elif 0
    // POP {R0,PC}
    parse_gadgets(ranges, p, NULL, is_LOAD_R0);
#elif 0
    // POP {R0-R1,PC}
    parse_gadgets(ranges, p, NULL, is_LOAD_R0R1);
#elif 0
    // LDMFD SP!, {R0-R3,PC} // POP {R0-R3,PC}
    parse_gadgets(ranges, p, NULL, is_LOAD_R0R3);
#elif 0
    // POP {R4,R5,PC}
    parse_gadgets(ranges, p, NULL, is_LOAD_R4R5);
#elif 0
    // LDR R0,[R0] / POP {R7,PC}
    parse_gadgets(ranges, p, NULL, is_LDR_R0_R0);
#elif 0
    // STR R0,[R4] / POP {R4,R7,PC}
    parse_gadgets(ranges, p, NULL, is_STR_R0_R4);
#elif 0
    // ADD R0,R1 / POP {R7,PC}
    parse_gadgets(ranges, p, NULL, is_ADD_R0_R1);
#elif 0
    // BLX R4 / POP {R4,R7,PC}
    parse_gadgets(ranges, p, NULL, is_BLX_R4);
#elif 1
    // BLX R4 / ADD SP, #? / POP {...PC}
    parse_gadgets(ranges, p, NULL, is_BLX_R4_SP, 5);
#elif 0
    // MOVS R0, #0 / BX LR
    parse_gadgets(ranges, p, NULL, is_RET_0);
#elif 0
    // CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}
    parse_gadgets(ranges, p, NULL, is_COMPARE);
#elif 0
    // ADDS / BX LR
    parse_gadgets(ranges, p, NULL, is_reg_pivot, 0, 6, 0xF0);
#elif 0
    // ADDS / POP {...PC}
    parse_gadgets(ranges, p, NULL, is_reg_pivot_alt, 0, 6, 0xFF, 3);
#elif 0
    // ldmia r4, ...
    parse_gadgets(ranges, p, NULL, is_ldmia, 4, 3, 0x5000);
#elif 0
    // ldmia.w r0, ...
    parse_gadgets(ranges, p, NULL, is_ldmiaw, 0, 3, 0x5000);
#elif 1
    parse_gadgets(ranges, p, NULL, is_string, "+ 0a f0 14 e8");
#endif

    delete_ranges(ranges);

    while (argc-- > 1) {
        parse_symbols(p, *++argv);
    }

    munmap(p, sz);
    close(fd);
    return 0;
}
#endif
