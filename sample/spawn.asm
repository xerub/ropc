### extern dlclose
### extern dlopen
### extern dlsym
### extern execve
### extern exit
### extern fork
### extern fprintf
### extern printf
### extern __stderrp
### extern waitpid
### extern optind
### extern optreset
### X_MAIN := 0xABCD
### args := &L_vec_9
        dg    0xFF000002                    ; -> PC: POP {R4,PC}
        dg    0x300CC2                      ; -> R4: fork
; R4=fork
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    pid                           ; -> R4
        dd    0                             ; -> R7
; R4=&pid R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=0 R7=0
        dg    0xFF00000F                    ; -> PC: POP {R4,R5,PC}
        du    L_stk_11                      ; -> R4
        du    L_stk_12                      ; -> R5
; R4=L_stk_11 R5=L_stk_12 R7=0
        dg    0xFF00000E                    ; -> PC: CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}
        dd    0                             ; -> R4
        du    L_stk_16                      ; -> R5
        dd    0                             ; -> R7
; R4=0 R5=L_stk_16 R7=0
        dg    0xFF000011                    ; -> PC: LDMIA R0, {...SP...PC}
L_stk_12:
        dd    0                             ; -> R0
        dd    0                             ; -> R4
        du    4 + child                     ; -> SP
        dg    0xFF000003                    ; -> PC: POP {R0,PC}
L_stk_11:
        dd    0                             ; -> R0
        du    L_stk_15                      ; -> R4
        du    4 + L_nxt_10                  ; -> SP
L_nxt_10:
; R0=0 R4=L_stk_15 R5=L_stk_16 R7=0
        dg    0xFF000004                    ; -> PC: POP {R0-R1,PC}
pid     dd    0                             ; -> R0
        dd    1                             ; -> R1
; R0=pid R1=1 R4=L_stk_15 R5=L_stk_16 R7=0
        dg    0xFF000007                    ; -> PC: ADD R0, R1 / POP {R7,PC}
        dd    0                             ; -> R7
; R1=1 R4=L_stk_15 R5=L_stk_16 R7=0
        dg    0xFF00000E                    ; -> PC: CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}
        dd    0                             ; -> R4
        du    L_stk_20                      ; -> R5
        dd    0                             ; -> R7
; R1=1 R4=0 R5=L_stk_20 R7=0
        dg    0xFF000011                    ; -> PC: LDMIA R0, {...SP...PC}
L_stk_16:
        dd    0                             ; -> R0
        dd    0                             ; -> R4
        du    4 + error                     ; -> SP
        dg    0xFF000004                    ; -> PC: POP {R0-R1,PC}
L_stk_15:
        dd    0                             ; -> R0
        dg    0x4289AFF6                    ; -> R4: waitpid
        du    4 + L_nxt_14                  ; -> SP
L_nxt_14:
; R0=0 R1=1 R4=waitpid R5=L_stk_20 R7=0
        dg    0xFF000005                    ; -> PC: POP {R0-R3,PC}
        du    pid                           ; -> R0
        du    status                        ; -> R1
        dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&pid R1=&status R2=0 R3=0 R4=waitpid R5=L_stk_20 R7=0
        dg    0xFF000006                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=&status R2=0 R3=0 R4=waitpid R5=L_stk_20 R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    L_var_17                      ; -> R4
        dd    0                             ; -> R7
; R4=&L_var_17 R5=L_stk_20 R7=0
        dg    0xFF000003                    ; -> PC: POP {R0,PC}
        du    pid                           ; -> R0
; R0=&pid R4=&L_var_17 R5=L_stk_20 R7=0
        dg    0xFF000006                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_var_17 R5=L_stk_20 R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0xC596A359                    ; -> R4: printf
        dd    0                             ; -> R7
; R4=printf R5=L_stk_20 R7=0
        dg    0xFF000005                    ; -> PC: POP {R0-R3,PC}
        du    L_str_0                       ; -> R0
