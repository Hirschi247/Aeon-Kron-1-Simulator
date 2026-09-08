/* ============================================================
 * KRON-3 — Instruction Set Simulator (Golden Reference Model)
 *
 * Stufe 1 vollständig: LI, ADD, STORE, JMP, HALT
 * Spätere Stufen sind als TODO markiert.
 *
 * Bauen:  cc -Wall -Wextra -O2 -o kron_sim kron_sim.c
 * Nutzen: ./kron_sim programm.mem [max_steps]
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ---------- Architektur-Konstanten (aus der Spezifikation) ---------- */

#define IMEM_WORDS   4096      /* 12-bit Adresse, 32 bit breit */
#define DMEM_WORDS   4096      /* 12-bit Adresse, 16 bit breit */
#define NUM_REGS     32

#define ADDR_MASK    0x0FFF    /* nur untere 12 bit indizieren Speicher */

/* I/O-Fenster — MUSS identisch zur Hardware sein */
#define IO_BASE      0x0F00
#define IO_LED       0x0F00

/* Registerrollen (Konvention, ab Stufe 5 relevant) */
#define REG_SP       30
#define REG_LR       31

/* ---------- Prozessorzustand ---------- */

typedef struct {
    uint16_t reg[NUM_REGS]; //FRAGE: Was bedeuted hier und bei imem und dmem "[]"? Sind das Arrays?
    uint16_t pc;                    /* effektiv 12 bit */

    /* Flags */
    int flag_z;
    int flag_n;
    int flag_v;

    uint32_t imem[IMEM_WORDS];
    uint16_t dmem[DMEM_WORDS];

    /* Peripherie */
    uint16_t led;                   /* untere 6 bit sichtbar */

    int halted;
    uint64_t steps; //FRAGE: Was ist mit Steps gemeint? Ist das die Anzahl der Schritte, die der Prozessor bisher ausgeführt hat?
} kron_t;

/* ---------- Dekodierte Instruktion ---------- */

typedef struct {
    uint32_t raw;

    uint8_t  prefix;                /* [31:30] */

    /* ALU */
    uint8_t  imm_mode;              /* [29]    */
    uint8_t  suppress;              /* [28]    */
    uint8_t  funct5;                /* [27:23] */
    uint8_t  rd;
    uint8_t  rs1;
    uint8_t  rs2;
    int16_t  imm13;                 /* signed, aus [12:0] */

    /* Memory */
    uint8_t  mem_funct;             /* [29:28] */
    uint8_t  mem_reg;               /* [27:23] — rd bei LOAD, rsrc bei STORE */
    uint8_t  rbase;                 /* [22:18] */
    int16_t  offset;                /* [17:2]  signed */
    uint16_t imm16;                 /* [22:7]  bei LI */

    /* Jump */
    uint8_t  cond;                  /* [29:27] */
    uint8_t  link;                  /* [26]    */
    uint8_t  ind;                   /* [25]    */
    uint16_t jaddr;                 /* [24:13] */
    uint8_t  jreg;                  /* [24:20] */
} decoded_t;

/* ---------- Prefixe ---------- */

enum { PFX_ALU = 0, PFX_MEM = 1, PFX_JMP = 2, PFX_EXT = 3 };

/* ---------- ALU-Funct-Codes (5 bit) ---------- */

enum {
    F_NOP  = 0x00,
    F_ADD  = 0x01,
    F_SUB  = 0x02,
    F_AND  = 0x03,
    F_OR   = 0x04,
    F_XOR  = 0x05,
    F_NOT  = 0x06,
    F_SHL  = 0x07,
    F_SHR  = 0x08,
    /* 0x09..0x1E frei — SAR, ROL, ROR, MUL, ADC, SBC ... */
    F_HALT = 0x1F
};

/* ---------- Memory-Funct-Codes (2 bit) ---------- */

enum { M_LOAD = 0, M_STORE = 1, M_LI = 2, M_RSVD = 3 };

/* ---------- Condition-Codes (3 bit) ---------- */

enum {
    C_ALWAYS = 0, C_EQ = 1, C_NE = 2, C_LT = 3,
    C_LE = 4, C_GT = 5, C_GE = 6, C_RSVD = 7
};

/* ============================================================
 * Hilfsfunktionen
 * ============================================================ */

/* Bitfeld [hi:lo] extrahieren */
static inline uint32_t bits(uint32_t w, int hi, int lo) {
    return (w >> lo) & ((1u << (hi - lo + 1)) - 1u);
} //FRAGE: Erkläre diese methode bitte genauer

/* Vorzeichenerweiterung aus n bit */
static inline int32_t sign_extend(uint32_t v, int n) {
    uint32_t m = 1u << (n - 1);
    return (int32_t)((v ^ m) - m);
} //FRAGE: Erkläre diese methode bitte genauer

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

