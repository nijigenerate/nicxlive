#include "puppet.hpp"
#include "render/common.hpp"
#include "render/graph_builder.hpp"
#include "render/scheduler.hpp"
#include "render/command_emitter.hpp"
#include "nodes/projectable.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace nicxlive::core {

using nodes::Composite;
using nodes::Driver;
using nodes::DynamicComposite;
using nodes::Node;
using nodes::NotifyReason;
using nodes::Part;
using nodes::Transform;

namespace {
class SimpleRenderCommandEmitter : public render::QueueCommandEmitter {
public:
    explicit SimpleRenderCommandEmitter(const std::shared_ptr<render::QueueRenderBackend>& backend)
        : render::QueueCommandEmitter(backend) {}
};
} // namespace

Puppet::Puppet() {
    puppetRootNode = std::make_shared<Node>();
    meta = PuppetMeta{};
    physics = PuppetPhysics{};
    root = std::make_shared<Node>();
    root->setParent(puppetRootNode);
    root->name = "Root";
    transform = Transform{};
    auto backendQueue = std::make_shared<render::QueueRenderBackend>();
    renderBackend = backendQueue;
    commandEmitterOwned = std::make_unique<SimpleRenderCommandEmitter>(backendQueue);
    commandEmitterRaw = commandEmitterOwned.get();
    renderGraph = RenderGraphBuilder{};
    renderScheduler = TaskScheduler{};
    renderContext.renderGraph = &renderGraph;
    renderContext.renderBackend = renderBackend.get();
    renderContext.gpuState = RenderGpuState::init();
    setCurrentRenderBackend(renderBackend);
    forceFullRebuild = true;
}

Puppet::Puppet(const std::shared_ptr<Node>& rootNode) : Puppet() {
    root = rootNode;
    if (root) {
        root->setParent(puppetRootNode);
        root->name = "Root";
    }
    scanParts(true, root);
    selfSort();
}

void Puppet::scanPartsRecurse(const std::shared_ptr<Node>& node, bool driversOnly) {
    if (!node) return;

    if (auto drv = std::dynamic_pointer_cast<Driver>(node)) {
        drivers.push_back(drv);
    } else if (!driversOnly) {
        if (auto dcomp = std::dynamic_pointer_cast<DynamicComposite>(node)) {
            rootParts.push_back(dcomp);
            driversOnly = true;
        } else if (auto comp = std::dynamic_pointer_cast<Composite>(node)) {
            rootParts.push_back(comp);
            driversOnly = true;
        } else if (auto part = std::dynamic_pointer_cast<Part>(node)) {
            part->ignorePuppet = false;
            rootParts.push_back(part);
        }
    }

    for (auto& child : node->childrenRef()) {
        scanPartsRecurse(child, driversOnly);
    }
}

void Puppet::scanParts(bool reparent, const std::shared_ptr<Node>& node) {
    rootParts.clear();
    drivers.clear();
    drivenParameters.clear();

    scanPartsRecurse(node);

    if (reparent) {
        if (puppetRootNode) puppetRootNode->clearChildren();
        if (node) {
            node->setParent(puppetRootNode);
        }
    }
}

void Puppet::selfSort() {
    std::stable_sort(rootParts.begin(), rootParts.end(), [](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
        if (!a || !b) return false;
        return a->zSort() > b->zSort();
    });
}

Puppet::FrameChangeState Puppet::consumeFrameChanges() {
    auto flags = pendingFrameChanges;
    if (forceFullRebuild) {
        flags.structureDirty = true;
        flags.attributeDirty = true;
        forceFullRebuild = false;
    }
    pendingFrameChanges = FrameChangeState{};
    return flags;
}

void Puppet::rebuildRenderTasks(const std::shared_ptr<Node>& rootNode) {
    if (!rootNode) return;
    renderScheduler.clearTasks();
    auto rootForTasks = rootNode;
    rootForTasks->registerRenderTasks(renderScheduler);
    renderScheduler.addTask(TaskOrder::Parameters, TaskKind::Parameters, [this, rootForTasks](RenderContext&) {
        updateParametersAndDrivers(rootForTasks);
    });
    schedulerCacheValid = true;
}