status  dd    0                             ; -> R1
L_var_17 dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=&L_str_0 R1=status R2=L_var_17 R3=0 R4=printf R5=L_stk_20 R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0xB0CAAED2                    ; -> R4: dlopen
        dd    0                             ; -> R7
; R4=dlopen R5=L_stk_20 R7=0
        dg    0xFF000004                    ; -> PC: POP {R0-R1,PC}
        du    L_str_1                       ; -> R0
        dd    5                             ; -> R1
; R0=&L_str_1 R1=5 R4=dlopen R5=L_stk_20 R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    h                             ; -> R4
        dd    0                             ; -> R7
; R4=&h R5=L_stk_20 R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_stk_19                      ; -> R4
        dd    0                             ; -> R7
; R4=L_stk_19 R5=L_stk_20 R7=0
        dg    0xFF00000E                    ; -> PC: CMP R0, #0 / IT EQ / MOVEQ R4, R5 / MOV R0, R4 / POP {R4,R5,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R5
        dd    0                             ; -> R7
; R4=0 R5=0 R7=0
        dg    0xFF000011                    ; -> PC: LDMIA R0, {...SP...PC}
L_stk_20:
        dd    0                             ; -> R0
        dd    0                             ; -> R4
        du    4 + done                      ; -> SP
        dg    0xFF000003                    ; -> PC: POP {R0,PC}
L_stk_19:
        dd    0                             ; -> R0
        dg    0x5B4053F                     ; -> R4: dlsym
        du    4 + L_nxt_18                  ; -> SP
L_nxt_18:
; R0=0 R4=dlsym R5=0 R7=0
        dg    0xFF000004                    ; -> PC: POP {R0-R1,PC}
h       dd    0                             ; -> R0
        du    L_str_2                       ; -> R1
; R0=h R1=&L_str_2 R4=dlsym R5=0 R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    mh                            ; -> R4
        dd    0                             ; -> R7
; R4=&mh R5=0 R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    my_main                       ; -> R4
        dd    0                             ; -> R7
; R4=&my_main R5=0 R7=0
        dg    0xFF000004                    ; -> PC: POP {R0-R1,PC}
mh      dd    0                             ; -> R0
X_MAIN  dd    0xABCD                        ; -> R1
; R0=mh R1=X_MAIN R4=&my_main R5=0 R7=0
        dg    0xFF000007                    ; -> PC: ADD R0, R1 / POP {R7,PC}
        dd    0                             ; -> R7
; R1=X_MAIN R4=&my_main R5=0 R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0xC596A359                    ; -> R4: printf
        dd    0                             ; -> R7
; R1=X_MAIN R4=printf R5=0 R7=0
        dg    0xFF000004                    ; -> PC: POP {R0-R1,PC}
        du    L_str_3                       ; -> R0
my_main dd    0                             ; -> R1
; R0=&L_str_3 R1=my_main R4=printf R5=0 R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0xC3CA884C                    ; -> R4: optind
        dd    0                             ; -> R7
; R4=optind R5=0 R7=0
        dg    0xFF000003                    ; -> PC: POP {R0,PC}
        dd    1                             ; -> R0
; R0=1 R4=optind R5=0 R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0xFBC4A67C                    ; -> R4: optreset
        dd    0                             ; -> R7
; R0=1 R4=optreset R5=0 R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        du    L_ptr_22                      ; -> R4
        dd    0                             ; -> R7
; R0=1 R4=&L_ptr_22 R5=0 R7=0
        dg    0xFF000003                    ; -> PC: POP {R0,PC}
        du    my_main                       ; -> R0
; R0=&my_main R4=&L_ptr_22 R5=0 R7=0
        dg    0xFF000006                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=&L_ptr_22 R5=0 R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
L_ptr_22 dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=L_ptr_22 R5=0 R7=0
        dg    0xFF000004                    ; -> PC: POP {R0-R1,PC}
        dd    1                             ; -> R0
        du    L_vec_4                       ; -> R1
