rope:
        dd    0                             ; initial EBP

%define LG(x) ___gadget %+ x
%define LU(x) ___local %+ x

%macro dg 1
LG(G):  dd %1
%assign G G+1
%endmacro

%macro du 1
LU(U):  dd %1
%assign U U+1
%endmacro

%assign G 0
%assign U 0

%include "rop-x86.asm"

        align 4
        dd    0                             ; begin gadget relocs

%assign R 0
%rep G
        dd    LG(R) - rope
%assign R R+1
%endrep

        dd    0                             ; begin pointer relocs

%assign R 0
%rep U
        dd    LU(R) - rope
%assign R R+1
%endrep

rope_end:
