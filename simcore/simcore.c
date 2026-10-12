//
// Created by Simon Hirsch on 10.10.2026.
//

#include "simcore.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "isa.h"


/* ============================================================
 * Hilfsfunktionen
 * ============================================================ */

/* Bitfeld [hi(pos + witdh):lo(pos)] extrahieren */
static inline uint64_t bits(uint64_t w, uint64_t pos, uint64_t width) {
    uint64_t val = (w >> pos) & ((1ULL << width) - 1);
    return val;
}

/* Vorzeichenerweiterung aus n bit */
static inline int64_t sign_extend(uint64_t v, int n) {
    uint64_t m = 1ULL << (n - 1);
    return (int64_t)((v ^ m) - m);
}

/* EINZIGE Stelle, an der Register geschrieben werden.
 * R0 ist hardwired 0 — Schreibzugriffe werden verworfen. */
static void write_reg(kron_t *k, uint8_t r, uint16_t v) {
    if (r != 0) k->reg[r] = v;
}

/* ============================================================
 * Speicher + I/O-Adressdecoder
 *
 * WICHTIG: Diese Adressabbildung muss exakt der Hardware
 * entsprechen. Ein Adressraum, eine Wahrheit.
 * ============================================================ */

static void mem_write(kron_t *k, uint16_t addr, uint16_t val) {
    addr &= ADDR_MASK;

    if (addr >= IO_BASE) {
        switch (addr) {
            case IO_LED:
                k->led = val & 0x003F;   /* 6 LEDs */
                break;
            default:
                /* reservierter I/O-Bereich: Schreibzugriff verpufft */
                break;
        }
        return;                          /* NICHT ins BSRAM */
    }

    k->dmem[addr] = val;
}

static uint16_t mem_read(kron_t *k, uint16_t addr) {
    addr &= ADDR_MASK;

    if (addr >= IO_BASE) {
        switch (addr) {
            case IO_LED: return k->led;
            default:     return 0;
        }
    }

    return k->dmem[addr];
}

/* ============================================================
 * Decode
 * ============================================================ */

static void decode(uint64_t w, decoded_t *d) {
    memset(d, 0, sizeof(*d));
    d->raw    = w;
    d->prefix = bits(w, KRON_PREFIX_POS, KRON_PREFIX_WIDTH);

    switch (d->prefix) {
        case PREFIX_ALU:
            d->subopcode        = (uint8_t)bits(w, KRON_SUBOPCODE_POS,    KRON_SUBOPCODE_WIDTH);
            d->imm_mode         = (uint8_t)bits(w, KRON_ALU_IMM_BIT,      1);
            d->suppress         = (uint8_t)bits(w, KRON_ALU_SUPPRESS_BIT, 1);
            d->rd               = (uint8_t)bits(w, KRON_RD_POS,           KRON_REGISTER_WIDTH);
            d->rs1              = (uint8_t)bits(w, KRON_RS1_POS,          KRON_REGISTER_WIDTH);
            if (d->imm_mode)
                d->operand      = (uint16_t)bits(w, KRON_OPERAND_POS, KRON_OPERAND_WIDTH);
            else
                d->operand      = (uint8_t)bits(w, KRON_ALU_RS2_POS,  KRON_REGISTER_WIDTH);
            break;

        case PREFIX_MEM:
            d->subopcode = (uint8_t) bits(w, KRON_SUBOPCODE_POS,   KRON_SUBOPCODE_WIDTH);
            d->mem_reg   = (uint8_t) bits(w, KRON_MEM_RD_RSRC_POS, KRON_REGISTER_WIDTH);
            d->rbase     = (uint8_t) bits(w, KRON_MEM_RBASE_POS,   KRON_REGISTER_WIDTH);
            d->offset    = (int16_t) bits(w, KRON_OPERAND_POS,     KRON_OPERAND_WIDTH);
            d->imm16     = (uint16_t)bits(w, KRON_OPERAND_POS,     KRON_OPERAND_WIDTH);
            break;

        case PREFIX_JUMP:
            d->link  = (uint8_t) bits(w, KRON_JUMP_LINK_BIT, 1);
            d->ind   = (uint8_t) bits(w, KRON_JUMP_IND_BIT,  1);
            d->cond  = (uint8_t) bits(w, KRON_JUMP_COND_POS, KRON_COND_WIDTH);
            d->jaddr = (uint16_t)bits(w, KRON_JUMP_ADDR_POS, KRON_JUMP_ADDR_WIDTH);
            d->jreg  = (uint8_t) bits(w, KRON_JUMP_REG_POS,  KRON_REGISTER_WIDTH);
            break;
    }
}

/* ============================================================
 * Flags
 * ============================================================ */

static void set_flags_logic(kron_t *k, uint16_t res) {
    k->flag_z = (res == 0);
    k->flag_n = (res >> 15) & 1;
    k->flag_v = 0;                       /* explizit 0 — keine Stale-Flags */
}

static void set_flags_add(kron_t *k, uint16_t a, uint16_t b, uint16_t res) {
    k->flag_z = (res == 0);
    k->flag_n = (res >> 15) & 1;
    k->flag_v = (((a >> 15) & 1) == ((b >> 15) & 1)) &&
                (((res >> 15) & 1) != ((a >> 15) & 1));
}

/* TODO Stufe 2: set_flags_sub()
 *   V = (a[15] != b[15]) && (res[15] != a[15]) */

