#pragma once

#include "nodes/node.hpp"
#include "nodes/composite.hpp"
#include "nodes/dynamic_composite.hpp"
#include "nodes/part.hpp"
#include "nodes/driver.hpp"
#include "nodes/filter.hpp"
#include "render.hpp"
#include "param/parameter.hpp"
#include "render/graph_builder.hpp"
#include "render/scheduler.hpp"
#include "render/command_emitter.hpp"
#include "texture.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nicxlive::core {

// Forward declarations for automation/animation/physics metadata.
using ::nicxlive::core::param::Parameter;

class Automation {
public:
    virtual ~Automation() = default;
    virtual void update() {}
    virtual void reconstruct(const std::shared_ptr<class Puppet>&) {}
    virtual void finalize(const std::shared_ptr<class Puppet>&) {}
};

class Animation {
public:
    virtual ~Animation() = default;
    virtual void update() {}
    virtual void reconstruct(const std::shared_ptr<class Puppet>&) {}
    virtual void finalize(const std::shared_ptr<class Puppet>&) {}
};

struct PuppetMeta {
    bool preservePixels{false};
    uint32_t thumbnailId{std::numeric_limits<uint32_t>::max()};
};
struct PuppetPhysics {};

class Puppet : public std::enable_shared_from_this<Puppet> {
public:
    Puppet();
    explicit Puppet(const std::shared_ptr<nodes::Node>& rootNode);

    // lifecycle / tasks
    void scanParts(bool reparent = true, const std::shared_ptr<nodes::Node>& node = nullptr);
    void selfSort();
    struct FrameChangeState {
        bool structureDirty{false};
        bool attributeDirty{false};

        bool any() const { return structureDirty || attributeDirty; }
        void mark(nodes::NotifyReason reason) {
            if (reason == nodes::NotifyReason::StructureChanged) {
                structureDirty = true;
            }
            attributeDirty = true;
        }
    };
    FrameChangeState consumeFrameChanges();
    void rebuildRenderTasks(const std::shared_ptr<nodes::Node>& rootNode);
    void updateParametersAndDrivers(const std::shared_ptr<nodes::Node>& rootNode);
    void update();
    void resetDrivers();

    // queries
    std::shared_ptr<nodes::Node> findNode(const std::shared_ptr<nodes::Node>& n, const std::string& name);
    std::shared_ptr<nodes::Node> findNode(const std::shared_ptr<nodes::Node>& n, uint32_t uuid);
    std::ptrdiff_t findParameterIndex(const std::string& name);
    std::shared_ptr<Parameter> findParameter(uint32_t uuid);
    std::shared_ptr<nodes::Node> actualRoot();
    std::shared_ptr<nodes::Node> findNodeByName(const std::string& name);
    std::shared_ptr<nodes::Node> findNodeById(uint32_t uuid);
    std::shared_ptr<Parameter> findParameterByName(const std::string& name);
    std::shared_ptr<Parameter> findParameterById(uint32_t uuid);
    std::vector<std::shared_ptr<nodes::Part>> getAllParts();
    void recordNodeChange(nodes::NotifyReason reason);

    // rendering
    void draw();
    void drawImmediateFallback();
    void rescanNodes();
    void updateTextureState();
    RenderCommandEmitter* commandEmitter();

    // textures
    uint32_t addTextureToSlot(const std::shared_ptr<::nicxlive::core::Texture>& texture);
    void populateTextureSlots();
    std::shared_ptr<::nicxlive::core::Texture> resolveTextureSlot(uint32_t runtimeUUID);
    std::shared_ptr<::nicxlive::core::Texture> findTextureByRuntimeUUID(uint32_t uuid);
    void setThumbnail(const std::shared_ptr<::nicxlive::core::Texture>& texture);
    std::ptrdiff_t getTextureSlotIndexFor(const std::shared_ptr<::nicxlive::core::Texture>& texture);
    void clearThumbnail(bool deleteTexture = false);
    void setRootNode(const std::shared_ptr<nodes::Node>& node);

    // Data
    PuppetMeta meta{};
    PuppetPhysics physics{};
    std::shared_ptr<nodes::Node> root{};
    std::vector<std::shared_ptr<Parameter>> parameters{};
    std::vector<std::shared_ptr<Automation>> automation{};
    std::map<std::string, std::shared_ptr<Animation>> animations{};
    std::vector<std::shared_ptr<::nicxlive::core::Texture>> textureSlots{};
    bool renderParameters{true};
    bool enableDrivers{true};
    nodes::Transform transform{};

private:
    std::shared_ptr<nodes::Node> puppetRootNode{};
    std::vector<std::shared_ptr<nodes::Node>> rootParts{};
    std::vector<std::shared_ptr<nodes::Driver>> drivers{};
    std::map<std::shared_ptr<Parameter>, std::weak_ptr<nodes::Driver>> drivenParameters{};

    std::unique_ptr<RenderCommandEmitter> commandEmitterOwned{};
    ::nicxlive::core::RenderGraphBuilder renderGraph{};
    std::shared_ptr<::nicxlive::core::RenderBackend> renderBackend{};
    ::nicxlive::core::TaskScheduler renderScheduler{};
    ::nicxlive::core::RenderContext renderContext{};
    ::nicxlive::core::RenderCommandEmitter* commandEmitterRaw{nullptr};

    FrameChangeState pendingFrameChanges{};
    bool schedulerCacheValid{false};
    bool forceFullRebuild{true};

    void scanPartsRecurse(const std::shared_ptr<nodes::Node>& node, bool driversOnly = false);
    void selfSortInternal() { selfSort(); }
    void rebuildRenderTasksInternal(const std::shared_ptr<nodes::Node>& rootNode) { rebuildRenderTasks(rootNode); }
    void drawImmediateFallbackInternal() { drawImmediateFallback(); }
    std::shared_ptr<Parameter> findParameterInternal(uint32_t uuid) { return findParameter(uuid); }
};

} // namespace nicxlive::core
