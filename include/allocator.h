/**
 * @file allocator.h
 * @brief Top-level register allocation API.
 *
 * The three algorithm variants requested  are exposed
 * as plain functions taking the parsed @ref InputData and returning an
 * @ref AllocationResult. Web construction and graph colouring live in
 * web.cpp / allocator.cpp respectively.
 */
#ifndef DA_PRJ2_ALLOCATOR_H
#define DA_PRJ2_ALLOCATOR_H

#include "types.h"
#include "web.h"

#include <string>
#include <vector>

/**
 * @brief Allocation decision for a single web.
 *
 * @c registerId is in [0, registersUsed) when a register was assigned and
 * -1 when the web was committed to memory (printed as `M:` in the output
 * file).
 */
struct WebAssignment {
    int webId = 0;
    int registerId = -1;
};

/**
 * @brief Final outcome of an allocator run.
 *
 * Even on infeasible runs the `webs` vector is populated so callers can
 * still emit a syntactically valid output file with every web mapped to
 * memory.
 */
struct AllocationResult {
    bool feasible = false;
    std::vector<Web> webs;
    int registersUsed = 0;
    std::vector<WebAssignment> assignments;   ///< Same size as @c webs.
    std::string message;                      ///< Human-readable status / warning.
};

/**
 * @brief T2.1 — straight graph colouring with the @c basic algorithm.
 *
 * Iterates N from 1 to @c config.numRegisters and returns the smallest N for
 * which Chaitin-style simplification succeeds.
 *
 * Complexity: O(W^3 + W^2 P) where W is the number of webs and P is the
 * average number of points per web.
 */
AllocationResult allocateBasic(const InputData& input);

/**
 * @brief T2.2 — colouring with up to @c config.algorithmParam spilled webs.
 *
 * Spill selection rule: each round picks the web with the largest residual
 * degree (most interferences) among the still-allocated webs. Rationale:
 * removing the most-connected node is the single change that most decreases
 * the chromatic-number lower bound, so it has the best chance of unblocking
 * the next colouring attempt.
 *
 * Complexity: at most (K_param+1) colouring attempts, each O(W^3 + W^2 P),
 * plus the same factor for the spill-candidate scan; overall
 * O(K_param * (W^3 + W^2 P)).
 */
AllocationResult allocateSpilling(const InputData& input);

/**
 * @brief T2.3 — colouring with up to @c config.algorithmParam split webs.
 *
 * Split selection rule: each round picks the multi-sub-web web with the
 * highest degree and breaks it into its atomic sub-webs (one Web per
 * originating input range). Rationale: only webs that are unions of
 * several sub-webs are even candidates for splitting; among those, the
 * most-connected one is the one whose decomposition is most likely to
 * reduce the global chromatic number.
 *
 * Complexity: at most (K_param+1) iterations; each iteration rebuilds the
 * webs (O(P log P)) and runs a colouring attempt (O(W'^3 + W'^2 P)) where
 * W' is the current web count (bounded by the total number of input
 * sub-webs). Overall O(K_param * (W'^3 + W'^2 P)).
 */
AllocationResult allocateSplitting(const InputData& input);

/**
 * @brief T2.4 — Free algorithm: hybrid split-then-spill heuristic.
 *
 * No numeric parameter. At every colouring failure we pick the
 * highest-degree still-active web in the interference graph and:
 *   - if it is the merger of >= 2 sub-webs (multiple input live ranges),
 *     we SPLIT it into its atomic sub-webs and retry;
 *   - otherwise (already atomic), we SPILL it to memory and retry.
 *
 * Rationale: splitting keeps the value in registers, so it is "cheaper"
 * than spilling (no extra load/store instructions). We therefore try
 * splitting first and only fall back to spilling once a problematic web
 * cannot be decomposed any further.
 *
 * Termination: each iteration either creates one new atomic group
 * (bounded by the total number of input live ranges) or marks one
 * sub-web as spilled (also bounded). With every sub-web spilled the
 * remaining graph is empty and trivially colourable, so the loop always
 * finishes.
 *
 * Complexity: at most 2N+2 iterations where N is the total number of
 * input sub-webs; each iteration runs a colouring attempt
 * (O(W'^3 + W'^2 P)). Overall O(N * (W'^3 + W'^2 P)).
 */
AllocationResult allocateFree(const InputData& input);

/**
 * @brief Dispatches on @c config.algorithm to the appropriate allocator.
 */
AllocationResult allocate(const InputData& input);

#endif // DA_PRJ2_ALLOCATOR_H
