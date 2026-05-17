/**
 * @file allocator.cpp
 * @brief Register-allocation algorithms (basic, spilling, splitting).
 *
 * All three variants share the same core: build webs, build an interference
 * graph and try to colour it with the smallest N <= K via a Chaitin-style
 * simplify/select pass. The spilling and splitting variants wrap that core
 * in an outer loop that selectively removes / fragments webs between
 * attempts.
 */
#include "allocator.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace {

/**
 * @brief Chaitin-style simplification-and-colouring of a sub-graph.
 *
 * Operates on the static interference graph induced by @p webs minus any
 * web id in @p excluded. Returns true and fills @p colours (indexed by web
 * id, -1 for excluded webs) iff the remaining graph can be N-coloured with
 * the greedy algorithm.
 */
bool tryColorWebs(const std::vector<Web>& webs,
                  const std::set<int>& excluded,
                  int N,
                  std::vector<int>& colors) {
    const int n = static_cast<int>(webs.size());
    colors.assign(n, -1);
    if (N <= 0) return n == static_cast<int>(excluded.size());

    // Build adjacency restricted to active vertices.
    std::vector<std::vector<int>> adj(n);
    for (int i = 0; i < n; ++i) {
        if (excluded.count(i)) continue;
        for (int j = i + 1; j < n; ++j) {
            if (excluded.count(j)) continue;
            if (websInterfere(webs[i], webs[j])) {
                adj[i].push_back(j);
                adj[j].push_back(i);
            }
        }
    }

    std::vector<bool> active(n, true);
    for (int e : excluded) if (e >= 0 && e < n) active[e] = false;
    std::vector<int> degree(n, 0);
    for (int i = 0; i < n; ++i) if (active[i]) degree[i] = static_cast<int>(adj[i].size());

    const int remaining = n - static_cast<int>(excluded.size());
    std::vector<int> stack;
    stack.reserve(remaining);

    while (static_cast<int>(stack.size()) < remaining) {
        int picked = -1;
        for (int v = 0; v < n; ++v) {
            if (!active[v]) continue;
            if (degree[v] < N) { picked = v; break; }
        }
        if (picked < 0) return false;   // would need spilling
        active[picked] = false;
        stack.push_back(picked);
        for (int u : adj[picked]) if (active[u]) --degree[u];
    }

    for (int i = static_cast<int>(stack.size()) - 1; i >= 0; --i) {
        int v = stack[i];
        std::vector<bool> used(N, false);
        for (int u : adj[v]) {
            if (colors[u] >= 0 && colors[u] < N) used[colors[u]] = true;
        }
        for (int c = 0; c < N; ++c) {
            if (!used[c]) { colors[v] = c; break; }
        }
    }
    return true;
}

/**
 * @brief Tries the colouring with N = 1, 2, ..., K and returns the result
 *        of the first successful attempt (i.e. the minimum number of
 *        registers actually needed for this graph configuration).
 */
bool colorWithMinRegisters(const std::vector<Web>& webs,
                           const std::set<int>& excluded,
                           int K,
                           std::vector<int>& colors,
                           int& registersUsed) {
    for (int N = 1; N <= K; ++N) {
        if (tryColorWebs(webs, excluded, N, colors)) {
            std::set<int> distinct;
            for (int c : colors) if (c >= 0) distinct.insert(c);
            registersUsed = static_cast<int>(distinct.size());
            return true;
        }
    }
    return false;
}

/// Builds an AllocationResult that flags every web as spilled to memory.
AllocationResult makeInfeasibleResult(std::vector<Web> webs, std::string msg) {
    AllocationResult r;
    r.feasible = false;
    r.registersUsed = 0;
    r.webs = std::move(webs);
    r.assignments.reserve(r.webs.size());
    for (const auto& w : r.webs) r.assignments.push_back({w.id, -1});
    r.message = std::move(msg);
    return r;
}

/// Packs a successful colouring into an AllocationResult.
AllocationResult makeSuccessResult(std::vector<Web> webs,
                                   const std::vector<int>& colors,
                                   const std::set<int>& spilled,
                                   int registersUsed,
                                   std::string msg) {
    AllocationResult r;
    r.feasible = true;
    r.registersUsed = registersUsed;
    r.webs = std::move(webs);
    r.assignments.reserve(r.webs.size());
    for (const auto& w : r.webs) {
        if (spilled.count(w.id)) r.assignments.push_back({w.id, -1});
        else                     r.assignments.push_back({w.id, colors[w.id]});
    }
    r.message = std::move(msg);
    return r;
}

