#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace nicxlive::core::param { class Parameter; class ParameterBinding; }

namespace nicxlive::core::nodes {
class Node;

// Native lifetime bookkeeping only. Like model mutation/rendering, collection
// runs at a quiescent host boundary, never concurrently with graph access.
void trackRetainedNode(const std::shared_ptr<Node>& node);
void trackRetainedParameter(const std::shared_ptr<param::Parameter>& parameter);
bool collectRetainedNodeCycles() noexcept;

// Declare before the Puppet's owning members so it runs after their destruction.
struct CollectRetainedNodesAfterMembers {
    ~CollectRetainedNodesAfterMembers() { collectRetainedNodeCycles(); }
};

namespace lifetime_detail {
// Trial deletion counts actual shared ownership cells, including array backing
// and shared callable contexts. An external array/iterator/delegate is a root,
// even if it does not own a Node shared_ptr directly.
class Graph {
public:
    struct Object {
        std::shared_ptr<void> owner;
        std::function<void(Graph&, std::size_t)> visit;
        std::vector<std::size_t> edges;
        Node* node{};
        std::size_t incoming{};
        bool live{};
        std::function<void()> release;
    };
    std::vector<Object> objects;
    std::map<std::weak_ptr<void>, std::size_t, std::owner_less<std::weak_ptr<void>>> ids;

    template<class T, class Visit>
    std::size_t add(const std::shared_ptr<T>& owner, Visit visit, Node* node = nullptr) {
        auto [it, inserted] = ids.try_emplace(owner, objects.size());
        if (inserted) objects.push_back({owner, visit, {}, node});
        return it->second;
    }
    void edge(std::size_t from, std::size_t to) {
        objects[from].edges.push_back(to);
        ++objects[to].incoming;
    }
    std::size_t addNode(const std::shared_ptr<Node>& node);
    std::size_t addParameter(const std::shared_ptr<param::Parameter>& parameter);
    std::size_t addBinding(const std::shared_ptr<param::ParameterBinding>& binding);
    void parameterEdge(std::size_t from, const std::shared_ptr<param::Parameter>& parameter) {
        if (parameter) edge(from, addParameter(parameter));
    }
    void nodeEdge(std::size_t from, const std::shared_ptr<Node>& node) {
        if (node) edge(from, addNode(node));
    }
    template<class Array, class Visit>
    void array(std::size_t from, const Array& array, Visit visit) {
        // Foreign retained views are opaque. Their references remain external
        // roots; never guess the type or exclusive ownership of foreign memory.
        if (auto owner = array.vectorBacking()) {
            auto* data = owner.get();
            auto id = add(owner, [data, visit](Graph& graph, std::size_t at) {
                for (const auto& item : *data) visit(graph, at, item);
            });
            edge(from, id);
        }
    }
    template<class Map>
    void nodeKeyMap(std::size_t from, const Map& map) {
        if (auto owner = map.retentionOwner()) {
            auto* data = &map;
            auto id = add(owner, [data](Graph& graph, std::size_t at) {
                data->visitRetainedEntries([&](const auto& entry) {
                    auto* item = entry.get();
                    auto id = graph.add(entry, [item](Graph& graph, std::size_t at) {
                        graph.nodeEdge(at, item->first);
                    });
                    graph.edge(at, id);
                });
            });
            edge(from, id);
        }
    }
};
void visitRetainedFilters(Graph& graph, std::size_t at, Node& node);
} // namespace lifetime_detail
} // namespace nicxlive::core::nodes
