#include "nb/topology.hpp"

#include <iostream>
#include <memory>

namespace nb {

namespace {
std::unique_ptr<Topology>& instancePtr() {
    static std::unique_ptr<Topology> ptr;
    return ptr;
}

}

Topology::Topology(bool allToAll) : allToAll_(allToAll) {}

Topology& Topology::instance(bool allToAll) {
    auto& ptr = instancePtr();
    if (!ptr) {
        ptr.reset(new Topology(allToAll));
    }
    return *ptr;
}

void Topology::reset() {
    instancePtr().reset();
}

Edge* Topology::getLiveEdge(int a, int b) {
    Edge* e = getEdge(a, b);
    if (!e || !e->isLive() || !isNodeAlive(a) || !isNodeAlive(b)) {
        return nullptr;
    }
    return e;
}

bool Topology::isNodeAlive(int node) const {
    return failedNodes_.find(node) == failedNodes_.end();
}

void Topology::newEdge(int a, int b, const EdgeOptions& options) {
    Edge* e = getEdge(a, b);
    if (e) {
        e->setOptions(options);
    } else {
        edges_.push_back(std::make_unique<Edge>(a, b, options));
    }
}

bool Topology::failEdge(int a, int b) { return changeEdge(a, b, false); }

bool Topology::restartEdge(int a, int b) { return changeEdge(a, b, true); }

void Topology::failNode(int a) { failedNodes_.insert(a); }

void Topology::restartNode(int a) { failedNodes_.erase(a); }

Edge* Topology::getEdge(int a, int b) {
    for (auto& e : edges_) {
        if (e->isEdge(a, b)) {
            return e.get();
        }
    }
    if (allToAll_) {
        edges_.push_back(std::make_unique<Edge>(a, b, EdgeOptions{}));
        return edges_.back().get();
    }
    return nullptr;
}

bool Topology::changeEdge(int a, int b, bool state) {
    Edge* e = getEdge(a, b);
    if (e) {
        e->setState(state);
        return true;
    }
    std::cerr << "No edge exists between " << a << " and " << b << std::endl;
    return false;
}

} // namespace nb