static int cond_true(kron_t *k, uint8_t cond) {
    int nv = k->flag_n ^ k->flag_v;
    switch (cond) {
        case JUMP_ALWAYS: return 1;
        /* TODO Stufe 3: restliche Condition-Codes
        case JUMP_EQ: return  k->flag_z;
        case JUMP_NE: return !k->flag_z;
        case JUMP_LT: return  nv;
        case JUMP_LE: return  k->flag_z || nv;
        case JUMP_GT: return !k->flag_z && !nv;
        case JUMP_GE: return !nv;
        */
        default:
            (void)nv;
            fprintf(stderr, "[sim] cond %u nicht implementiert @ PC=%03X\n",
                    cond, k->pc);
            k->halted = 1;
            return 0;
    }
}

/* ============================================================
 * Execute
 * ============================================================ */

static void exec_alu(kron_t *k, decoded_t *d) {
    uint16_t a = k->reg[d->rs1];
    uint16_t b = d->imm_mode ? (uint16_t)d->operand : k->reg[d->operand & KRON_MASK(KRON_REGISTER_WIDTH)];
    uint16_t res = 0;
    int writes_back = 1;

    switch (d->subopcode) {
        case ALU_NOP:
            return;                       /* keine Flags, kein Writeback */

        case ALU_HALT:
            k->halted = 1;
            return;

        case ALU_ADD:
            res = (uint16_t)(a + b);
            set_flags_add(k, a, b, res);
            break;

        case ALU_SUB:
            res = (uint16_t)(a - b);
            /* TODO Stufe 2: set_flags_sub(k, a, b, res); */
            break;

        /* TODO Stufe 2:
         * case ALU_AND: res = a & b; set_flags_logic(k, res); break;
         * case ALU_OR:  res = a | b; set_flags_logic(k, res); break;
         * case ALU_XOR: res = a ^ b; set_flags_logic(k, res); break;
         * case ALU_NOT: res = ~a;    set_flags_logic(k, res); break;
         * case ALU_SHL: res = a << (b & 0xF); set_flags_logic(k, res); break;
         * case ALU_SHR: res = a >> (b & 0xF); set_flags_logic(k, res); break;
         */

        default:
            fprintf(stderr, "[sim] ALU funct 0x%02X nicht implementiert @ PC=%03X\n",
                    d->subopcode, k->pc);
            k->halted = 1;
            return;
    }

    (void)set_flags_logic;                /* bis Stufe 2 ungenutzt */

    if (d->suppress) writes_back = 0;     /* CMP-Mechanik */
    if (writes_back) write_reg(k, d->rd, res);
}

static void exec_mem(kron_t *k, decoded_t *d) {
    switch (d->subopcode) {
        case MEM_LI:
            write_reg(k, d->mem_reg, d->imm16);
            break;

        case MEM_STORE: {
            uint16_t addr = (uint16_t)(k->reg[d->rbase] + d->offset);
            mem_write(k, addr, k->reg[d->mem_reg]);
            break;
        }

        /* TODO Stufe 3:
        case MEM_LOAD: {
            uint16_t addr = (uint16_t)(k->reg[d->rbase] + d->offset);
            write_reg(k, d->mem_reg, mem_read(k, addr));
            break;
        }
        */

        default:
            fprintf(stderr, "[sim] MEM Subopcode %u nicht implementiert @ PC=%03X\n",
                    d->subopcode, k->pc);
            k->halted = 1;
    }
    /* Memory-Instruktionen verändern KEINE Flags */
}

/* Rückgabe: 1 = PC wurde gesetzt, 0 = PC+1 */
static int exec_jump(kron_t *k, decoded_t *d) {
    if (!cond_true(k, d->cond)) return 0;

    uint16_t target = d->ind ? (k->reg[d->jreg] & ADDR_MASK)
                             : (d->jaddr & ADDR_MASK);

    /* Link wird NUR geschrieben, wenn tatsächlich gesprungen wird.
     * PC ist 12 bit, wird zero-extended in R31 abgelegt. */
    if (d->link) write_reg(k, REG_LR, (uint16_t)((k->pc + 1) & ADDR_MASK));

    k->pc = target;
    return 1;
}

/* ============================================================
 * Trace — die Co-Simulation-Schnittstelle
 *
 * Format bewusst simpel und deterministisch: Deine Verilog-
 * Testbench gibt später dieselben Zeilen aus, dann findet
 * `diff` den ersten Abweichungspunkt.
 * ============================================================ */

static void trace(kron_t *k, uint16_t pc_at_fetch, uint64_t instr) {
    printf("%03X %09" PRIX64 " | Z%d N%d V%d | LED %02X\n",
           pc_at_fetch, instr, k->flag_z, k->flag_n, k->flag_v, k->led);
}

/* ============================================================
 * Ein Schritt = eine Instruktion
 * ============================================================ */

void step(kron_t *k) {
    uint16_t pc_at_fetch = k->pc & ADDR_MASK;
    uint64_t instr = k->imem[pc_at_fetch];
    decoded_t d;
    int pc_written = 0;

    decode(instr, &d);

    switch (d.prefix) {
        case PREFIX_ALU: exec_alu(k, &d); break;
        case PREFIX_MEM: exec_mem(k, &d); break;
        case PREFIX_JUMP: pc_written = exec_jump(k, &d); break;
        case 0x03:
            fprintf(stderr, "[sim] Prefix 11 ist reserviert @ PC=%03X\n", pc_at_fetch);
            break;
    }

    trace(k, pc_at_fetch, instr);

    if (!pc_written && !k->halted)
        k->pc = (uint16_t)((k->pc + 1) & ADDR_MASK);

    k->steps++;
}
