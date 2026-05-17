# Programming Project II — Compiler Register Allocation

**DA — Análise e Síntese de Algoritmos · FEUP · Spring 2026 · L.EIC016**

A command-line tool that assigns physical registers to program variables
using **graph-coloring register allocation** on top of an intermediate
3-address-code representation. Live ranges read from an input file are
merged into webs (per-variable groups of overlapping live ranges), an
interference graph is built using the lecture-provided `Graph<T>` data
structure, and one of four allocation algorithms maps webs to registers
(or to memory if coloring is infeasible).

| | |
|--|--|
| Course | Análise e Síntese de Algoritmos (DA) |
| Institution | FEUP — Faculdade de Engenharia da Universidade do Porto |
| Semester | Spring 2026 |
| Turma | T14 |
| Group | G4 |
| Authors | Daniel Rama · Inês Afonso · Pedro Lopes |
| Deadline | May 17, 2026 |

---

## Table of contents

1. [Building](#building)
2. [Running](#running)
3. [Input file formats](#input-file-formats)
4. [Output file format](#output-file-format)
5. [Algorithms](#algorithms)
6. [Project layout](#project-layout)
7. [Sample datasets](#sample-datasets)
8. [Time-complexity summary](#time-complexity-summary)
9. [Documentation (Doxygen)](#documentation-doxygen)
10. [Submission](#submission)

---

## Building

The project uses CMake (>= 3.10) and a C++17 compiler.

```bash
mkdir -p build && cd build
cmake ..
make
```

This produces `build/myProg`. Convenience symlink at the project root:

```bash
ln -sf build/myProg myProg
```

so that `./myProg ...` works from the top-level directory.

---

## Running

### Batch mode

```bash
./myProg -b <ranges.txt> <registers.txt> <output.txt>
```

- `ranges.txt` — live-range descriptions (see [Input file formats](#input-file-formats)).
- `registers.txt` — register count + algorithm selection.
- `output.txt` — destination file for the allocation result.

Status / error messages go to **stderr**; the structured allocation
result goes to `output.txt`. Exit codes:

| Code | Meaning |
|:----:|---------|
| 0 | Successful, feasible allocation |
| 1 | Parse error or infeasible allocation |
| 2 | Wrong number of CLI arguments |

### Interactive mode

Launching `./myProg` with no arguments opens the menu:

```
=== Compiler Register Allocation Tool ===
Interactive mode. Type 0 to exit.

  1) Set ranges file
  2) Set registers file
  3) Set output file
  4) Show current settings
  5) Load / parse input files
  6) View parsed data
  7) Run register allocation
  8) Show interference graph (ASCII)
  9) Export interference graph (DOT)
  0) Exit
```

Typical demo flow:

```
> 1   ranges/ranges6.txt
> 2   registers/registers3.txt
> 5   (parse)
> 8   (ASCII view of the graph)
> 9   graph.dot          (export)
> 7   (run allocator + write output)
> 0   (exit)
```

Render the exported DOT file with Graphviz:

```bash
dot -Tpng graph.dot -o graph.png
xdg-open graph.png
```

---

## Input file formats

### Live-ranges file (`ranges*.txt`)

Each non-comment, non-blank line declares one live range:

```
<variable_name>: <tok1>, <tok2>, ...
```

where every `tokN` is a non-negative integer line number optionally
suffixed with `+` (definition / start of range) and/or `-` (last use /
end of range). Comments begin with `#`. Multiple lines per variable are
allowed — each becomes a separate live range that may later be merged
into a web with other ranges of the SAME variable that share at least
one program point.

Example (provided dataset `ranges/ranges1.txt`):

```
# this line is just a comment to be ignored
sum: 7+,8,9,10-
i: 1+,2,3,4,5,6-
i: 9+,10,11,12-
i: 12+,13,14-
i: 20+,11,12-
```

### Registers / algorithm file (`registers*.txt`)

Two key-value entries (order is irrelevant, comments allowed):

```
registers: <N>            # number of physical registers
algorithm: <kind>[, K]    # kind ∈ {basic, spilling, splitting, free}
                          # K required for spilling/splitting
```

Examples:

```
registers: 2
algorithm: basic
```

```
registers: 2
algorithm: spilling, 1     # allow at most 1 spilled web
```

```
registers: 1
algorithm: free            # hybrid, no parameter
```

---

## Output file format

Per §3.5 of the project handout:

```
# Total number of webs followed by the listing of the program points of each one
# program points in each web are sorted in ascending order
webs: 3
web0: 7+,8,9,10-
web1: 1+,2,3,4,5,6-
web2: 9+,10,11,12,13,14-,20+
# Total number of registers used, followed by assignment to webs
registers: 2
r0: web1
r0: web2
r1: web0
```

A point that is BOTH defined and last-used at the same line (e.g. the
classic `i = i + 1` idiom in a merged web) appears WITHOUT any marker
because the web flows through that point.

If the allocation is **infeasible**, the `registers:` count is `0` and
every web is mapped to memory:

```
registers: 0
M: web0
M: web1
M: web2
```

A warning is also issued to stderr in that case.

---

## Algorithms

The `algorithm:` entry in the registers file picks one of four variants.

### 1. `basic` — straight graph coloring

Chaitin-style simplify/select pass. Iterates `N` from `1` to `K` and
returns the smallest `N` for which the interference graph can be
colored without spilling. Reports **infeasible** if even `N = K` fails.

### 2. `spilling, K` — coloring with up to K spills

Wraps `basic` in an outer loop. On each failure, picks the
**highest-degree** still-active web, commits it to memory, and retries
— up to `K` spills.

> **Rationale:** removing the most-connected node is the single change
> that most decreases the chromatic-number lower bound, so it has the
> best chance of unblocking the next coloring attempt.

### 3. `splitting, K` — coloring with up to K split webs

Same as basic, but on failure picks the **multi-sub-web web with the
highest degree** and breaks it into its atomic sub-webs (one Web per
originating input live range). Up to `K` splits.

> **Rationale:** only webs that are mergers of several sub-webs are
> candidates for splitting; among them, the most-connected one is the
> one whose decomposition is most likely to reduce the global chromatic
> number.

### 4. `free` — hybrid split-then-spill (T2.4)

No numeric parameter. On every coloring failure:

1. Look for a splittable web (≥ 2 sub-webs). If any exists, pick the
   one with the highest degree and **split** it.
2. Otherwise, pick the highest-degree atomic web and **spill** it.

The loop is bounded by `2N + 2` iterations where `N` is the total
number of input sub-webs — every iteration either grows the group list
or marks one sub-web as spilled, both monotonically.

> **Rationale:** splitting keeps the value in registers, so it is
> "cheaper" than spilling (no extra load/store instructions). We
> therefore try splitting first and only fall back to spilling once a
> problematic web cannot be decomposed any further.

---

## Project layout

```
Projeto2/
├── CMakeLists.txt              CMake build script (C++17)
├── Doxyfile                    Doxygen configuration
├── README.md                   this file
├── PLAN.md                     internal task breakdown
├── ESTATISTICAS.md             empirical results tables
├── ESTRUTURA_CLASSES.md        data-structure design notes
│
├── include/                    public headers
│   ├── allocator.h             4 allocation algorithms + dispatcher
│   ├── menu.h                  interactive + batch entry points
│   ├── output.h                output formatter
│   ├── parser.h                parseRanges / parseRegisters
│   ├── types.h                 Variable, LiveRange, RegisterConfig, ...
│   ├── web.h                   SubWeb, Web, interference predicate, graph builder
│   └── data_structures/        provided TP classes (used unchanged)
│       ├── Graph.h
│       └── MutablePriorityQueue.h
│
├── src/                        implementation files (1:1 with headers)
│   ├── allocator.cpp
│   ├── main.cpp
│   ├── menu.cpp
│   ├── output.cpp
│   ├── parser.cpp
│   └── web.cpp
│
├── doc/                        Doxygen sources
│   └── mainpage.md             Doxygen front page
│
├── ranges/                     ready-to-use live-range datasets
│   ├── ranges1.txt … ranges6.txt        (provided by the handout)
│   ├── ranges_split_clean.txt            (custom — exercises splitting)
│   └── ranges_split_demo.txt
│
├── registers/                  ready-to-use register/algorithm configs
│   ├── registers1.txt registers2.txt registers3.txt   (provided)
│   ├── basic_K2.txt                                   (K=2, basic)
│   ├── spill_K2.txt                                   (K=2, spilling, 1)
│   ├── split_K1.txt split_K2.txt                      (K=1/2, splitting, 1)
│   ├── free_K1.txt free_K2.txt                        (K=1/2, free)
│
├── basic/basic/                ORIGINAL course material (datasets), untouched
│   ├── ranges/   (ranges1..6.txt)
│   ├── registers/ (registers1..3.txt)
│   └── README.md
│
├── tests/                      curated demo scenarios (mirrors ranges/registers)
│   ├── inputs/
│   └── configs/
│
├── build/                      CMake output (gitignored)
└── DA2026_PRJ2_T14_G4/         FINAL submission folder
    ├── Code/                   (CMakeLists, Doxyfile, include/, src/, doc/)
    ├── Documentation/          (Doxygen-generated HTML)
    └── apresentacao_p2.pdf
```

---

## Sample datasets

### Standard datasets (from the handout)

All run with `algorithm: basic` and the K value of the paired
`registers*.txt` file.

| Dataset | K | Webs | Edges | Registers used | Status |
|---------|:-:|:----:|:-----:|:--------------:|:------:|
| `ranges1` | 2 | 3 | 1 | 2 | ✓ |
| `ranges2` | 2 | 2 | 1 | 2 | ✓ |
| `ranges3` | 2 | 2 | 1 | 2 | ✓ |
| `ranges4` | 1 | 4 | 0 | 1 | ✓ |
| `ranges5` | 1 | 3 | 0 | 1 | ✓ |
| `ranges6` | 3 | 5 | 5 | 3 | ✓ |

### Custom split-friendly case (`ranges_split_clean`, K = 2)

A variant of `ranges6` where the variable `e` is split into two input
ranges that share a clean handoff at line 9, so it fuses into a single
web. The merged graph is a 5-cycle (chromatic 3); after splitting back
the graph becomes a 6-vertex bipartite chain (chromatic 2).

| Algorithm | Webs | Splits | Spills | Registers | Status |
|-----------|:----:|:------:|:------:|:---------:|:------:|
| `basic` | 5 | 0 | 0 | — | INFEASIBLE |
| `spilling, 1` | 5 | 0 | 1 | 2 | ✓ |
| `splitting, 1` | 6 | 1 | 0 | 2 | ✓ |
| `free` | 6 | 1 | 0 | 2 | ✓ |

The `free` algorithm picks the cheapest action automatically: 1 split,
0 spills, 2 registers.

### Reproducing the table

```bash
./myProg -b ranges/ranges1.txt registers/registers2.txt out.txt
./myProg -b ranges/ranges6.txt registers/registers3.txt out.txt
./myProg -b ranges/ranges_split_clean.txt registers/free_K2.txt out.txt
```

---

## Time-complexity summary

Notation: `V` = number of variables, `R` = total number of input live
ranges, `P` = average points per live range, `W` = number of webs after
merging, `K` = number of available registers, `K_param` = numeric
parameter of `spilling` / `splitting`, `N` = total number of input
sub-webs.

| Function | Complexity |
|----------|------------|
| `parseRanges`, `parseRegisters` | O(L) tokens |
| `buildSubWebs` | O(R · P log P) |
| `mergeSubWebs` | O(V · R² · P) worst case |
| `websInterfere` | O(\|A.points\| + \|B.points\|) |
| `buildInterferenceGraph` | O(W² · P) |
| `allocateBasic` | O(K · (W³ + W² · P)) |
| `allocateSpilling` | O(K_param · (W³ + W² · P)) |
| `allocateSplitting` | O(K_param · (W'³ + W'² · P)) |
| `allocateFree` | O(N · (W'³ + W'² · P)), loop bounded by 2N + 2 |

---

## Documentation (Doxygen)

Regenerate the HTML documentation from the project root:

```bash
doxygen Doxyfile
```

This produces `doc/html/`. The entry page is `doc/html/index.html`.

```bash
xdg-open doc/html/index.html
```

The Doxyfile generates **zero warnings** on the current code base, and
every public function, type and file is documented with `@brief`,
parameter descriptions and time complexity where applicable.
