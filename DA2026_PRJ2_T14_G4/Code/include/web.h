/**
 * @file web.h
 * @brief Web construction and the interference graph built on top of them.
 *
 * The pipeline is: input Variables -> SubWebs (one per input LiveRange)
 * -> grouping of SubWebs into Webs -> Graph<int> interference graph.
 *
 * SubWebs are the atomic units. Webs are unions of SubWebs of the SAME
 * variable that share at least one program point. Splitting (T2.3) is
 * implemented by changing the grouping, which is why the SubWeb layer is
 * kept around even when the simpler allocators do not need it.
 */
#ifndef DA_PRJ2_WEB_H
#define DA_PRJ2_WEB_H

#include "data_structures/Graph.h"
#include "types.h"

#include <string>
#include <vector>

/**
 * @brief A program point belonging to a (sub-)web, with the def/last-use
 *        markers carried over from the input.
 *
 * When multiple SubWebs are merged into a Web both markers are OR-ed, so a
 * single point can carry isDef=true AND isLastUse=true simultaneously
 * (typical of an i=i+1 pattern).
 */
struct WebPoint {
    int line = 0;
    bool isDef = false;
    bool isLastUse = false;
};

/**
 * @brief Atomic web fragment: exactly one input LiveRange.
 */
struct SubWeb {
    int id = 0;                       ///< Index in the flat subwebs vector.
    std::string varName;              ///< Originating variable.
    int varIndex = 0;                 ///< Originating variable index (in input).
    int rangeIndex = 0;               ///< Range index inside that variable.
    std::vector<WebPoint> points;     ///< Sorted ascending by line.
};

/**
 * @brief A web — a (possibly merged) group of SubWebs assigned a single
 *        register (or spilled).
 *
 * Web ids are always assigned contiguously (0..N-1) so they can be used
 * directly as the vertex info of Graph<int>.
 */
struct Web {
    int id = 0;                       ///< Assigned by buildWebsFromGroups.
    std::vector<int> subWebIds;       ///< Composing SubWeb ids.
    std::vector<WebPoint> points;     ///< Union of all sub-web points, sorted ascending.
};

/**
 * @brief Builds one SubWeb per LiveRange of every Variable, preserving the
 *        original ordering and sorting each SubWeb's points by program line.
 *
 * Complexity: O(P log P) where P is the total number of program points.
 */
std::vector<SubWeb> buildSubWebs(const std::vector<Variable>& variables);

/**
 * @brief Groups SubWebs of the same variable that share at least one
 *        program point (union-find). Each returned group is a list of
 *        SubWeb ids that will become one Web.
 *
 * Complexity: O(V * R^2 * P) in the worst case where V = variables,
 * R = ranges per variable, P = points per range; in practice far less
 * because most variables only have a handful of ranges.
 */
std::vector<std::vector<int>> mergeSubWebs(const std::vector<SubWeb>& subwebs);

/**
 * @brief Materialises Webs from a grouping. Web ids are contiguous
 *        starting at 0 in the same order as @p groups.
 *
 * Complexity: O(P log P) total across all webs.
 */
std::vector<Web> buildWebsFromGroups(const std::vector<std::vector<int>>& groups,
                                     const std::vector<SubWeb>& subwebs);

/**
 * @brief Convenience entry point: subwebs -> groups -> webs in one call.
 *
 * Used by the allocators that never need to perform splitting.
 */
std::vector<Web> buildWebs(const std::vector<Variable>& variables);

/**
 * @brief Pairwise interference test using the def/last-use exception
 *        described: two webs do NOT interfere at a
 *        shared point X iff one of them is being defined-only there and the
 *        other is being last-used-only there. They DO interfere as soon as
 *        any shared point is not such a clean handoff.
 *
 * Complexity: O(|A.points| + |B.points|) using the points' sortedness.
 */
bool websInterfere(const Web& a, const Web& b);

/**
 * @brief Builds the interference graph on the provided webs using the TP
 *        Graph class. Vertex info is the web id; edges are bidirectional.
 *
 * Complexity: O(W^2 * P) where W is the number of webs and P is the
 * average number of points per web.
 */
Graph<int> buildInterferenceGraph(const std::vector<Web>& webs);

/**
 * @brief Prints a human-readable ASCII view of the interference graph
 *        to @p os: one line per web showing its id, points and its list
 *        of neighbours.
 *
 * @code
 *   web0  points: 7+,8,9,10-          interferes with: web2
 *   web1  points: 1+,2,3,4,5,6-       interferes with: (none)
 *   web2  points: 9+,10,11,...        interferes with: web0
 *   total vertices: 3   total edges: 1
 * @endcode
 */
void printGraphAscii(std::ostream& os, const std::vector<Web>& webs);

/**
 * @brief Emits the interference graph in Graphviz DOT format. Render
 *        with:
 *
 * @code
 *   dot -Tpng graph.dot -o graph.png
 * @endcode
 *
 * Each web becomes a node labelled `webN\n<points>`; each interference
 * becomes an undirected edge.
 */
void writeGraphDot(std::ostream& os, const std::vector<Web>& webs,
                   const std::string& title = "interference");

#endif // DA_PRJ2_WEB_H