/// Picks the still-active web with the highest residual degree (ties broken
/// by web id for determinism).
int pickHighestDegreeWeb(const std::vector<Web>& webs, const std::set<int>& excluded) {
    int best = -1;
    int bestDeg = -1;
    for (std::size_t i = 0; i < webs.size(); ++i) {
        if (excluded.count(static_cast<int>(i))) continue;
        int deg = 0;
        for (std::size_t j = 0; j < webs.size(); ++j) {
            if (j == i) continue;
            if (excluded.count(static_cast<int>(j))) continue;
            if (websInterfere(webs[i], webs[j])) ++deg;
        }
        if (deg > bestDeg) {
            bestDeg = deg;
            best = static_cast<int>(i);
        }
    }
    return best;
}

/// Picks the multi-sub-web web with the highest degree, ready to be sliced.
/// Optionally ignores webs in @p excluded (already spilled).
int pickHighestDegreeMultiSubWeb(const std::vector<Web>& webs,
                                 const std::set<int>& excluded = {}) {
    int best = -1;
    int bestDeg = -1;
    for (std::size_t i = 0; i < webs.size(); ++i) {
        if (webs[i].subWebIds.size() < 2) continue;
        if (excluded.count(static_cast<int>(i))) continue;
        int deg = 0;
        for (std::size_t j = 0; j < webs.size(); ++j) {
            if (j == i) continue;
            if (excluded.count(static_cast<int>(j))) continue;
            if (websInterfere(webs[i], webs[j])) ++deg;
        }
        if (deg > bestDeg) {
            bestDeg = deg;
            best = static_cast<int>(i);
        }
    }
    return best;
}

}

AllocationResult allocateBasic(const InputData& input) {
    auto webs = buildWebs(input.variables);
    const int K = input.config.numRegisters;
    std::vector<int> colors;
    int regsUsed = 0;
    if (colorWithMinRegisters(webs, /*excluded=*/{}, K, colors, regsUsed)) {
        return makeSuccessResult(std::move(webs), colors, /*spilled=*/{}, regsUsed,
                                 "ok");
    }
    return makeInfeasibleResult(std::move(webs),
        "basic: cannot colour interference graph with " + std::to_string(K) +
        " register(s)");
}

AllocationResult allocateSpilling(const InputData& input) {
    auto webs = buildWebs(input.variables);
    const int K = input.config.numRegisters;
    const int maxSpills = input.config.algorithmParam;

    std::set<int> spilled;
    for (int spillCount = 0; spillCount <= maxSpills; ++spillCount) {
        std::vector<int> colors;
        int regsUsed = 0;
        if (colorWithMinRegisters(webs, spilled, K, colors, regsUsed)) {
            std::ostringstream msg;
            msg << "spilling: " << spillCount << " web(s) spilled to memory";
            return makeSuccessResult(std::move(webs), colors, spilled, regsUsed,
                                     msg.str());
        }
        if (spillCount == maxSpills) break;
        int candidate = pickHighestDegreeWeb(webs, spilled);
        if (candidate < 0) break;
        spilled.insert(candidate);
    }
    return makeInfeasibleResult(std::move(webs),
        "spilling: still infeasible after spilling " + std::to_string(maxSpills) +
        " web(s)");
}

AllocationResult allocateSplitting(const InputData& input) {
    auto subwebs = buildSubWebs(input.variables);
    auto groups = mergeSubWebs(subwebs);
    const int K = input.config.numRegisters;
    const int maxSplits = input.config.algorithmParam;

    for (int splitCount = 0; splitCount <= maxSplits; ++splitCount) {
        auto webs = buildWebsFromGroups(groups, subwebs);
        std::vector<int> colors;
        int regsUsed = 0;
        if (colorWithMinRegisters(webs, /*excluded=*/{}, K, colors, regsUsed)) {
            std::ostringstream msg;
            msg << "splitting: " << splitCount << " web(s) split";
            return makeSuccessResult(std::move(webs), colors, /*spilled=*/{},
                                     regsUsed, msg.str());
        }
        if (splitCount == maxSplits) break;
        int target = pickHighestDegreeMultiSubWeb(webs);
        if (target < 0) break;   // nothing else to split

        // Break the chosen web into its atomic sub-webs: keep the first
        // sub-web in the original group slot, push each of the remaining
        // ones as its own new group.
        std::vector<int> targetSubs = groups[target];
        groups[target] = { targetSubs.front() };
        for (std::size_t k = 1; k < targetSubs.size(); ++k) {
            groups.push_back({ targetSubs[k] });
        }
    }

    auto webs = buildWebsFromGroups(groups, subwebs);
    return makeInfeasibleResult(std::move(webs),
        "splitting: still infeasible after splitting " + std::to_string(maxSplits) +
        " web(s)");
}

