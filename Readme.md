# AEON KRON-1 Simulator

Ein in C geschriebener **Instruction Set Simulator** für die **KRON-1**, eine selbst entworfene
16-Bit-CPU mit 36-Bit-Befehlswort und Harvard-Architektur.

Der Simulator lädt ein Maschinenprogramm im `.mem`-Format, führt es Befehl für Befehl aus und gibt
am Ende den Prozessorzustand (Register, LED-Ausgang, Anzahl der Schritte) aus. Er dient als
**Golden Reference Model**: Später sollen Assembler-Ausgaben und eine Hardware-Umsetzung (Verilog)
gegen ihn verglichen werden.

> [!NOTE]
> **Dieses Projekt befindet sich in aktiver Entwicklung.**
> Es ist ein persönliches Lernprojekt, mit dem ich C und Rechnerarchitektur lerne. Noch nicht alle
> Befehle der ISA sind umgesetzt, Schnittstellen können sich ändern, und manche Teile
> (z. B. die Debug-Schnittstelle in `main.h`) sind bisher nur als Gerüst vorhanden.

---

## Das KRON-1-Projekt

Der Simulator ist ein Teil eines größeren Projekts:

| Repo | Inhalt |
|------|--------|
| [kron-1-isa](https://github.com/Hirschi247/kron-1-isa) | Befehlssatz als C-Bibliothek – **Single Source of Truth** für Opcodes, Bitfelder und Mnemonics |
| [assembler](https://github.com/Hirschi247/assembler) | Übersetzt KRON-Assembler (`.asm`) in `.mem`-Dateien |
| **Aeon-Kron-1-Simulator** (dieses Repo) | Führt `.mem`-Programme aus |

Assembler und Simulator binden beide die ISA-Bibliothek ein. So ist garantiert, dass sie dasselbe
Befehlsformat verwenden.

---

## Architektur in Kürze

| Parameter | Wert |
|-----------|------|
| Registerbreite | 16 Bit |
| Register | 32 (`R0` fest 0, `R30` Stack-Pointer, `R31` Link-Register) |
| Befehlswort | 36 Bit, feste Breite |
| Befehlsspeicher | 4096 × 36 Bit |
| Datenspeicher | 4096 × 16 Bit |
| Flags | Z, N, V |
| I/O | Memory-mapped, LED-Ausgang an Adresse `0x0F00` |

Die vollständige Spezifikation steht im
[ISA-Repo (KRON-ISA.md)](https://github.com/Hirschi247/kron-1-isa/blob/main/KRON-ISA.md).

### Aktueller Stand

- [x] Laden von `.mem`-Programmen
- [x] Dekodierung aller drei Befehlsklassen (ALU, Memory, Jump)
- [x] `LI`, `ADD`/`ADDI`, `SUB`/`SUBI`, `STORE`, `JMP`/`JMPR`, `HALT`
- [x] Flags Z, N, V bei `ADD`
- [x] Memory-mapped LED-Ausgang
- [x] Trace-Ausgabe pro Schritt (Grundlage für späteren Vergleich mit der Hardware)
- [ ] Flags bei `SUB`, Logik- und Shift-Befehle (`AND`, `OR`, `XOR`, `NOT`, `SHL`, …)
- [ ] Bedingte Sprünge (`EQ`, `NE`, `LT`, `LE`, `GT`, `GE`)
- [ ] `LOAD`, Unterprogramme mit Link-Register und Stack
- [ ] Debug-Schnittstelle (Step, Continue, Register/Speicher lesen und schreiben) für ein TUI-Frontend

---

## Bauen

### Voraussetzungen

- C-Compiler mit C11-Unterstützung (GCC, Clang oder MSVC)
- CMake ≥ 4.1
- **Das [ISA-Repo](https://github.com/Hirschi247/kron-1-isa)** – der Simulator wird dagegen gelinkt

### Ordnerstruktur

Die `CMakeLists.txt` erwartet das ISA-Repo im Ordner **`isa` direkt neben** diesem Repo:

```
KRON1/
├── isa/                      ← git clone https://github.com/Hirschi247/kron-1-isa.git isa
└── Aeon-Kron-1-Simulator/    ← dieses Repo
```

Also zum Beispiel:

```bash
git clone https://github.com/Hirschi247/kron-1-isa.git isa
```

```bash
git clone https://github.com/Hirschi247/Aeon-Kron-1-Simulator.git
```

### Kompilieren

```bash
cmake -S Aeon-Kron-1-Simulator -B Aeon-Kron-1-Simulator/build
```

```bash
cmake --build Aeon-Kron-1-Simulator/build
```

Beim Konfigurieren werden die Beispielprogramme aus `programs/` automatisch in den Build-Ordner
kopiert.

---

## Benutzung

```
AEON_KRON_1 <programm.mem> [max_steps]
```

| Argument | Bedeutung |
|----------|-----------|
| `programm.mem` | Das auszuführende Programm |
| `max_steps` | Optional: maximale Anzahl ausgeführter Befehle (Standard: `100`). Schützt vor Endlosschleifen in Programmen ohne `HALT`. |

Beispiel (aus dem Build-Ordner):

```bash
./AEON_KRON_1 test.mem
```

Der Simulator läuft, bis ein `HALT` erreicht ist oder `max_steps` erreicht wurde. Für jeden
ausgeführten Befehl gibt er eine Trace-Zeile aus (PC, Befehlswort, Flags, LED), am Ende den
Zustand – alle Register ungleich 0 und den Wert des LED-Ausgangs:

```
[sim] <n> Instruktionen geladen
<pc> <befehl> | Z<z> N<n> V<v> | LED <hex>
...
[sim] Ende nach <steps> Schritten, LED=<hex>
  R1  = <hex>
  R2  = <hex>
  ...
```

### Das `.mem`-Format

Eine Textdatei mit **einem 36-Bit-Befehl pro Zeile** als Hexzahl (9 Stellen). Leere Zeilen und Zeilen,
die mit `#` oder `/` beginnen, werden ignoriert.

```
408200000
408400001
004210002
```

`.mem`-Dateien kann man von Hand schreiben oder mit dem
[KRON-Assembler](https://github.com/Hirschi247/assembler) aus `.asm`-Quelltext erzeugen.

### Beispielprogramme

Im Ordner [`programs/`](programs) liegen kleine Testprogramme. `kombi.asm` ist ein größerer
kombinierter Test für Assembler und Simulator, `kombi_expected.mem` die erwartete Assembler-Ausgabe
dazu.

---

## Projektstruktur

```
├── main.c              Einstiegspunkt: Programm laden, Simulation ausführen, Zustand ausgeben
├── main.h              Geplante Debug-Schnittstelle (noch Gerüst)
├── simcore/
│   ├── simcore.h       Prozessorzustand (kron_t), dekodierte Instruktion, Architektur-Konstanten
│   └── simcore.c       Fetch, Decode, Execute
├── programs/           Beispielprogramme (.mem / .asm)
└── CMakeLists.txt
```

---

## Ausblick

Aktuell in der Konzeption ist eine **Debug-Schnittstelle** mit einem Frontend als **TUI**
(Text User Interface). Damit soll man Programme Schritt für Schritt ausführen und dabei Register
und Speicher live beobachten und verändern können. Erste Funktionsdeklarationen dafür liegen
bereits in `main.h`.

Langfristig soll die KRON-1 auch als Hardware auf einem FPGA laufen. Der Simulator dient dann als
Referenz, gegen die die Verilog-Umsetzung getestet wird.
