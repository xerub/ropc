ROP = simple

CC = clang
CFLAGS = -arch arm64 -isysroot /usr/local/share/SDKs/iPhoneOS7.0.sdk
CFLAGS += -Wall -W -pedantic
CFLAGS += -O2
CFLAGS += -DTEST

LD = clang
LDFLAGS = -arch arm64 -isysroot /usr/local/share/SDKs/iPhoneOS7.0.sdk
LDLIBS =

SOURCES = \
	pivot1.c

OBJECTS = $(SOURCES:.c=.o)

.c.o:
	$(CC) -o $@ $(CFLAGS) -c $<

all: pivot1

pivot1: $(OBJECTS)
	$(LD) -o $@ $(LDFLAGS) $^ $(LDLIBS) && ldid -Sent.xml $@

pivot1.c: rope.inc

rope.inc: rope
	xxd -i $< > $@

rope: wrap64.asm
	nasm -o $@ -O6 $<

wrap64.asm: wrap64_head.asm $(ROP).asm wrap64_foot.asm
	cat $^ > $@

$(ROP).asm: $(ROP).c
	../ropc -c dyld_shared_cache_arm64 -O2 -g $< | sed 's|\(L_str_.*\)\\n\", 0|\1\", 10, 0|' > $@

clean:
	-$(RM) $(OBJECTS) $(ROP).asm rope.inc rope

distclean: clean
	-$(RM) wrap64.asm
	-$(RM) pivot1
