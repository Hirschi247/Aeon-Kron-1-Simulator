//
// Created by Simon Arbeit on 03.10.2026.
//

#ifndef KRON_SIM_MAIN_H
#define KRON_SIM_MAIN_H


/* ------------------------------------------------------------
 * Datentypen der Debug-Schnittstelle
 *
 * Bisher nur als Forward Declarations (unvollständige Typen).
 * Der Aufbau wird mit der Implementierung festgelegt.
 * ------------------------------------------------------------ */
typedef struct info_answer      info_answer;
typedef struct state            state;
typedef struct read_reg_req     read_reg_req;
typedef struct read_reg_answer  read_reg_answer;
typedef struct write_reg_req    write_reg_req;
typedef struct read_dmem_req    read_dmem_req;
typedef struct read_dmem_answer read_dmem_answer;
typedef struct write_dmem_req   write_dmem_req;
typedef struct read_imem_req    read_imem_req;
typedef struct read_imem_answer read_imem_answer;
typedef struct write_imem_req   write_imem_req;


/* ============================================================
 * Simulations-Schnittstelle
 *
 * Diese Funktionen werden von der Testbench aufgerufen. Sie
 * kapseln den Simulator und geben die Ergebnisse in
 * strukturierten Datentypen zurück.
 * ============================================================ */

/* ============================================================
 * Info: Gibt die Architektur-Informationen zurück
 * PV  = Protokollversion
 * ISA = Instruction Set Architecture Version
 * FCS = Freeze/Cycle-Step verfügbar (fehlt = 0)
 * ============================================================ */
info_answer Info(void);

/* ============================================================
 * State: Gibt den aktuellen Prozessorzustand zurück
 * Gibt ungefähr das wieder, was es bereits in der simcore.h
 * unter typedef struct kron_t gibt.
 * ============================================================ */
state State(void);

/* ============================================================
 * Step: Führt eine Instruktion aus und gibt den neuen Zustand
 * zurück. Wenn der Prozessor bereits angehalten ist, passiert
 * nichts.
 * ============================================================ */
state Step(void);

/* ============================================================
 * Continue: Führt Instruktionen aus, bis der Prozessor
 * angehalten ist. Gibt den neuen Zustand zurück.
 * ============================================================ */
state Continue(void);

/* ============================================================
 * Reset: Setzt den Prozessor in den Ausgangszustand zurück.
 * ============================================================ */
state Reset(void);

/* ============================================================
 * ReadReg: Liest den Wert eines oder aller Register aus.
 * ============================================================ */
read_reg_answer ReadReg(read_reg_req req);

/* ============================================================
 * WriteReg: Schreibt einen Wert in ein Register. (R0 ist hardwired 0!)
 * ============================================================ */
int WriteReg(write_reg_req req);

/* ============================================================
 * ReadDmem: Liest aus dem Daten-Speicher aus.
 * ============================================================ */
read_dmem_answer ReadDmem(read_dmem_req req);

/* ============================================================
 * WriteDmem: Schreibt in den Daten-Speicher.
 * ============================================================ */
int WriteDmem(write_dmem_req req);

/* ============================================================
 * ReadImem: Liest aus dem Instruktions-Speicher aus.
 * ============================================================ */
read_imem_answer ReadImem(read_imem_req req);

/* ============================================================
 * WriteImem: Schreibt in den Instruktions-Speicher.
 * ============================================================ */
int WriteImem(write_imem_req req);

#endif //KRON_SIM_MAIN_H
