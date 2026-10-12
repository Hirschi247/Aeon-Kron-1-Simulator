/* ============================================================
 * KRON-1 — Instruction Set Simulator (Golden Reference Model)
 * ============================================================ */

#include "main.h"
#include "simcore/simcore.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ============================================================
 * Programm laden (.mem, ein 36-bit-Hexwort pro Zeile)
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
        k->imem[n++] = (uint64_t)strtoull(p, NULL, 16);
    }
    fclose(f);
    fprintf(stderr, "[sim] %d Instruktionen geladen\n", n);
    return n;
}

/* ============================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Falsche Startargumente. Nutzung: %s [Name der Programmdatei] [max_steps]\n", argv[0]);
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