static void decode(uint32_t w, decoded_t *d) {
    memset(d, 0, sizeof(*d));
    d->raw    = w;
    d->prefix = bits(w, 31, 30);

    /* ALU */
    d->imm_mode = bits(w, 29, 29);
    d->suppress = bits(w, 28, 28);
    d->funct5   = bits(w, 27, 23);
    d->rd       = bits(w, 22, 18);
    d->rs1      = bits(w, 17, 13);
    d->rs2      = bits(w, 12,  8);
    d->imm13    = (int16_t)sign_extend(bits(w, 12, 0), 13);

    /* Memory */
    d->mem_funct = bits(w, 29, 28);
    d->mem_reg   = bits(w, 27, 23);
    d->rbase     = bits(w, 22, 18);
    d->offset    = (int16_t)bits(w, 17, 2);      /* 16 bit, bereits signed-breit */
    d->imm16     = (uint16_t)bits(w, 22, 7);

    /* Jump */
    d->cond  = bits(w, 29, 27);
    d->link  = bits(w, 26, 26);
    d->ind   = bits(w, 25, 25);
    d->jaddr = bits(w, 24, 13);
    d->jreg  = bits(w, 24, 20);
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
        case C_ALWAYS: return 1;
        /* TODO Stufe 3: restliche Condition-Codes
        case C_EQ: return  k->flag_z;
        case C_NE: return !k->flag_z;
        case C_LT: return  nv;
        case C_LE: return  k->flag_z || nv;
        case C_GT: return !k->flag_z && !nv;
        case C_GE: return !nv;
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
    uint16_t b = d->imm_mode ? (uint16_t)d->imm13 : k->reg[d->rs2];
    uint16_t res = 0;
    int writes_back = 1;

    switch (d->funct5) {
        case F_NOP:
            return;                       /* keine Flags, kein Writeback */

        case F_HALT:
            k->halted = 1;
            return;

        case F_ADD:
            res = (uint16_t)(a + b);
            set_flags_add(k, a, b, res);
            break;

        /* TODO Stufe 2:
         * case F_SUB: res = a - b; set_flags_sub(...); break;
         * case F_AND: res = a & b; set_flags_logic(k, res); break;
         * case F_OR:  res = a | b; set_flags_logic(k, res); break;
         * case F_XOR: res = a ^ b; set_flags_logic(k, res); break;
         * case F_NOT: res = ~a;    set_flags_logic(k, res); break;
         * case F_SHL: res = a << (b & 0xF); set_flags_logic(k, res); break;
         * case F_SHR: res = a >> (b & 0xF); set_flags_logic(k, res); break;
         */

        default:
            fprintf(stderr, "[sim] ALU funct 0x%02X nicht implementiert @ PC=%03X\n",
                    d->funct5, k->pc);
            k->halted = 1;
            return;
    }

    (void)set_flags_logic;                /* bis Stufe 2 ungenutzt */

    if (d->suppress) writes_back = 0;     /* CMP-Mechanik */
    if (writes_back) write_reg(k, d->rd, res);
}

static void exec_mem(kron_t *k, decoded_t *d) {
    switch (d->mem_funct) {
        case M_LI:
            write_reg(k, d->mem_reg, d->imm16);
            break;

        case M_STORE: {
            uint16_t addr = (uint16_t)(k->reg[d->rbase] + d->offset);
            mem_write(k, addr, k->reg[d->mem_reg]);
            break;
        }

        /* TODO Stufe 3:
        case M_LOAD: {
            uint16_t addr = (uint16_t)(k->reg[d->rbase] + d->offset);
            write_reg(k, d->mem_reg, mem_read(k, addr));
            break;
        }
        */

        default:
            fprintf(stderr, "[sim] MEM funct %u nicht implementiert @ PC=%03X\n",
                    d->mem_funct, k->pc);
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

static void trace(kron_t *k, uint16_t pc_at_fetch, uint32_t instr) {
    printf("%03X %08X | Z%d N%d V%d | LED %02X\n",
           pc_at_fetch, instr, k->flag_z, k->flag_n, k->flag_v, k->led);
}

/* ============================================================
 * Ein Schritt = eine Instruktion
 * ============================================================ */

static void step(kron_t *k) {
    uint16_t pc_at_fetch = k->pc & ADDR_MASK;
    uint32_t instr = k->imem[pc_at_fetch];
    decoded_t d;
    int pc_written = 0;

    decode(instr, &d);

    switch (d.prefix) {
        case PFX_ALU: exec_alu(k, &d); break;
        case PFX_MEM: exec_mem(k, &d); break;
        case PFX_JMP: pc_written = exec_jump(k, &d); break;
        case PFX_EXT:
            fprintf(stderr, "[sim] Prefix 11 ist reserviert @ PC=%03X\n", pc_at_fetch);
            k->halted = 1;
            break;
    }

    trace(k, pc_at_fetch, instr);

    if (!pc_written && !k->halted)
        k->pc = (uint16_t)((k->pc + 1) & ADDR_MASK);

    k->steps++;
}

/* ============================================================
 * Programm laden (.mem, ein 32-bit-Hexwort pro Zeile)
 * ============================================================ */

static int load_mem(kron_t *k, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }

    char line[128];
    int n = 0;
    while (fgets(line, sizeof(line), f) && n < IMEM_WORDS) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '/' || *p == '#') continue;
        k->imem[n++] = (uint32_t)strtoul(p, NULL, 16);
    }
    fclose(f);
    fprintf(stderr, "[sim] %d Instruktionen geladen\n", n);
    return n;
}

/* ============================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Nutzung: %s programm.mem [max_steps]\n", argv[0]);
        return 1;
    }

    /* Stufe 1 läuft in einer Endlosschleife — ohne Limit hält der
     * Simulator nie an. Später übernimmt HALT diese Rolle. */
    uint64_t max_steps = (argc > 2) ? strtoull(argv[2], NULL, 10) : 100;

    kron_t k;
    memset(&k, 0, sizeof(k));

    if (load_mem(&k, argv[1]) < 0) return 1;

    while (!k.halted && k.steps < max_steps)
        step(&k);

    fprintf(stderr, "[sim] Ende nach %llu Schritten, LED=%02X\n",
            (unsigned long long)k.steps, k.led);

    /* Registerdump zum Vergleich */
    for (int i = 0; i < NUM_REGS; i++)
        if (k.reg[i]) fprintf(stderr, "  R%-2d = %04X\n", i, k.reg[i]);

    return 0;
}