void Puppet::updateParametersAndDrivers(const std::shared_ptr<Node>& rootNode) {
    if (!rootNode) return;
    if (renderParameters) {
        for (auto& param : parameters) {
            if (!enableDrivers) {
                if (param) param->update();
            } else {
                if (param && drivenParameters.find(param) == drivenParameters.end()) {
                    param->update();
                }
            }
        }
    }

    rootNode->transformChanged();

    if (renderParameters && enableDrivers) {
        for (auto& drv : drivers) {
            if (drv && drv->renderEnabled()) {
                drv->updateDriver();
            }
        }
    }
}

std::shared_ptr<Node> Puppet::findNode(const std::shared_ptr<Node>& n, const std::string& name) {
    if (!n) return nullptr;
    if (n->name == name) return n;
    for (auto& child : n->childrenRef()) {
        if (auto found = findNode(child, name)) return found;
    }
    return nullptr;
}

std::shared_ptr<Node> Puppet::findNode(const std::shared_ptr<Node>& n, uint32_t uuid) {
    if (!n) return nullptr;
    if (n->uuid == uuid) return n;
    for (auto& child : n->childrenRef()) {
        if (auto found = findNode(child, uuid)) return found;
    }
    return nullptr;
}

void Puppet::update() {
    transform.update();

    for (auto& auto_ : automation) {
        if (auto_) auto_->update();
    }

    auto rootNode = actualRoot();
    if (!rootNode) return;

    renderContext.renderGraph = &renderGraph;
    renderContext.renderBackend = renderBackend.get();
    renderContext.gpuState = RenderGpuState::init();

    bool pendingStructure = pendingFrameChanges.structureDirty;
    if (forceFullRebuild || !schedulerCacheValid || pendingStructure) {
        rebuildRenderTasks(rootNode);
    }

    renderScheduler.executeRange(renderContext, TaskOrder::Init, TaskOrder::Parameters);

    auto frameChanges = consumeFrameChanges();
    if (frameChanges.structureDirty) {
        rebuildRenderTasks(rootNode);
        renderScheduler.executeRange(renderContext, TaskOrder::Init, TaskOrder::Parameters);
        auto additional = consumeFrameChanges();
        frameChanges.attributeDirty = frameChanges.attributeDirty || additional.attributeDirty;
        frameChanges.structureDirty = frameChanges.structureDirty || additional.structureDirty;
    }

    std::fprintf(stderr, "[nicxlive] puppet.update tasks total=%zu rb=%zu r=%zu re=%zu\n",
                 renderScheduler.totalTaskCount(),
                 renderScheduler.taskCount(TaskOrder::RenderBegin),
                 renderScheduler.taskCount(TaskOrder::Render),
                 renderScheduler.taskCount(TaskOrder::RenderEnd));
    renderGraph.beginFrame();
    renderScheduler.executeRange(renderContext, TaskOrder::PreProcess, TaskOrder::Final);
    std::fprintf(stderr, "[nicxlive] puppet.update graph depth=%zu rootItems=%zu empty=%d\n",
                 renderGraph.passDepth(),
                 renderGraph.rootItemCount(),
                 renderGraph.empty() ? 1 : 0);
}

void Puppet::resetDrivers() {
    for (auto& driver : drivers) {
        if (driver) driver->reset();
    }
}

std::ptrdiff_t Puppet::findParameterIndex(const std::string& name) {
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (parameters[i] && parameters[i]->name == name) {
            return static_cast<std::ptrdiff_t>(i);
        }
    }
    return -1;
}

std::shared_ptr<Parameter> Puppet::findParameter(uint32_t uuid) {
    for (auto& p : parameters) {
        if (p && p->uuid == uuid) return p;
    }
    return nullptr;
}

std::shared_ptr<nodes::Node> Puppet::actualRoot() {
    auto node = root;
    while (node && node->parentPtr() && node->parentPtr() != puppetRootNode) {
        node = node->parentPtr();
    }
    return node;
}

