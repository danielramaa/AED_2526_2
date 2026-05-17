@mainpage Programming Project II — Compiler Register Allocation

# Overview

This project implements a register-allocation tool for an intermediate
3-address-code representation, following the **graph-coloring** approach
described in §2 of the course handout. Live ranges read from an input file
are merged into **webs** (per-variable groups of overlapping live ranges),
an interference graph is built on top of those webs using the
@ref Graph "TP `Graph<T>` data structure" from the lectures, and one of
four allocation algorithms is run on the graph to map webs to physical
registers (or to memory if the coloring is infeasible).

# Tool usage

Two entry points:

- **Interactive menu** (default): runs a text-based menu that lets the user
  set the input files, parse them, inspect the parsed data and run the
  allocation.
  @code
  ./myProg
  @endcode

- **Batch mode** (matches §4.1 of the handout):
  @code
  ./myProg -b ranges.txt registers.txt allocation.txt
  @endcode
  Errors are written to standard error; the result of the allocation goes
  to the output file in the format described in §3.5 of the handout. When
  the allocation is infeasible, a warning is also emitted to the console.

# Pipeline

1. **Parsing** — @ref parseRanges reads the live-range file and
   @ref parseRegisters reads the register/algorithm configuration.
2. **Sub-web construction** — @ref buildSubWebs turns every input
   `LiveRange` into one `SubWeb`.
3. **Merging** — @ref mergeSubWebs runs a union-find pass that joins
   sub-webs of the SAME variable whenever they share at least one program
   point.
4. **Web construction** — @ref buildWebsFromGroups produces the final list
   of `Web` objects, each carrying its merged point set with the `+` / `-`
   markers OR-ed together.
5. **Interference graph** — @ref buildInterferenceGraph populates a
   `Graph<int>` (vertex info = web id) using @ref websInterfere as the
   pairwise interference predicate. The interference rule from §2.5 of the
   handout (clean def-vs-last-use handoff exception) is implemented there.
6. **Allocation** — @ref allocate dispatches to one of the four algorithms
   based on the `algorithm:` entry of the configuration file.
7. **Output** — @ref writeAllocation formats the `AllocationResult` to the
   spec format.

# Algorithms

| Variant      | Entry point             | Behaviour |
|--------------|-------------------------|-----------|
| `basic`      | @ref allocateBasic      | Plain Chaitin-style simplify/select coloring, iterating N from 1 to K to find the minimum register count. Infeasible if even N=K cannot color the graph. |
| `spilling`   | @ref allocateSpilling   | Same coloring, wrapped in an outer loop that spills the **highest-degree** still-active web on each failure, up to K spills allowed by the input file. |
| `splitting`  | @ref allocateSplitting  | Same coloring, but on failure picks the **multi-sub-web web with the highest degree** and breaks it into its atomic sub-webs, up to K splits. |
| `free`       | @ref allocateFree       | Hybrid (T2.4): on each failure, picks the highest-degree non-spilled web; if it has ≥2 sub-webs **split** it, otherwise **spill** it. No numeric parameter — the heuristic keeps going until colorable or every sub-web is in memory. |

# Time-complexity summary

Let `V` = number of variables, `R` = total number of input live ranges,
`P` = average points per live range, `W` = number of webs after merging,
`K` = number of available registers, and `K_param` = the numeric parameter
of the spilling/splitting variants.

- @ref parseRanges and @ref parseRegisters — **O(L)** where L is the total
  number of tokens in the input file.
- @ref buildSubWebs — **O(R · P log P)** (one sort per sub-web).
- @ref mergeSubWebs — **O(V · R² · P)** worst case across same-variable
  pairs; usually much less because R per variable is small.
- @ref websInterfere — **O(|A.points| + |B.points|)** by walking both
  sorted lists in parallel.
- @ref buildInterferenceGraph — **O(W² · P)**.
- @ref allocateBasic — **O(K · (W³ + W² · P))** in the worst case
  (K iterations of the simplify/select pass).
- @ref allocateSpilling — **O(K_param · (W³ + W² · P))**.
- @ref allocateSplitting — **O(K_param · (W'³ + W'² · P))** where W' is
  the current web count after the running splits.
- @ref allocateFree — **O(N · (W'³ + W'² · P))** where N is the total
  number of input sub-webs (loop is bounded by 2N+2 iterations).

# Project layout

- `include/` — public headers (one per module).
- `include/data_structures/Graph.h` — the TP graph class, used as-is per
  §4.1.2 of the handout.
- `src/` — implementation files matched 1:1 with the headers.
- `basic/basic/` — sample inputs provided with the project handout.
