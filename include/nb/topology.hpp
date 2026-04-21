#pragma once

#include "nb/edge.hpp"
#include "nb/edge_options.hpp"

#include <memory>
#include <unordered_set>
#include <vector>

namespace nb {

// Topology: singleton tracking the set of edges and the set of failed
// nodes. Mirrors Java Topology. `allToAll=true` auto-creates edges on
// lookup when no explicit edge was declared (used by Trawler default mode).
class Topology {
public:
    static Topology& instance(bool allToAll = false);
    // Reset the singleton (used by tests).
    static void reset();

    // Return the live edge between a and b, or nullptr.
    Edge* getLiveEdge(int a, int b);

    bool isNodeAlive(int node) const;

    void newEdge(int a, int b, const EdgeOptions& options);
    bool failEdge(int a, int b);
    bool restartEdge(int a, int b);
    void failNode(int a);
    void restartNode(int a);

private:
    explicit Topology(bool allToAll);

    Edge* getEdge(int a, int b);
    bool changeEdge(int a, int b, bool live);

    std::vector<std::unique_ptr<Edge>> edges_;
    std::unordered_set<int> failedNodes_;
    bool allToAll_;
};

} // namespace nb
