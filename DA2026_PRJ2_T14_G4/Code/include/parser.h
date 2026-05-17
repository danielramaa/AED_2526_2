/**
 * @file parser.h
 * @brief Functions to load the two input files (live-ranges + register
 *        configuration) into the in-memory data structures defined in
 *        types.h.
 */
#ifndef DA_PRJ2_PARSER_H
#define DA_PRJ2_PARSER_H

#include "types.h"
#include <string>
#include <vector>

/**
 * @brief Parses a live-ranges input file
 *
 * Lines beginning with `#` and blank lines are ignored. Each non-comment line
 * has the form `name: tok1, tok2, ...` where every `tok` is a non-negative
 * integer optionally suffixed with `+` (start / definition) and/or `-`
 * (end / last use).
 *
 * On success @p variables is filled with one Variable per distinct name
 * (preserving the order of first appearance) and each Variable contains one
 * LiveRange per input line it appeared on.
 *
 * @param path       Path of the ranges file.
 * @param variables  Output vector, cleared before being populated.
 * @param error      On failure, a human-readable description of the problem.
 * @return true on success, false on I/O or syntax error.
 *
 * Complexity: O(L) where L is the total number of tokens in the file.
 */
bool parseRanges(const std::string& path,
                 std::vector<Variable>& variables,
                 std::string& error);

/**
 * @brief Parses a register-configuration file
 * Expected entries: `registers: N` and `algorithm: <kind>[, K]` where
 * `<kind>` is one of `basic`, `spilling`, `splitting`, `free`. Comments
 * (`#`) and blank lines are tolerated and the order of the two entries is
 * irrelevant.
 *
 * @param path    Path of the registers file.
 * @param config  Output structure, fully overwritten on success.
 * @param error   On failure, a human-readable description of the problem.
 * @return true on success, false on I/O, syntax or semantic error.
 *
 * Complexity: O(L) where L is the number of lines of the file.
 */
bool parseRegisters(const std::string& path,
                    RegisterConfig& config,
                    std::string& error);

#endif // DA_PRJ2_PARSER_H
