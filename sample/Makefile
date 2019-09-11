all: rop-x86 rop-x86_64 rop-arm rop-arm64


rop-x86: wrap-x86.asm rop-x86.asm
	nasm -o $@ -O6 $<

rop-x86.asm: rop-x86.i
	./ropc-x86 -o $@ -O2 -g -n -c dyld_shared_cache_i386 $<

rop-x86.i: rope.c
	gcc -o $@ -DBITS=32 -E $<


rop-x86_64: wrap-x86_64.asm rop-x86_64.asm
	nasm -o $@ -O6 $<

rop-x86_64.asm: rop-x86_64.i
	./ropc-x86_64 -o $@ -O2 -g -n -c dyld_shared_cache_x86_64 $<

rop-x86_64.i: rope.c
	gcc -o $@ -DBITS=64 -E $<


rop-arm: wrap-arm.asm rop-arm.asm
	nasm -o $@ -O6 $<

rop-arm.asm: rop-arm.i
	./ropc-arm -o $@ -O2 -g -n -c dyld_shared_cache_armv7 $<

rop-arm.i: rope.c
	gcc -o $@ -DBITS=32 -E $<


rop-arm64: wrap-arm64.asm rop-arm64.asm
	nasm -o $@ -O6 $<

rop-arm64.asm: rop-arm64.i
	./ropc-arm64 -o $@ -O2 -g -n -c dyld_shared_cache_arm64 $<

rop-arm64.i: rope.c
	gcc -o $@ -DBITS=64 -DARM64 -E $<


clean:
	-$(RM) rop-x86.asm rop-x86_64.asm rop-arm.asm rop-arm64.asm
	-$(RM) rop-x86.i rop-x86_64.i rop-arm.i rop-arm64.i

distclean: clean
	-$(RM) rop-x86 rop-x86_64 rop-arm rop-arm64
