#include "retained_graph.hpp"
#include "part.hpp"
#include "projectable.hpp"
#include "simple_physics_systems.hpp"
#include "path_deformer.hpp"
#include "../render/resource_lifetime_internal.hpp"
#include <atomic>
#include <mutex>

namespace nicxlive::core::nodes {
namespace {
std::atomic<bool> registryAlive{true};
struct Registry {
    std::recursive_mutex mutex;
    std::map<std::weak_ptr<param::Parameter>, bool, std::owner_less<std::weak_ptr<param::Parameter>>> parameters;
    bool collecting{};
    std::map<std::weak_ptr<Node>, bool, std::owner_less<std::weak_ptr<Node>>> nodes;
    ~Registry() { registryAlive.store(false); }
};
Registry& registry() {
    static Registry value;
    return value;
}
}

void trackRetainedNode(const std::shared_ptr<Node>& node) {
    if (!node || !registryAlive.load()) return;
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    state.nodes.try_emplace(node, true);
}

void trackRetainedParameter(const std::shared_ptr<param::Parameter>& parameter) {
    if (!parameter || !registryAlive.load()) return;
    auto& state = registry(); std::lock_guard lock(state.mutex);
    state.parameters.try_emplace(parameter, true);
}

std::size_t lifetime_detail::Graph::addParameter(const std::shared_ptr<param::Parameter>& parameter) {
    trackRetainedParameter(parameter);
    auto* ptr = parameter.get();
    const auto id = add(parameter, [ptr](Graph& graph, std::size_t at) {
        graph.array(at, ptr->bindings, [](Graph& graph, auto at, const auto& binding) {
            if (binding) graph.edge(at, graph.addBinding(binding));
        });
    });
    objects[id].release = [ptr] { ptr->bindings.clear(); };
    return id;
}

std::size_t lifetime_detail::Graph::addBinding(const std::shared_ptr<param::ParameterBinding>& binding) {
    const auto typed = [&]<class T>(param::ParameterBindingImpl<T>* ptr) {
        const auto id = add(binding, [ptr](Graph& graph, std::size_t at) {
            graph.parameterEdge(at, ptr->parameter);
            const auto target = ptr->target.target.resource();
            if (auto node = std::dynamic_pointer_cast<Node>(target)) graph.nodeEdge(at, node);
            else if (auto parameter = std::dynamic_pointer_cast<param::Parameter>(target)) graph.parameterEdge(at, parameter);
        });
        objects[id].release = [ptr] { ptr->parameter.reset(); ptr->target.target.reset(); };
        return id;
    };
    if (auto* ptr = dynamic_cast<param::ParameterBindingImpl<float>*>(binding.get())) return typed(ptr);
    if (auto* ptr = dynamic_cast<param::ParameterBindingImpl<param::DeformSlot>*>(binding.get())) return typed(ptr);
    return add(binding, [](Graph&, std::size_t) {});
}

std::size_t lifetime_detail::Graph::addNode(const std::shared_ptr<Node>& node) {
    auto* ptr = node.get();
    return add(node, [ptr](Graph& graph, std::size_t at) {
        graph.array(at, ptr->childrenRef(), [](Graph& g, auto at, const auto& child) { g.nodeEdge(at, child); });
        graph.array(at, ptr->changePool, [](Graph& g, auto at, const auto& event) { g.nodeEdge(at, event.first); });
        visitRetainedFilters(graph, at, *ptr);
        graph.array(at, ptr->notifyListeners, [](Graph& graph, auto at, const auto& listener) {
            if (auto node = listener.template retainedMethodContext<&Node::notifyChange>()) graph.nodeEdge(at, node);
            else if (auto node = listener.template retainedMethodContext<&Projectable::onAncestorChanged>()) graph.nodeEdge(at, node);
        });
        if (auto* projectable = dynamic_cast<Projectable*>(ptr)) {
            for (const auto& child : projectable->subParts) graph.nodeEdge(at, child);
            for (const auto& child : projectable->maskParts) graph.nodeEdge(at, child);
            for (const auto& child : projectable->queuedOffscreenParts) graph.nodeEdge(at, child);
        }
        if (auto* path = dynamic_cast<PathDeformer*>(ptr); path && path->driver) {
            auto* adapter = dynamic_cast<PathDeformer::ConnectedDriverAdapter*>(path->driver.get());
            if (adapter) {
                auto id = graph.add(path->driver, [adapter](Graph& graph, auto at) {
                    if (auto* pendulum = dynamic_cast<ConnectedPendulumDriver*>(adapter->impl())) graph.nodeEdge(at, pendulum->deformer_);
                    else if (auto* spring = dynamic_cast<ConnectedSpringPendulumDriver*>(adapter->impl())) graph.nodeEdge(at, spring->deformer_);
                });
                graph.edge(at, id);
            }
        }
        if (auto* driver = dynamic_cast<SimplePhysicsDriver*>(ptr)) {
            graph.parameterEdge(at, driver->param_);
            if (driver->system) {
                auto* system = driver->system.get();
                const auto id = graph.add(driver->system, [system](Graph& graph, auto at) {
                    if (auto* pendulum = dynamic_cast<Pendulum*>(system)) graph.nodeEdge(at, pendulum->driver);
                    else if (auto* spring = dynamic_cast<SpringPendulum*>(system)) graph.nodeEdge(at, spring->driver);
                });
                graph.edge(at, id);
            }
        }
        if (auto* part = dynamic_cast<Part*>(ptr)) {
            graph.array(at, part->masks, [](Graph& g, auto at, const auto& mask) { g.nodeEdge(at, mask.maskSrc.lock()); });
        }
        if (auto* drawable = dynamic_cast<Drawable*>(ptr)) {
            graph.array(at, drawable->weldedLinks, [](Graph& g, auto at, const auto& link) { g.nodeEdge(at, link.target.lock()); });
            graph.nodeKeyMap(at, drawable->weldingApplied);
            graph.nodeKeyMap(at, drawable->attachedIndex);
        }
    }, ptr);
}

bool collectRetainedNodeCycles() noexcept {
    // No object is modified until discovery, root marking and all allocations
    // succeed. Memory pressure defers collection to the next safe point.
    try {
        if (!registryAlive.load()) return true;
        auto& state = registry();
        std::lock_guard lock(state.mutex);
        if (state.collecting) return true;
        state.collecting = true;
        struct Reset { bool& flag; ~Reset() { flag = false; } } reset{state.collecting};
        {
            lifetime_detail::Graph graph;
            for (auto it = state.nodes.begin(); it != state.nodes.end();) {
                if (auto node = it->first.lock()) { graph.addNode(node); ++it; }
                else it = state.nodes.erase(it);
            }
            for (auto it = state.parameters.begin(); it != state.parameters.end();) {
                if (auto parameter = it->first.lock()) { graph.addParameter(parameter); ++it; }
                else it = state.parameters.erase(it);
            }
            for (std::size_t i = 0; i < graph.objects.size(); ++i) {
                // Discovering a target can relocate the vector: don't invoke a
                // callable stored in the vector while its storage can move.
                auto visit = graph.objects[i].visit;
                visit(graph, i);
            }
            for (const auto& object : graph.objects) {
                if (object.node) state.nodes.try_emplace(
                    std::shared_ptr<Node>(object.owner, object.node), true);
            }
            std::vector<std::size_t> work;
            work.reserve(graph.objects.size());
            for (std::size_t i = 0; i < graph.objects.size(); ++i) {
                auto& object = graph.objects[i];
                // An unsupported ownership shape or concurrent access must
                // defer collection rather than risk classifying a live owner.
                if (object.owner.use_count() < static_cast<long>(object.incoming + 1)) return false;
                if (object.owner.use_count() > static_cast<long>(object.incoming + 1)) {
                    object.live = true;
                    work.push_back(i);
                }
            }
            for (std::size_t i = 0; i < work.size(); ++i) {
                for (const auto next : graph.objects[work[i]].edges) {
                    if (!graph.objects[next].live) {
                        graph.objects[next].live = true;
                        work.push_back(next);
                    }
                }
            }
            std::size_t retirements = 0;
            std::size_t textures = 0;
            for (const auto& object : graph.objects) {
                if (object.live || !object.node) continue;
                if (auto* drawable = dynamic_cast<Drawable*>(object.node)) {
                    ++retirements;
                    for (const auto& texture : drawable->textures) if (texture) ++textures;
                }
                if (auto* part = dynamic_cast<Part*>(object.node))
                    for (const auto& texture : part->textures) if (texture) ++textures;
                if (auto* projectable = dynamic_cast<Projectable*>(object.node)) {
                    if (projectable->stencil) ++textures;
                    if (projectable->offscreenSurface) {
                        if (projectable->offscreenSurface->stencil) ++textures;
                        for (const auto& texture : projectable->offscreenSurface->textures) if (texture) ++textures;
                    }
                }
            }
            if (retirements) render::detail::reserveDrawableReleases(retirements);
            if (textures) inReserveTextureDisposals(textures);
            // Drop descriptors, never mutate shared backing or live targets.
            // The graph keeps every candidate alive throughout this phase.
            for (auto& object : graph.objects) {
                if (object.live) continue;
                if (object.release) object.release();
                auto* node = object.node;
                if (!node) continue;
                node->childrenRef().clear();
                node->changePool.clear(); node->notifyListeners.clear();
                if (auto* projectable = dynamic_cast<Projectable*>(node)) {
                    projectable->subParts.clear(); projectable->maskParts.clear(); projectable->queuedOffscreenParts.clear();
                }
                if (auto* path = dynamic_cast<PathDeformer*>(node)) path->driver.reset();
                if (auto* driver = dynamic_cast<SimplePhysicsDriver*>(node)) {
                    driver->param_.reset(); driver->system.reset();
                }
                node->preProcessFilters.clear(); node->postProcessFilters.clear();
                if (auto* deformable = dynamic_cast<Deformable*>(node)) {
                    deformable->deformPreProcessFilters.clear(); deformable->deformPostProcessFilters.clear();
                }
                if (auto* part = dynamic_cast<Part*>(node)) part->masks.clear();
                if (auto* drawable = dynamic_cast<Drawable*>(node)) {
                    drawable->weldedLinks.clear();
                    drawable->weldingApplied = {};
                    drawable->attachedIndex = {};
                }
            }
        }
        for (auto it = state.nodes.begin(); it != state.nodes.end();) {
            if (it->first.expired()) it = state.nodes.erase(it); else ++it;
        }
        for (auto it = state.parameters.begin(); it != state.parameters.end();) {
            if (it->first.expired()) it = state.parameters.erase(it); else ++it;
        }
        return true;
    } catch (...) {
        // Destructor and allocation-failure unwinding must never terminate.
        return false;
    }
}
} // namespace nicxlive::core::nodes