void Puppet::draw() {
    if (!commandEmitterOwned || !renderBackend) {
        std::fprintf(stderr, "[nicxlive] puppet.draw fallback reason=no_emitter_or_backend\n");
        drawImmediateFallback();
        return;
    }
    if (renderGraph.empty()) {
        std::fprintf(stderr, "[nicxlive] puppet.draw fallback reason=graph_empty rootItems=%zu depth=%zu\n",
                     renderGraph.rootItemCount(),
                     renderGraph.passDepth());
        drawImmediateFallback();
        return;
    }
    commandEmitterOwned->beginFrame(renderBackend.get(), renderContext.gpuState);
    renderGraph.playback(commandEmitterOwned.get());
    commandEmitterOwned->endFrame(renderBackend.get(), renderContext.gpuState);
}

void Puppet::drawImmediateFallback() {
    selfSort();
    for (auto& rp : rootParts) {
        if (rp && rp->renderEnabled()) {
            rp->drawOne();
        }
    }
}

void Puppet::rescanNodes() {
    auto node = actualRoot();
    scanParts(false, node);
}

void Puppet::updateTextureState() {
    for (auto& tex : textureSlots) {
        if (tex) tex->setFiltering(meta.preservePixels);
    }
}

RenderCommandEmitter* Puppet::commandEmitter() {
    return commandEmitterRaw;
}

void Puppet::setRenderBackend(const std::shared_ptr<::nicxlive::core::RenderBackend>& backend) {
    if (!backend) return;
    renderBackend = backend;
    renderContext.renderBackend = renderBackend.get();
    setCurrentRenderBackend(renderBackend);
    if (auto queueBackend = std::dynamic_pointer_cast<render::QueueRenderBackend>(renderBackend)) {
        commandEmitterOwned = std::make_unique<SimpleRenderCommandEmitter>(queueBackend);
        commandEmitterRaw = commandEmitterOwned.get();
    }
}

bool Puppet::isRenderGraphEmpty() const {
    return renderGraph.empty();
}

std::size_t Puppet::rootPartCount() const {
    return rootParts.size();
}

std::shared_ptr<nodes::Node> Puppet::findNodeByName(const std::string& name) {
    return findNode(root, name);
}

std::shared_ptr<nodes::Node> Puppet::findNodeById(uint32_t uuid) {
    return findNode(root, uuid);
}

std::shared_ptr<Parameter> Puppet::findParameterByName(const std::string& name) {
    auto idx = findParameterIndex(name);
    if (idx < 0 || static_cast<std::size_t>(idx) >= parameters.size()) return nullptr;
    return parameters[static_cast<std::size_t>(idx)];
}

std::shared_ptr<Parameter> Puppet::findParameterById(uint32_t uuid) {
    return findParameter(uuid);
}

std::vector<std::shared_ptr<nodes::Part>> Puppet::getAllParts() {
    std::vector<std::shared_ptr<nodes::Part>> parts;
    std::function<void(const std::shared_ptr<Node>&)> dfs = [&](const std::shared_ptr<Node>& n) {
        if (!n) return;
        if (auto p = std::dynamic_pointer_cast<nodes::Part>(n)) {
            parts.push_back(p);
        }
        for (auto& c : n->childrenRef()) dfs(c);
    };
    dfs(root);
    return parts;
}

uint32_t Puppet::addTextureToSlot(const std::shared_ptr<Texture>& texture) {
    auto it = std::find(textureSlots.begin(), textureSlots.end(), texture);
    if (it == textureSlots.end()) {
        textureSlots.push_back(texture);
        return static_cast<uint32_t>(textureSlots.size() - 1);
    }
    return static_cast<uint32_t>(std::distance(textureSlots.begin(), it));
}

void Puppet::populateTextureSlots() {
    textureSlots.clear();
    for (auto& part : getAllParts()) {
        if (!part) continue;
        for (auto id : part->textureIds) {
            if (id == static_cast<int>(std::numeric_limits<uint32_t>::max())) continue;
            // runtimeUUID として扱う
            auto tex = findTextureByRuntimeUUID(static_cast<uint32_t>(id));
            if (!tex) {
                // placeholder texture allocation
                tex = std::make_shared<Texture>();
                tex->setRuntimeUUID(static_cast<uint32_t>(id));
                textureSlots.push_back(tex);
            }
            if (std::find(textureSlots.begin(), textureSlots.end(), tex) == textureSlots.end()) {
                textureSlots.push_back(tex);
            }
        }
    }
}

