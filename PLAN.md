# Programming Project II — Task Breakdown
**Total: 20 points · Deadline: May 17, 2026 at 23:59**

---

## 1. Infrastructure (do first)

- **T1.1 (1pt)** — Interactive CLI menu + batch mode (`myProg -b ranges.txt reg.txt out.txt`)
- **T1.2 (1pt)** — Parser for input files (live ranges + registers) and construction of data structures based on the TP graph class

---

## 2. Algorithms (core of the project)

- **T2.1 (4pt)** — Basic: live ranges → webs → interference graph → greedy graph coloring. Do this first — everything else depends on it.
- **T2.2 (3pt)** — Spilling: when coloring fails, spill up to K webs to memory. Justify the selection criterion.
- **T2.3 (3pt)** — Splitting: split up to K webs into sub-webs to reduce interference. Justify the selection criterion.
- **T2.4 (4pt)** — Free: any custom approach, with the only restriction that interfering webs cannot share the same register.

---

## 3. Documentation

- **T1.3 (2pt)** — Doxygen comments throughout all code + time complexity analysis of the main algorithms
- Output files in the correct format (webs + registers + infeasible warning to console)
- Submission zip: `DA2026_PRJ2_TTN_GGN.zip` containing source code, Doxygen-generated HTML docs, and presentation in PDF

---

## 4. Demo

- **T3.1 (2pt)** — 10-minute presentation: show results on the provided datasets, explain the graph class and design decisions, highlight the most challenging aspects
- Supporting PowerPoint, exported as PDF for the zip