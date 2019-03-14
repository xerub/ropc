%define LU(x) ___u %+ x

%macro dg 1
%assign T U+1
LU(U):  dq (%1) + (0xC000000000000000 | (((LU(T) - LU(U)) / 8) << 48))
%assign U T
%endmacro

%macro du 1
%assign T U+1
LU(U):  dq (%1) + (0x8000000000000000 | (((LU(T) - LU(U)) / 8) << 48))
%assign U T
%endmacro

%assign U 0

rope:
        du    0                             ; initial X29 (also start pointer relocs)

%define dd dq
