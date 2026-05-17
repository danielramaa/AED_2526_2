/**
 * @file output.cpp
 * @brief Implementation of the allocation writer.
 */
#include "output.h"

#include <algorithm>
#include <fstream>
#include <ostream>

namespace {

/// When a merged web has BOTH `isDef` and `isLastUse` at the same line the
/// value is being used and redefined on the same instruction (think
/// `i = i + 1`): the web flows through that point so it is shown bare,
/// without either marker. Only points that are a pure definition or a pure
/// last-use within the merged web keep their `+` / `-` respectively.
std::string formatPoint(const WebPoint& p) {
    std::string s = std::to_string(p.line);
    if (p.isDef && p.isLastUse) {
        // Bare line number: the web is live across this point.
    } else if (p.isDef) {
        s.push_back('+');
    } else if (p.isLastUse) {
        s.push_back('-');
    }
    return s;
}

/// Comma-joins all points of a web, assuming points are already sorted.
std::string formatWebPoints(const Web& w) {
    std::string s;
    for (std::size_t i = 0; i < w.points.size(); ++i) {
        if (i) s.push_back(',');
        s += formatPoint(w.points[i]);
    }
    return s;
}

} 

void writeAllocation(std::ostream& os, const AllocationResult& result) {
    os << "# Total number of webs followed by the listing of the program points of each one\n";
    os << "# program points in each web are sorted in ascending order\n";
    os << "webs: " << result.webs.size() << "\n";
    for (const auto& w : result.webs) {
        os << "web" << w.id << ": " << formatWebPoints(w) << "\n";
    }

    os << "# Total number of registers used, followed by assignment to webs\n";
    os << "registers: " << result.registersUsed << "\n";

    // Emit register lines grouped by register id (r0 first, then r1, ...),
    // followed by every memory-spilled web. Within each group webs keep
    // their natural id order.
    std::vector<WebAssignment> sorted = result.assignments;
    std::sort(sorted.begin(), sorted.end(),
              [](const WebAssignment& a, const WebAssignment& b) {
        if (a.registerId == b.registerId) return a.webId < b.webId;
        // Spilled (-1) goes last.
        if (a.registerId < 0) return false;
        if (b.registerId < 0) return true;
        return a.registerId < b.registerId;
    });
    for (const auto& a : sorted) {
        if (a.registerId < 0) os << "M: web" << a.webId << "\n";
        else                  os << "r" << a.registerId << ": web" << a.webId << "\n";
    }
}

bool writeAllocationToFile(const std::string& path, const AllocationResult& result) {
    std::ofstream out(path);
    if (!out) return false;
    writeAllocation(out, result);
    return true;
}