; R0=1 R1=&L_vec_4 R4=L_ptr_22 R5=0 R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        du    rv                            ; -> R4
        dd    0                             ; -> R7
; R4=&rv R5=0 R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0xD9523F3F                    ; -> R4: fprintf
        dd    0                             ; -> R7
; R4=fprintf R5=0 R7=0
        dg    0xFF000005                    ; -> PC: POP {R0-R3,PC}
        dg    0x2104706E                    ; -> R0: __stderrp
        du    L_str_5                       ; -> R1
rv      dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=__stderrp R1=&L_str_5 R2=rv R3=0 R4=fprintf R5=0 R7=0
        dg    0xFF000006                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=&L_str_5 R2=rv R3=0 R4=fprintf R5=0 R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dg    0x67E06670                    ; -> R4: dlclose
        dd    0                             ; -> R7
; R4=dlclose R5=0 R7=0
        dg    0xFF000003                    ; -> PC: POP {R0,PC}
        du    h                             ; -> R0
; R0=&h R4=dlclose R5=0 R7=0
        dg    0xFF000006                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R4=dlclose R5=0 R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=0 R5=0 R7=0
done:
;
        dg    0xFF000003                    ; -> PC: POP {R0,PC}
        dd    0                             ; -> R0
; R0=0
        dg    0x2FB91E                      ; -> PC: exit
;
error:
;
        dg    0xFF000004                    ; -> PC: POP {R0-R1,PC}
        dg    0x2104706E                    ; -> R0: __stderrp
        du    L_str_6                       ; -> R1
; R0=__stderrp R1=&L_str_6
        dg    0xFF000006                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=&L_str_6 R7=0
        dg    0xFF000002                    ; -> PC: POP {R4,PC}
        dg    0xD9523F3F                    ; -> R4: fprintf
; R1=&L_str_6 R4=fprintf R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=0 R7=0
        dg    0xFF000003                    ; -> PC: POP {R0,PC}
        dd    -1                            ; -> R0
; R0=-1 R4=0 R7=0
        dg    0x2FB91E                      ; -> PC: exit
;
child:
;
        dg    0xFF000003                    ; -> PC: POP {R0,PC}
        du    args                          ; -> R0
; R0=&args
        dg    0xFF000006                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R7=0
        dg    0xFF000002                    ; -> PC: POP {R4,PC}
        du    L_var_23                      ; -> R4
; R4=&L_var_23 R7=0
        dg    0xFF00000D                    ; -> PC: STR R0, [R4] / POP {R4,R7,PC}
        dg    0xB323E700                    ; -> R4: execve
        dd    0                             ; -> R7
; R4=execve R7=0
        dg    0xFF000005                    ; -> PC: POP {R0-R3,PC}
args    du    L_vec_9                       ; -> R0
L_var_23 dd    0                             ; -> R1
        dd    0                             ; -> R2
        dd    0                             ; -> R3
; R0=args R1=L_var_23 R2=0 R3=0 R4=execve R7=0
        dg    0xFF000006                    ; -> PC: LDR R0, [R0] / POP {R7,PC}
        dd    0                             ; -> R7
; R1=L_var_23 R2=0 R3=0 R4=execve R7=0
        dg    0xFF000008                    ; -> PC: BLX R4 / POP {R4,R7,PC}
        dd    0                             ; -> R4
        dd    0                             ; -> R7
; R4=0 R7=0
        dg    0x2FB91E                      ; -> PC: exit
;
L_vec_9:
        du L_str_7
        du L_str_8
        dd 0
L_str_8 db "firstarg", 0
L_str_7 db "program", 0
L_str_6 db "fork failed\n", 0
L_str_5 db "rv = %x", 0
L_vec_4:
        du L_str_1
        dd 0
L_str_3 db "my_main = %x\n", 0
L_str_2 db "_mh_execute_header", 0
L_str_1 db "other_program", 0
L_str_0 db "got status %x for pid %d\n", 0
