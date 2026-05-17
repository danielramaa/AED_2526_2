/**
 * @file output.h
 * @brief Output writer: turns an AllocationResult into the text format
 *        specified
 */
#ifndef DA_PRJ2_OUTPUT_H
#define DA_PRJ2_OUTPUT_H

#include "allocator.h"

#include <iosfwd>
#include <string>

/**
 * @brief Writes the result to @p os in the spec format.
 *
 * Layout (each line a literal line of the output file):
 * @code
 *   webs: N
 *   web0: <comma-separated points>
 *   ...
 *   registers: M
 *   r0: webX
 *   r0: webY
 *   r1: webZ
 *   M: webK   (for spilled webs / infeasible runs)
 * @endcode
 *
 * Points within a web are sorted ascending and carry their `+` / `-`
 * markers verbatim.
 */
void writeAllocation(std::ostream& os, const AllocationResult& result);

/**
 * @brief Convenience wrapper that opens @p path for writing and emits the
 *        allocation through writeAllocation.
 *
 * @return true on success, false if the file could not be opened.
 */
bool writeAllocationToFile(const std::string& path, const AllocationResult& result);

#endif // DA_PRJ2_OUTPUT_H
