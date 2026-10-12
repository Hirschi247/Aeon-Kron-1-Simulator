; ============================================================
; kombi.asm — Kombinierter Test für Assembler + Simulator
;
; Benutzt nur Befehle aus isa_table:
;   LI, ADD, ADDI, SUB, SUBI, STORE, JMP, JMPR, HALT
;
; Testet u.a.: Labels (vorwärts + rückwärts), Label als LI-Operand,
; offset(rbase)-Adressierung, I/O (LED), R0 hardwired, 16-bit-Überlauf,
; imm16 ohne Sign-Extension, "Unterprogramm" per JMPR.
; ============================================================

; ---- 1: Konstanten, R0-Schreibschutz ----
start:  LI    R1, 1             ; 00  R1 = 1
        LI    R2, 1             ; 01  R2 = 1
        ADDI  R0, R0, 7         ; 02  muss verpuffen -> R0 bleibt 0
        LI    R20, 0x100        ; 03  Basisadresse Datenbereich

; ---- 2: Fibonacci, ausgerollt ----
        ADD   R3, R1, R2        ; 04  R3 = 2
        ADD   R4, R2, R3        ; 05  R4 = 3
        ADD   R5, R3, R4        ; 06  R5 = 5
        ADD   R6, R4, R5        ; 07  R6 = 8
        ADD   R7, R5, R6        ; 08  R7 = 13

        STORE R3, 0(R20)        ; 09  MEM[0x100] = 2
        STORE R4, 1(R20)        ; 0A  MEM[0x101] = 3
        STORE R5, 2(R20)        ; 0B  MEM[0x102] = 5
        STORE R6, 3(R20)        ; 0C  MEM[0x103] = 8
        STORE R7, 4(R20)        ; 0D  MEM[0x104] = 13

; ---- 3: "Unterprogramm" ohne CALL ----
        LI    R31, ret1         ; 0E  Rücksprungadresse von Hand (Label als Zahl!)
        JMP   double            ; 0F  Vorwärtsreferenz
ret1:   STORE R8, 0xF00(R0)     ; 10  LED = R8

; ---- 4: Überlauf und Immediates ----
        LI    R9, 0xFFFF        ; 11
        ADDI  R9, R9, 1         ; 12  0xFFFF + 1 -> 0x0000
        SUBI  R10, R0, 1        ; 13  0 - 1      -> 0xFFFF
        ADDI  R11, R0, 0x1234   ; 14  imm16 ohne Sign-Extension -> 0x1234
        SUB   R12, R7, R3       ; 15  13 - 2 = 11
        JMP   end               ; 16  springt über die Falle

trap:   LI    R29, 0xDEAD       ; 17  darf NIE ausgeführt werden
        HALT                    ; 18

end:    HALT                    ; 19

; ---- Unterprogramm: R8 = 2 * R7 ----
double:
        ADD   R8, R7, R7        ; 1A  R8 = 26 = 0x1A
        JMPR  R31               ; 1B  "RET"