std::shared_ptr<Texture> Puppet::findTextureByRuntimeUUID(uint32_t uuid) {
    for (auto& tex : textureSlots) {
        if (tex && tex->getRuntimeUUID() == uuid) return tex;
    }
    return nullptr;
}

std::shared_ptr<Texture> Puppet::resolveTextureSlot(uint32_t runtimeUUID) {
    if (runtimeUUID < textureSlots.size()) {
        if (auto byIndex = textureSlots[static_cast<std::size_t>(runtimeUUID)]) {
            return byIndex;
        }
    }
    if (auto t = findTextureByRuntimeUUID(runtimeUUID)) return t;
    auto tex = std::make_shared<Texture>();
    tex->setRuntimeUUID(runtimeUUID);
    textureSlots.push_back(tex);
    return tex;
}

void Puppet::setThumbnail(const std::shared_ptr<Texture>& texture) {
    if (!texture) return;
    if (meta.thumbnailId == std::numeric_limits<uint32_t>::max()) {
        meta.thumbnailId = addTextureToSlot(texture);
    } else if (meta.thumbnailId < textureSlots.size()) {
        textureSlots[meta.thumbnailId] = texture;
    }
}

std::ptrdiff_t Puppet::getTextureSlotIndexFor(const std::shared_ptr<Texture>& texture) {
    auto it = std::find(textureSlots.begin(), textureSlots.end(), texture);
    if (it == textureSlots.end()) return -1;
    return static_cast<std::ptrdiff_t>(std::distance(textureSlots.begin(), it));
}

void Puppet::clearThumbnail(bool deleteTexture) {
    if (deleteTexture && meta.thumbnailId < textureSlots.size()) {
        textureSlots.erase(textureSlots.begin() + meta.thumbnailId);
    }
    meta.thumbnailId = std::numeric_limits<uint32_t>::max();
}

void Puppet::setRootNode(const std::shared_ptr<Node>& node) {
    if (root) {
        root->setParent(nullptr);
    }
    if (node) {
        node->setParent(puppetRootNode);
    }
    root = node;
}

void Puppet::recordNodeChange(nodes::NotifyReason reason) {
    pendingFrameChanges.mark(reason);
    if (reason == nodes::NotifyReason::StructureChanged) {
        forceFullRebuild = true;
    }
}

::nicxlive::core::serde::SerdeException Puppet::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    try {
        std::size_t topKeys = 0;
        bool hasNodesKey = false;
        for (const auto& kv : data) {
            ++topKeys;
            if (kv.first == "nodes") hasNodesKey = true;
        }
        std::fprintf(stderr, "[nicxlive] puppet.deserialize topKeys=%zu hasNodes=%d\n", topKeys, hasNodesKey ? 1 : 0);
        if (auto metaNode = data.get_child_optional("meta")) {
            if (auto preserve = metaNode->get_optional<bool>("preservePixels")) meta.preservePixels = *preserve;
            if (auto thumb = metaNode->get_optional<uint32_t>("thumbnailId")) meta.thumbnailId = *thumb;
        }
        if (auto rootNode = data.get_child_optional("nodes")) {
            std::fprintf(stderr, "[nicxlive] puppet.deserialize nodes childCount=%zu\n", rootNode->size());
            std::string type = rootNode->get<std::string>("type", "Node");
            auto loadedRoot = nodes::Node::inInstantiateNode(type);
            if (!loadedRoot) loadedRoot = std::make_shared<nodes::Node>();
            loadedRoot->deserializeFromFghj(*rootNode);
            std::fprintf(stderr, "[nicxlive] puppet.deserialize loadedRoot children=%zu\n", loadedRoot->childrenList().size());
            loadedRoot->setParent(puppetRootNode);
            loadedRoot->setPuppet(shared_from_this());
            loadedRoot->reconstruct();
            loadedRoot->finalize();
            root = loadedRoot;
        }
        rescanNodes();
    } catch (const std::exception& ex) {
        return std::string(ex.what());
    }
    return std::nullopt;
}

std::shared_ptr<nodes::Node> resolvePuppetNodeById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid) {
    if (!puppet || uuid == 0) return nullptr;
    return puppet->findNodeById(uuid);
}

} // namespace nicxlive::core