AllocationResult allocateFree(const InputData& input) {
    auto subwebs = buildSubWebs(input.variables);
    auto groups = mergeSubWebs(subwebs);
    const int K = input.config.numRegisters;

    // Sub-webs (not whole webs) are the unit of spill bookkeeping because
    // web ids change every time we split a group: tracking by sub-web id
    // is the only thing stable across iterations.
    std::set<int> spilledSubWebIds;
    int splitCount = 0;
    int spillCount = 0;

    // Loose upper bound: every iteration either grows `groups` by one
    // (split) or grows `spilledSubWebIds` by one (spill). Both vectors
    // are bounded by the total number of sub-webs, so 2N+2 is plenty.
    const int maxIter = static_cast<int>(subwebs.size()) * 2 + 2;

    for (int iter = 0; iter <= maxIter; ++iter) {
        auto webs = buildWebsFromGroups(groups, subwebs);

        // A web is excluded from colouring iff every one of its sub-webs
        // has been spilled. After splitting, an already-spilled sub-web
        // ends up alone in its own group and that group is naturally
        // excluded — no extra bookkeeping needed.
        std::set<int> excluded;
        for (const auto& w : webs) {
            if (w.subWebIds.empty()) continue;
            bool allSpilled = true;
            for (int sid : w.subWebIds) {
                if (!spilledSubWebIds.count(sid)) { allSpilled = false; break; }
            }
            if (allSpilled) excluded.insert(w.id);
        }

        std::vector<int> colors;
        int regsUsed = 0;
        if (colorWithMinRegisters(webs, excluded, K, colors, regsUsed)) {
            std::ostringstream msg;
            msg << "free: " << splitCount << " split(s), " << spillCount
                << " spill(s)";
            return makeSuccessResult(std::move(webs), colors, excluded,
                                     regsUsed, msg.str());
        }

        // Colouring failed: we prefer splitting over spilling because
        // splitting keeps the value in registers. Strategy:
        //   1- try to find a splittable web (>= 2 sub-webs) that still
        //      participates in an interference — if any, split it;
        //   2- otherwise fall back to spilling the highest-degree
        //      atomic web.
        int splitCandidate = pickHighestDegreeMultiSubWeb(webs, excluded);
        if (splitCandidate >= 0 && groups[splitCandidate].size() >= 2) {
            // SPLIT: break the chosen web's group into atomic sub-webs.
            // Keep the first sub-web in the original slot to preserve
            // determinism; append every other sub-web as its own group.
            std::vector<int> targetSubs = groups[splitCandidate];
            groups[splitCandidate] = { targetSubs.front() };
            for (std::size_t k = 1; k < targetSubs.size(); ++k) {
                groups.push_back({ targetSubs[k] });
            }
            ++splitCount;
            continue;
        }

        // No splittable candidate, fall back to spilling.
        int spillCandidate = pickHighestDegreeWeb(webs, excluded);
        if (spillCandidate < 0) break;   // nothing more we can do
        for (int sid : webs[spillCandidate].subWebIds) {
            spilledSubWebIds.insert(sid);
        }
        ++spillCount;
    }

    // We only get here in pathological cases (ex: K == 0 with non-empty
    // input). Report every web as spilled.
    auto webs = buildWebsFromGroups(groups, subwebs);
    return makeInfeasibleResult(std::move(webs),
        "free: still infeasible after " + std::to_string(splitCount) +
        " split(s) and " + std::to_string(spillCount) + " spill(s)");
}

AllocationResult allocate(const InputData& input) {
    switch (input.config.algorithm) {
        case AlgorithmKind::BASIC:     return allocateBasic(input);
        case AlgorithmKind::SPILLING:  return allocateSpilling(input);
        case AlgorithmKind::SPLITTING: return allocateSplitting(input);
        case AlgorithmKind::FREE:      return allocateFree(input);
    }
    return allocateBasic(input);
}
