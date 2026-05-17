/**
 * @file web.cpp
 * @brief Web construction and interference graph implementation.
 */
#include "web.h"

#include <algorithm>
#include <numeric>
#include <ostream>
#include <sstream>
#include <unordered_map>

namespace {

/// Minimal union-find used to merge SubWebs that share program points.
/// (Field renamed away from `parent` to dodge the macro defined by
/// MutablePriorityQueue.h, which is transitively pulled in via Graph.h.)
struct UnionFind {
    std::vector<int> p;
    explicit UnionFind(int n) : p(n) { std::iota(p.begin(), p.end(), 0); }
    int find(int x) { return p[x] == x ? x : p[x] = find(p[x]); }
    void unite(int a, int b) { p[find(a)] = find(b); }
};

/// True iff @p a and @p b share at least one program line.
bool subWebsSharePoint(const SubWeb& a, const SubWeb& b) {
    std::size_t i = 0, j = 0;
    while (i < a.points.size() && j < b.points.size()) {
        if      (a.points[i].line < b.points[j].line) ++i;
        else if (a.points[i].line > b.points[j].line) ++j;
        else return true;
    }
    return false;
}

/// Merges multiple per-line marker sets into a single sorted point list.
std::vector<WebPoint> mergePointLists(const std::vector<const std::vector<WebPoint>*>& sources) {
    std::unordered_map<int, WebPoint> byLine;
    for (const auto* src : sources) {
        for (const auto& p : *src) {
            auto& dst = byLine[p.line];
            dst.line       = p.line;
            dst.isDef      = dst.isDef      || p.isDef;
            dst.isLastUse  = dst.isLastUse  || p.isLastUse;
        }
    }
    std::vector<WebPoint> out;
    out.reserve(byLine.size());
    for (auto& kv : byLine) out.push_back(kv.second);
    std::sort(out.begin(), out.end(),
              [](const WebPoint& x, const WebPoint& y) { return x.line < y.line; });
    return out;
}

} // namespace

std::vector<SubWeb> buildSubWebs(const std::vector<Variable>& variables) {
    std::vector<SubWeb> result;
    int nextId = 0;
    for (std::size_t vi = 0; vi < variables.size(); ++vi) {
        const auto& var = variables[vi];
        for (std::size_t ri = 0; ri < var.ranges.size(); ++ri) {
            SubWeb sw;
            sw.id         = nextId++;
            sw.varName    = var.name;
            sw.varIndex   = static_cast<int>(vi);
            sw.rangeIndex = static_cast<int>(ri);
            sw.points.reserve(var.ranges[ri].points.size());
            for (const auto& pp : var.ranges[ri].points) {
                WebPoint wp;
                wp.line      = pp.line;
                wp.isDef     = pp.isDef;
                wp.isLastUse = pp.isLastUse;
                sw.points.push_back(wp);
            }
            std::sort(sw.points.begin(), sw.points.end(),
                      [](const WebPoint& a, const WebPoint& b) { return a.line < b.line; });
            result.push_back(std::move(sw));
        }
    }
    return result;
}

std::vector<std::vector<int>> mergeSubWebs(const std::vector<SubWeb>& subwebs) {
    const int n = static_cast<int>(subwebs.size());
    UnionFind uf(n);

    // Group SubWeb ids by variable name so we only test within-variable pairs.
    std::unordered_map<std::string, std::vector<int>> byVar;
    for (int i = 0; i < n; ++i) byVar[subwebs[i].varName].push_back(i);

    for (auto& kv : byVar) {
        const auto& ids = kv.second;
        for (std::size_t i = 0; i < ids.size(); ++i)
            for (std::size_t j = i + 1; j < ids.size(); ++j)
                if (subWebsSharePoint(subwebs[ids[i]], subwebs[ids[j]]))
                    uf.unite(ids[i], ids[j]);
    }

    // Collect groups, preserving first-appearance order of the representative.
    std::unordered_map<int, std::vector<int>> rootToGroup;
    std::vector<int> rootOrder;
    for (int i = 0; i < n; ++i) {
        int r = uf.find(i);
        if (rootToGroup.find(r) == rootToGroup.end()) rootOrder.push_back(r);
        rootToGroup[r].push_back(i);
    }
    std::vector<std::vector<int>> out;
    out.reserve(rootOrder.size());
    for (int r : rootOrder) out.push_back(std::move(rootToGroup[r]));
    return out;
}

