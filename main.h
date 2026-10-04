//
// Created by Simon Arbeit on 03.10.2026.
//

#ifndef KRON_SIM_MAIN_H
#define KRON_SIM_MAIN_H

/* ---------- Architektur-Konstanten (aus der Spezifikation) ---------- */

#define IMEM_WORDS   4096      /* 12-bit Adresse, 36 bit breit */
#define DMEM_WORDS   4096      /* 12-bit Adresse, 16 bit breit */
#define NUM_REGS     32

#define ADDR_MASK    0x0FFF    /* nur untere 12 bit indizieren Speicher */

/* I/O-Fenster — MUSS identisch zur Hardware sein */
#define IO_BASE      0x0F00
#define IO_LED       0x0F00

/* Registerrollen (Konvention, ab Stufe 5 relevant) */
#define REG_SP       30
#define REG_LR       31
#include <stdint.h>

/* ---------- Prozessorzustand ---------- */

typedef struct {
    uint16_t reg[NUM_REGS]; //FRAGE: Was bedeuted hier und bei imem und dmem "[]"? Sind das Arrays?
    uint16_t pc;                    /* effektiv 12 bit */

    /* Flags */
    int flag_z;
    int flag_n;
    int flag_v;

    uint64_t imem[IMEM_WORDS];
    uint16_t dmem[DMEM_WORDS];

    /* Peripherie */
    uint16_t led;                   /* untere 6 bit sichtbar */

    int halted;
    uint64_t steps; //FRAGE: Was ist mit Steps gemeint? Ist das die Anzahl der Schritte, die der Prozessor bisher ausgeführt hat?
} kron_t;

/* ---------- Dekodierte Instruktion ---------- */

typedef struct {
    uint64_t raw;

    /* Everywhere the same */
    uint8_t  prefix;                /* [35:34] */
    uint8_t  subopcode;             /* [30:26] */


    /* ALU */
    uint8_t  imm_mode;              /* [33]    */
    uint8_t  suppress;              /* [32]    */
    uint8_t  rd;                    /* [25:21] */
    uint8_t  rs1;                   /* [20:16] */
    uint16_t operand;               /* [4:0] - Register / [15:0] Immediate */

    /* Memory */
    uint8_t  mem_reg;               /* [25:21] — rd bei LOAD / rsrc bei STORE */
    uint8_t  rbase;                 /* [20:16] */
    int16_t  offset;                /* [17:2]  signed */
    uint16_t imm16;                 /* [22:7]  bei LI */

    /* Jump */
    uint8_t  cond;                  /* [29:27] */
    uint8_t  link;                  /* [26]    */
    uint8_t  ind;                   /* [25]    */
    uint16_t jaddr;                 /* [24:13] */
    uint8_t  jreg;                  /* [24:20] */
} decoded_t;

#endif //KRON_SIM_MAIN_H
