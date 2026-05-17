/**
 * @file types.h
 * @brief Core data structures shared by the parser, the menu and the
 *        register-allocation algorithms.
 */
#ifndef DA_PRJ2_TYPES_H
#define DA_PRJ2_TYPES_H

#include <string>
#include <vector>

/**
 * @brief A single program point (line number) belonging to a live range.
 *
 * A point may simultaneously be the definition (`+`) of a new range and the
 * last use (`-`) of a previous range; both flags are independent.
 */
struct ProgramPoint {
    int line = 0;        ///< Program line number.
    bool isDef = false;  ///< True when this point is marked with `+` (definition / start).
    bool isLastUse = false; ///< True when this point is marked with `-` (last use / end).
};

/**
 * @brief A live range as it appears on one input line.
 *
 * The points are stored in the order they appear in the file (which the
 * parser also enforces to be ascending). The input format guarantees that the
 * first point of every range is a definition (`+`) and the last is a last
 * use (`-`), but intermediate points may also carry markers when ranges
 * touch / overlap on the same program line.
 */
struct LiveRange {
    std::vector<ProgramPoint> points;
};

/**
 * @brief A variable as named in the source program, together with all the
 *        live ranges parsed for it.
 *
 * A given variable may appear in several entries of the ranges file – each
 * such entry becomes a separate LiveRange. Web construction (T2.1) will fuse
 * those that share program points.
 */
struct Variable {
    std::string name;
    std::vector<LiveRange> ranges;
};

/**
 * @brief Algorithm variants supported by the allocator.
 *
 * The numeric parameter `K` is only meaningful for SPILLING and SPLITTING.
 */
enum class AlgorithmKind {
    BASIC,
    SPILLING,
    SPLITTING,
    FREE
};

/**
 * @brief Parsed contents of the `registers` input file.
 */
struct RegisterConfig {
    int numRegisters = 0;           ///< Number of available physical registers (K).
    AlgorithmKind algorithm = AlgorithmKind::BASIC;
    int algorithmParam = 0;         ///< Numeric parameter (only for SPILLING / SPLITTING).
    bool hasParam = false;          ///< True if a parameter was supplied with the algorithm.
};

/**
 * @brief Everything the allocator needs as input: variables + register/algorithm config.
 */
struct InputData {
    std::vector<Variable> variables;
    RegisterConfig config;
};

/// Human-readable name for an AlgorithmKind value (used by menu / output).
inline std::string algorithmKindName(AlgorithmKind k) {
    switch (k) {
        case AlgorithmKind::BASIC:     return "basic";
        case AlgorithmKind::SPILLING:  return "spilling";
        case AlgorithmKind::SPLITTING: return "splitting";
        case AlgorithmKind::FREE:      return "free";
    }
    return "?";
}

#endif // DA_PRJ2_TYPES_H