std::vector<Web> buildWebsFromGroups(const std::vector<std::vector<int>>& groups,
                                     const std::vector<SubWeb>& subwebs) {
    std::vector<Web> webs;
    webs.reserve(groups.size());
    for (std::size_t g = 0; g < groups.size(); ++g) {
        Web w;
        w.id = static_cast<int>(g);
        w.subWebIds = groups[g];
        std::vector<const std::vector<WebPoint>*> sources;
        sources.reserve(groups[g].size());
        for (int sid : groups[g]) sources.push_back(&subwebs[sid].points);
        w.points = mergePointLists(sources);
        webs.push_back(std::move(w));
    }
    return webs;
}

std::vector<Web> buildWebs(const std::vector<Variable>& variables) {
    auto sub = buildSubWebs(variables);
    auto groups = mergeSubWebs(sub);
    return buildWebsFromGroups(groups, sub);
}

bool websInterfere(const Web& a, const Web& b) {
    std::size_t i = 0, j = 0;
    while (i < a.points.size() && j < b.points.size()) {
        if (a.points[i].line < b.points[j].line) { ++i; continue; }
        if (a.points[i].line > b.points[j].line) { ++j; continue; }

        const WebPoint& pa = a.points[i];
        const WebPoint& pb = b.points[j];

        // Clean handoff: one of the webs has ONLY a def at this point and
        // the other has ONLY a last-use. Any other combination (both
        // intermediate, both def, both last-use, def+lastuse on the same
        // web etc.) means both webs are simultaneously live and therefore
        // interfere at this point.
        bool aDefOnly = pa.isDef && !pa.isLastUse;
        bool aUseOnly = pa.isLastUse && !pa.isDef;
        bool bDefOnly = pb.isDef && !pb.isLastUse;
        bool bUseOnly = pb.isLastUse && !pb.isDef;

        bool cleanHandoff = (aDefOnly && bUseOnly) || (bDefOnly && aUseOnly);
        if (!cleanHandoff) return true;
        ++i; ++j;
    }
    return false;
}

Graph<int> buildInterferenceGraph(const std::vector<Web>& webs) {
    Graph<int> g;
    for (const auto& w : webs) g.addVertex(w.id);
    for (std::size_t i = 0; i < webs.size(); ++i) {
        for (std::size_t j = i + 1; j < webs.size(); ++j) {
            if (websInterfere(webs[i], webs[j])) {
                g.addBidirectionalEdge(webs[i].id, webs[j].id, 1.0);
            }
        }
    }
    return g;
}

namespace {

std::string pointsLabel(const Web& w) {
    std::string s;
    for (std::size_t i = 0; i < w.points.size(); ++i) {
        if (i) s.push_back(',');
        s += std::to_string(w.points[i].line);
        if (w.points[i].isDef && w.points[i].isLastUse) {
            // bare — flows through (same convention as output.cpp)
        } else if (w.points[i].isDef) {
            s.push_back('+');
        } else if (w.points[i].isLastUse) {
            s.push_back('-');
        }
    }
    return s;
}

} // namespace

void printGraphAscii(std::ostream& os, const std::vector<Web>& webs) {
    int totalEdges = 0;
    for (const auto& w : webs) {
        std::ostringstream neighbours;
        bool first = true;
        for (const auto& other : webs) {
            if (other.id == w.id) continue;
            if (!websInterfere(w, other)) continue;
            if (other.id > w.id) ++totalEdges; // count each pair once
            if (!first) neighbours << ", ";
            neighbours << "web" << other.id;
            first = false;
        }
        std::string nb = neighbours.str();
        if (nb.empty()) nb = "(none)";

        os << "  web" << w.id << "  "
           << "points: " << pointsLabel(w) << "\n"
           << "        interferes with: " << nb << "\n";
    }
    os << "  total vertices: " << webs.size()
       << "   total edges: " << totalEdges << "\n";
}

void writeGraphDot(std::ostream& os, const std::vector<Web>& webs,
                   const std::string& title) {
    os << "graph " << title << " {\n";
    os << "  // Render with: dot -Tpng " << title << ".dot -o " << title << ".png\n";
    os << "  node [shape=ellipse, style=filled, fillcolor=\"#cfe8ff\","
       << " fontname=\"Helvetica\"];\n";
    os << "  edge [color=\"#444444\"];\n";
    for (const auto& w : webs) {
        os << "  web" << w.id << " [label=\"web" << w.id
           << "\\n" << pointsLabel(w) << "\"];\n";
    }
    for (std::size_t i = 0; i < webs.size(); ++i) {
        for (std::size_t j = i + 1; j < webs.size(); ++j) {
            if (websInterfere(webs[i], webs[j])) {
                os << "  web" << webs[i].id << " -- web" << webs[j].id << ";\n";
            }
        }
    }
    os << "}\n";
}
