#include <stdexcept>
#include "puppet.hpp"
#include "puppet_root_sort.hpp"
#include "resource.hpp"
#include "compat/native_access_fault.hpp"
#include "compat/source_allocator.hpp"
#include "render/common.hpp"
#include "render/graph_builder.hpp"
#include "render/scheduler.hpp"
#include "render/command_emitter.hpp"
#include "render/profiler.hpp"
#include "debug_log.hpp"
#include "nodes/projectable.hpp"
#include "nodes/mesh_group.hpp"
#include "nodes/path_deformer.hpp"
#include "nodes/grid_deformer.hpp"
#include "timing.hpp"
#include "utils/snapshot.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <new>

namespace nicxlive::core {
std::size_t Puppet::toHash() {
    const auto address=reinterpret_cast<std::size_t>(objectIdentity());
    return address^(address>>4);
}
bool Puppet::opEquals(std::shared_ptr<Puppet> other) {
    return other && objectIdentity()==other->objectIdentity();
}
int Puppet::opCmp(std::shared_ptr<Puppet>) {
    ngRegisterModelResourceClassNames();
    std::string message="need opCmp for class ";
    message+=d_runtime::ngObjectClassName(typeid(*this)).view();
    throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
        d_runtime::RetainedString::fromStaticLiteral("object.d"),195);
}
bool ngPuppetObjectEquals(std::shared_ptr<Puppet> left,std::shared_ptr<Puppet> right) {
    return common::ngClassObjectEquals(std::move(left),std::move(right),ngRegisterModelResourceClassNames);
}

d_runtime::RetainedString Puppet::toString() const {
    // The source keeps one mutable line descriptor for this traversal only.
    std::vector<bool> lineSet;
    const auto branch = [&](auto&& recurse, std::shared_ptr<nodes::Node> node, int indent) -> std::string {
        auto current = d_runtime::ngRequireObject(node);
        lineSet.push_back(!current->childrenList().empty());
        std::string indentation;
        if (indent != 0) {
            for (std::size_t index = 1; index < lineSet.size(); ++index)
                indentation += lineSet[index - 1] ? "\342\224\202 " : "  ";
        }
        const auto prefix = current->childrenList().empty() ? "" : "\342\225\255\342\224\200";
        const auto type = current->typeId();
        const auto name = current->getName();
        const auto uuid = current->getUUID();
        std::string result(prefix);
        result += '[';
        result.append(type.view());
        result += "] ";
        result.append(name.view());
        result += " <";
        result += std::to_string(uuid);
        result += ">\n";
        const auto children = current->childrenList();
        for (std::size_t index = 0; index < children.size(); ++index) {
            const auto child = children[index];
            const char* terminal = "\342\224\234\342\206\222";
            if (index == current->childrenList().size() - 1) {
                terminal = "\342\225\260\342\206\222";
                lineSet[static_cast<std::size_t>(indent)] = false;
            }
            const auto nested = recurse(recurse, child, indent + 1);
            result += indentation;
            result += terminal;
            result += nested;
        }
        lineSet.pop_back();
        return result;
    };
    return d_runtime::RetainedString(branch(branch, root, 0));
}

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

bool perfEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_PROFILE");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

struct PuppetPerfWindow {
    std::chrono::steady_clock::time_point lastReport{};
    std::size_t frames{0};
    double totalMs{0.0};
    double automationMs{0.0};
    double initParamMs{0.0};
    double preFinalMs{0.0};
    double rebuildMs{0.0};

    void add(double total, double automation, double initParam, double preFinal, double rebuild) {
        if (!perfEnabled()) return;
        frames++;
        totalMs += total;
        automationMs += automation;
        initParamMs += initParam;
        preFinalMs += preFinal;
        rebuildMs += rebuild;
        auto now = std::chrono::steady_clock::now();
        if (lastReport.time_since_epoch().count() == 0) {
            lastReport = now;
            return;
        }
        if ((now - lastReport) < std::chrono::seconds(1)) return;
        const double frameDiv = frames ? static_cast<double>(frames) : 1.0;
        std::fprintf(stderr,
                     "[nicxlive][perf/update] frames=%zu total=%.3fms automation=%.3fms init=%.3fms prefinal=%.3fms rebuild=%.3fms\n",
                     frames,
                     totalMs / frameDiv,
                     automationMs / frameDiv,
                     initParamMs / frameDiv,
                     preFinalMs / frameDiv,
                     rebuildMs / frameDiv);
        frames = 0;
        totalMs = 0.0;
        automationMs = 0.0;
        initParamMs = 0.0;
        preFinalMs = 0.0;
        rebuildMs = 0.0;
        lastReport = now;
    }
};

PuppetPerfWindow& puppetPerfWindow() {
    static PuppetPerfWindow window;
    return window;
}
} // namespace

namespace {
template<class Construct> std::shared_ptr<Puppet> createOwnedPuppetMemory(Construct&& construct) {
    d_runtime::SourceAllocator<Puppet> allocator;
    auto* storage = allocator.allocate(1);
    try {
        // The actual storage-only tag constructor executes in its caller's lexical
        // context, outside allocation translation, before creating the owner.
        std::forward<Construct>(construct)(storage);
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    return std::shared_ptr<Puppet>(storage, [allocator](Puppet* object) mutable {
        std::destroy_at(object);
        allocator.deallocate(object, 1);
    }, allocator);
}
} // namespace

std::shared_ptr<Puppet> Puppet::create() {
    auto puppet = createOwnedPuppetMemory([](Puppet* storage) {
        ::new (static_cast<void*>(storage)) Puppet(DeferredConstruction{});
    });
    puppet->constructOwned();
    return puppet;
}

std::shared_ptr<Puppet> Puppet::create(std::shared_ptr<Node> rootNode) {
    auto puppet = createOwnedPuppetMemory([](Puppet* storage) {
        ::new (static_cast<void*>(storage)) Puppet(DeferredConstruction{});
    });
    puppet->constructOwned(std::move(rootNode));
    return puppet;
}

void Puppet::constructOwned() {
    const auto self = shared_from_this();
    puppetRootNode = Node::createForPuppet(self);
    meta = std::allocate_shared<PuppetMeta>(d_runtime::SourceAllocator<PuppetMeta>{});
    physics = std::allocate_shared<PuppetPhysics>(d_runtime::SourceAllocator<PuppetPhysics>{});
    root = Node::create(puppetRootNode);
    root->setName(common::NullableString::fromStaticLiteral("Root"));
    transform = Transform{};
    initializeRenderState();
}

void Puppet::constructOwned(std::shared_ptr<Node> rootNode) {
    const auto self = shared_from_this();
    meta = std::allocate_shared<PuppetMeta>(d_runtime::SourceAllocator<PuppetMeta>{});
    physics = std::allocate_shared<PuppetPhysics>(d_runtime::SourceAllocator<PuppetPhysics>{});
    root = std::move(rootNode);
    puppetRootNode = Node::createForPuppet(self);
    root->setName(common::NullableString::fromStaticLiteral("Root"));
    scanParts(true, root);
    transform = Transform{};
    selfSort();
    initializeRenderState();
}

Puppet::Puppet() {
    puppetRootNode = std::make_shared<Node>();
    meta = std::make_shared<PuppetMeta>();
    physics = std::make_shared<PuppetPhysics>();
    root = std::make_shared<Node>();
    root->setParent(puppetRootNode);
    root->name = "Root";
    transform = Transform{};
    initializeRenderState();
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

void Puppet::initializeRenderState() {
    renderBackend = getCurrentRenderBackend();
    if (!renderBackend) renderBackend = std::make_shared<render::QueueRenderBackend>();
    if (auto backendQueue = std::dynamic_pointer_cast<render::QueueRenderBackend>(renderBackend)) {
        commandEmitterOwned = std::make_unique<SimpleRenderCommandEmitter>(backendQueue);
        commandEmitterRaw = commandEmitterOwned.get();
    } else {
        commandEmitterOwned = renderBackend->createCommandEmitter();
        commandEmitterRaw = commandEmitterOwned.get();
    }
    renderGraph = RenderGraphBuilder{};
    renderScheduler = std::make_shared<TaskScheduler>();
    renderContext.renderGraph = &renderGraph;
    renderContext.renderBackend = renderBackend.get();
    renderContext.gpuState = RenderGpuState::init();
    // Preserve an explicit host backend, but keep standalone loading usable.
    if (!getCurrentRenderBackend()) setCurrentRenderBackend(renderBackend);
    forceFullRebuild = true;
}

void Puppet::scanPartsRecurse(const std::shared_ptr<Node>& node, bool driversOnly) {
    if (!node) return;

    if (auto drv = std::dynamic_pointer_cast<Driver>(node)) {
        drivers.push_back(drv);
        static int sDriverTypeLog = 0;
        if (sDriverTypeLog < 64) {
            NJCX_DBG_LOG("[nicxlive] scan-driver uuid=%u type=%s name=%s\n",
                         drv->uuid, drv->typeId().c_str(), drv->name.c_str());
            ++sDriverTypeLog;
        }
        const auto affectedParameters = drv->getAffectedParameters();
        for (auto param : affectedParameters) drivenParameters[param] = drv;
    } else if (!driversOnly) {
        if (auto dcomp = std::dynamic_pointer_cast<DynamicComposite>(node)) {
            dcomp->scanParts();
            rootParts.push_back(dcomp);
            driversOnly = true;
        } else if (auto comp = std::dynamic_pointer_cast<Composite>(node)) {
            comp->scanParts();
            rootParts.push_back(comp);
            driversOnly = true;
        } else if (auto part = std::dynamic_pointer_cast<Part>(node)) {
            part->ignorePuppet = false;
            rootParts.push_back(part);
        }
    }

    const auto children = d_runtime::ngRequireObject(node)->childrenList();
    for (auto child : children) scanPartsRecurse(child, driversOnly);
}

void Puppet::scanParts(bool reparent, const std::shared_ptr<Node>& node) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    rootParts.clear();
    drivers.clear();
    drivenParameters.clear();

    scanPartsRecurse(node);

    if (reparent) {
        {
            const auto internalRoot = puppetRootNode;
            if (internalRoot) internalRoot->clearChildren();
        }
        d_runtime::ngRequireObject(node)->setParent(puppetRootNode);
    }
}

void Puppet::selfSort() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    puppet_sort_detail::phobosStableNodeSort(rootParts, [](std::shared_ptr<Node> a, std::shared_ptr<Node> b) {
        const auto az = d_runtime::ngRequireObject(a)->zSort();
        const auto bz = d_runtime::ngRequireObject(b)->zSort();
        return az > bz;
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
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto capturedRoot = rootNode;
    if (!capturedRoot) return;
    d_runtime::ngRequireObject(renderScheduler)->clearTasks();
    // The actual Source overrides now receive the current nullable class handle.
    capturedRoot->registerRenderTasks(renderScheduler);
    auto rootForTasks = capturedRoot;
    d_runtime::ngRequireObject(renderScheduler)->addTask(TaskOrder::Parameters, TaskKind::Parameters, [this, rootForTasks](RenderContext&) {
        updateParametersAndDrivers(rootForTasks);
    });
    schedulerCacheValid = true;
}

void Puppet::updateParametersAndDrivers(const std::shared_ptr<Node>& rootNode) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto capturedRoot = rootNode;
    if (!capturedRoot) return;
    static int sUpdLog = 0;
    if (renderParameters) {
        const auto capturedParameters = parameters;
        for (auto param : capturedParameters) {
            if (!enableDrivers || drivenParameters.find(param) == drivenParameters.end())
                d_runtime::ngRequireObject(param)->update();
        }
    }

    capturedRoot->transformChanged();

    if (renderParameters && enableDrivers) {
        std::size_t ranDrivers = 0;
        const auto capturedDrivers = drivers;
        for (auto drv : capturedDrivers) {
            if (!drv) continue;
            if (drv->renderEnabled()) {
                drv->updateDriver();
                ++ranDrivers;
            }
        }
        if (sUpdLog < 20) {
            NJCX_DBG_LOG("[nicxlive] upd-drivers total=%zu ran=%zu rootParts=%zu\n",
                         drivers.size(), ranDrivers, rootParts.size());
            ++sUpdLog;
        }
    }
}

std::shared_ptr<Node> Puppet::findNode(const std::shared_ptr<Node>& n, const std::string& name) {
    return findNode(n, d_runtime::RetainedString(name));
}

std::shared_ptr<Node> Puppet::findNode(std::shared_ptr<Node> node, d_runtime::RetainedString name) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (d_runtime::ngRequireObject(node)->getName().view() == name.view()) return node;
    const auto children = node->childrenList();
    for (auto child : children) {
        if (auto found = findNode(child, name)) return found;
    }
    return nullptr;
}

std::shared_ptr<Node> Puppet::findNode(std::shared_ptr<Node> n, uint32_t uuid) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (d_runtime::ngRequireObject(n)->getUUID() == uuid) return n;
    const auto children = n->childrenList();
    for (auto child : children) {
        if (auto found = findNode(child, uuid)) return found;
    }
    return nullptr;
}

void Puppet::update() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto updateProfile = render::profileScope("Puppet.update.total");
    const auto totalStart = std::chrono::steady_clock::now();
    double automationMs = 0.0;
    double initParamMs = 0.0;
    double preFinalMs = 0.0;
    double rebuildMs = 0.0;
    transform.update();

    const auto automationStart = std::chrono::steady_clock::now();
    const auto capturedAutomation = automation;
    for (auto item : capturedAutomation) d_runtime::ngRequireObject(item)->update();
    automationMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - automationStart).count();

    auto rootNode = actualRoot();
    if (!rootNode) return;

    renderContext.renderGraph = &renderGraph;
    renderContext.renderBackend = renderBackend.get();
    renderContext.gpuState = RenderGpuState::init();

    bool pendingStructure = pendingFrameChanges.structureDirty;
    NJCX_DBG_LOG("[nicxlive] frameChange pending structure=%d attr=%d force=%d cache=%d\n",
                 pendingFrameChanges.structureDirty ? 1 : 0,
                 pendingFrameChanges.attributeDirty ? 1 : 0,
                 forceFullRebuild ? 1 : 0,
                 schedulerCacheValid ? 1 : 0);
    if (forceFullRebuild || !schedulerCacheValid || pendingStructure) {
        const auto rebuildStart = std::chrono::steady_clock::now();
        rebuildRenderTasks(rootNode);
        rebuildMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rebuildStart).count();
    }

    const auto initParamStart = std::chrono::steady_clock::now();
    d_runtime::ngRequireObject(renderScheduler)->executeRange(renderContext, TaskOrder::Init, TaskOrder::Parameters);
    initParamMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - initParamStart).count();

    auto frameChanges = consumeFrameChanges();
    NJCX_DBG_LOG("[nicxlive] frameChange consumed1 structure=%d attr=%d\n",
                 frameChanges.structureDirty ? 1 : 0,
                 frameChanges.attributeDirty ? 1 : 0);
    if (frameChanges.structureDirty) {
        const auto rebuildStart = std::chrono::steady_clock::now();
        rebuildRenderTasks(rootNode);
        rebuildMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rebuildStart).count();
        const auto initParamRetryStart = std::chrono::steady_clock::now();
        d_runtime::ngRequireObject(renderScheduler)->executeRange(renderContext, TaskOrder::Init, TaskOrder::Parameters);
        initParamMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - initParamRetryStart).count();
        auto additional = consumeFrameChanges();
        NJCX_DBG_LOG("[nicxlive] frameChange consumed2 structure=%d attr=%d\n",
                     additional.structureDirty ? 1 : 0,
                     additional.attributeDirty ? 1 : 0);
        frameChanges.attributeDirty = frameChanges.attributeDirty || additional.attributeDirty;
        frameChanges.structureDirty = frameChanges.structureDirty || additional.structureDirty;
    }

    NJCX_DBG_LOG("[nicxlive] puppet.update tasks total=%zu rb=%zu r=%zu re=%zu\n",
                 d_runtime::ngRequireObject(renderScheduler)->totalTaskCount(),
                 d_runtime::ngRequireObject(renderScheduler)->taskCount(TaskOrder::RenderBegin),
                 d_runtime::ngRequireObject(renderScheduler)->taskCount(TaskOrder::Render),
                 d_runtime::ngRequireObject(renderScheduler)->taskCount(TaskOrder::RenderEnd));
    renderGraph.beginFrame();
    const auto preFinalStart = std::chrono::steady_clock::now();
    d_runtime::ngRequireObject(renderScheduler)->executeRange(renderContext, TaskOrder::PreProcess, TaskOrder::Final);
    preFinalMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - preFinalStart).count();
    NJCX_DBG_LOG("[nicxlive] puppet.update graph depth=%zu rootItems=%zu empty=%d\n",
                 renderGraph.passDepth(),
                 renderGraph.rootItemCount(),
                 renderGraph.empty() ? 1 : 0);
    render::renderProfilerFrameCompleted();
    const double totalMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - totalStart).count();
    puppetPerfWindow().add(totalMs, automationMs, initParamMs, preFinalMs, rebuildMs);
}

void Puppet::resetDrivers() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto capturedDrivers = drivers;
    for (auto driver : capturedDrivers) d_runtime::ngRequireObject(driver)->reset();
    inUpdate();
}

std::ptrdiff_t Puppet::findParameterIndex(const std::string& name) {
    return findParameterIndex(d_runtime::RetainedString(name));
}

std::ptrdiff_t Puppet::findParameterIndex(d_runtime::RetainedString name) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto capturedParameters = parameters;
    for (std::size_t index = 0; index < capturedParameters.size(); ++index) {
        const auto parameter = capturedParameters[index];
        if (d_runtime::ngRequireObject(parameter)->getName().view() == name.view())
            return static_cast<std::ptrdiff_t>(index);
    }
    return -1;
}

std::shared_ptr<Parameter> Puppet::findParameter(uint32_t uuid) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    const auto capturedParameters = parameters;
    for (auto parameter : capturedParameters) {
        if (d_runtime::ngRequireObject(parameter)->getUUID() == uuid) return parameter;
    }
    return nullptr;
}

std::shared_ptr<nodes::Node> Puppet::actualRoot() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto node = root;
    while (node && node->parentPtr() && !ngResourceObjectEquals(node->parentPtr(), puppetRootNode)) {
        node = node->parentPtr();
    }
    return node;
}

void Puppet::draw() {
    auto drawProfile = render::profileScope("Puppet.draw.total");
    if (!commandEmitterOwned || !renderBackend) {
        NJCX_DBG_LOG("[nicxlive] puppet.draw fallback reason=no_emitter_or_backend\n");
        drawImmediateFallback();
        return;
    }
    if (renderGraph.empty()) {
        NJCX_DBG_LOG("[nicxlive] puppet.draw fallback reason=graph_empty rootItems=%zu depth=%zu\n",
                     renderGraph.rootItemCount(),
                     renderGraph.passDepth());
        drawImmediateFallback();
        return;
    }
    commandEmitterOwned->beginFrame(renderBackend.get(), renderContext.gpuState);
    renderGraph.playback(commandEmitterOwned.get());
    commandEmitterOwned->endFrame(renderBackend.get(), renderContext.gpuState);
    ::nicxlive::utils::Snapshot::processPending();
}

void Puppet::drawImmediateFallback() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    selfSort();
    const auto capturedParts = rootParts;
    for (auto part : capturedParts) {
        if (!d_runtime::ngRequireObject(part)->renderEnabled()) continue;
        part->drawOne();
    }
}

void Puppet::applyDeformToChildren() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto filters = findNodesType<nodes::NodeFilter>(root);
    for (const auto filter : filters) {
        filter->applyDeformToChildren(parameters);
    }
}

void Puppet::rescanNodes() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto node = actualRoot();
    scanParts(false, node);
}

void Puppet::updateTextureState() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto capturedTextures = textureSlots;
    for (auto texture : capturedTextures) {
        d_runtime::ngRequireObject(texture)->setFiltering(
            d_runtime::ngRequireObject(meta)->preservePixels ? Filtering::Nearest : Filtering::Linear);
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
    } else {
        commandEmitterOwned = renderBackend->createCommandEmitter();
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
    return findNodeByName(d_runtime::RetainedString(name));
}

std::shared_ptr<nodes::Node> Puppet::findNodeByName(d_runtime::RetainedString name) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return findNode(root, name);
}

std::shared_ptr<nodes::Node> Puppet::findNodeById(uint32_t uuid) {
    return findNode(root, uuid);
}

std::shared_ptr<Parameter> Puppet::findParameterByName(const std::string& name) {
    return findParameterByName(d_runtime::RetainedString(name));
}

std::shared_ptr<Parameter> Puppet::findParameterByName(d_runtime::RetainedString name) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    // Evaluate the original array operand before the virtual index expression.
    const auto capturedParameters = parameters;
    const auto index = findParameterIndex(name);
    return capturedParameters.at(static_cast<std::size_t>(index));
}

std::shared_ptr<Parameter> Puppet::findParameterById(uint32_t uuid) {
    return findParameter(uuid);
}

common::SharedArray<std::shared_ptr<nodes::Part>> Puppet::getAllParts() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return findNodesType<nodes::Part>(root);
}

uint32_t Puppet::addTextureToSlot(const std::shared_ptr<Texture>& texture) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto target = texture;
    if (getTextureSlotIndexFor(target) < 0) {
        textureSlots.push_back(target);
    }
    // Original D returns the final slot even when the texture already exists.
    return static_cast<uint32_t>(textureSlots.size()) - 1u;
}

void Puppet::populateTextureSlots() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!textureSlots.empty()) textureSlots.resize(0);
    const auto parts = getAllParts();
    for (auto part : parts) {
        // Source fixed-array cells stay live; each foreach value is retained.
        for (auto texture : d_runtime::ngRequireObject(part)->textures) {
            if (texture) addTextureToSlot(texture);
        }
    }
}

std::shared_ptr<Texture> Puppet::findTextureByRuntimeUUID(uint32_t uuid) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    (void)uuid;
    auto slots = textureSlots;
    for (std::size_t index = 0; index < slots.size(); ++index) {
        auto& slot = slots[index];
        // Source tests truthiness and returns the ref cell AFTER the virtual call.
        if (d_runtime::ngRequireObject(slot)->getRuntimeUUID()) return slot;
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
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto target = texture;
    if (d_runtime::ngRequireObject(meta)->thumbnailId == std::numeric_limits<uint32_t>::max()) {
        const auto slot = addTextureToSlot(target);
        d_runtime::ngRequireObject(meta)->thumbnailId = slot;
    } else {
        textureSlots.at(d_runtime::ngRequireObject(meta)->thumbnailId) = target;
    }
}

std::ptrdiff_t Puppet::getTextureSlotIndexFor(const std::shared_ptr<Texture>& texture) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto target = texture;
    const auto slots = textureSlots;
    for (std::size_t index = 0; index < slots.size(); ++index) {
        const auto candidate = slots[index];
        if (ngTextureObjectEquals(candidate, target)) return static_cast<std::ptrdiff_t>(index);
    }
    return -1;
}

void Puppet::clearThumbnail(bool deleteTexture) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (deleteTexture)
        textureSlots = textureSlots.removed(d_runtime::ngRequireObject(meta)->thumbnailId);
    d_runtime::ngRequireObject(meta)->thumbnailId = std::numeric_limits<uint32_t>::max();
}

void Puppet::setRootNode(const std::shared_ptr<Node>& node) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto target = node;
    if (root) d_runtime::ngRequireObject(root)->reparent(nullptr, 0);
    d_runtime::ngRequireObject(target)->reparent(puppetRootNode, 0);
    root = target;
}

void Puppet::recordNodeChange(nodes::NotifyReason reason) {
    NJCX_DBG_LOG("[nicxlive] recordNodeChange reason=%d\n", static_cast<int>(reason));
    pendingFrameChanges.mark(reason);
}

bool Puppet::getIsNodeBound(const std::shared_ptr<nodes::Node>& node) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto target = node;
    const auto capturedParameters = parameters;
    for (auto parameter : capturedParameters)
        if (d_runtime::ngRequireObject(parameter)->hasAnyBinding(target)) return true;
    return false;
}

void Puppet::removeParameter(const std::shared_ptr<Parameter>& parameter) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    // Retain the D value argument before equality can replace its caller slot.
    const auto target = parameter;
    const auto capturedParameters = parameters;
    std::ptrdiff_t index = -1;
    for (std::size_t i = 0; i < capturedParameters.size(); ++i) {
        if (!ngResourceObjectEquals(capturedParameters[i], target)) continue;
        index = static_cast<std::ptrdiff_t>(i);
        break;
    }
    // The source removes from the current member after countUntil completes.
    if (index >= 0) parameters = parameters.removed(static_cast<std::size_t>(index));
}

void Puppet::reconstruct() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    d_runtime::ngRequireObject(root)->reconstruct();
    const auto parameterSnapshot = parameters.duplicate();
    for (auto parameter : parameterSnapshot) d_runtime::ngRequireObject(parameter)->reconstruct(shared_from_this());
    const auto automationSnapshot = automation.duplicate();
    for (auto item : automationSnapshot) d_runtime::ngRequireObject(item)->reconstruct(shared_from_this());
    auto animationSnapshot = animations.duplicate();
    for (auto& [name, item] : animationSnapshot) item.reconstruct(shared_from_this());
}

void Puppet::finalize() {
    const auto self = shared_from_this();
    d_runtime::ngRequireObject(root)->setPuppet(self);
    d_runtime::ngRequireObject(root)->setName(common::NullableString::fromStaticLiteral("Root"));
    puppetRootNode = nodes::Node::createForPuppet(self);
    d_runtime::ngRequireObject(root)->finalize();
    {
        const auto capturedParameters = parameters;
        for (auto parameter : capturedParameters) d_runtime::ngRequireObject(parameter)->finalize(self);
    }
    {
        const auto capturedAutomation = automation;
        for (auto item : capturedAutomation) d_runtime::ngRequireObject(item)->finalize(self);
    }
    for (auto& [name, item] : animations) item.finalize(self);
    scanParts(true, root);
    selfSort();
}

void Puppet::finalizeDeserialization(const ::nicxlive::core::serde::Fghj&) {
    reconstruct();
    finalize();
}

::nicxlive::core::serde::SerdeException Puppet::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    try {
        std::size_t topKeys = 0;
        bool hasNodesKey = false;
        for (const auto& kv : data) {
            ++topKeys;
            if (kv.first == "nodes") hasNodesKey = true;
        }
        NJCX_DBG_LOG("[nicxlive] puppet.deserialize topKeys=%zu hasNodes=%d\n", topKeys, hasNodesKey ? 1 : 0);
        if (auto metaNode = data.get_child_optional("meta")) {
            if (auto preserve = metaNode->get_optional<bool>("preservePixels")) meta->preservePixels = *preserve;
            if (auto thumb = metaNode->get_optional<uint32_t>("thumbnailId")) meta->thumbnailId = *thumb;
        }
        if (auto physicsNode = data.get_child_optional("physics")) {
            if (auto ppm = physicsNode->get_optional<float>("pixelsPerMeter")) physics->pixelsPerMeter = *ppm;
            if (auto gravity = physicsNode->get_optional<float>("gravity")) physics->gravity = *gravity;
        }
        std::shared_ptr<nodes::Node> loadedRoot;
        if (auto rootNode = data.get_child_optional("nodes")) {
            NJCX_DBG_LOG("[nicxlive] puppet.deserialize nodes childCount=%zu\n", rootNode->size());
            std::string type = rootNode->get<std::string>("type", "Node");
            loadedRoot = nodes::Node::inInstantiateNode(type);
            if (!loadedRoot) loadedRoot = std::make_shared<nodes::Node>();
            if (auto rootErr = loadedRoot->deserializeFromFghj(*rootNode)) {
                return rootErr;
            }
            NJCX_DBG_LOG("[nicxlive] puppet.deserialize loadedRoot children=%zu\n", loadedRoot->childrenList().size());
        }
        if (auto paramNode = data.get_child_optional("param")) {
            for (const auto& child : *paramNode) {
                auto p = param::inParameterCreate(child.second);
                parameters.push_back(p);
            }
            NJCX_DBG_LOG("[nicxlive] puppet.deserialize params=%zu\n", parameters.size());
        }

        if (loadedRoot) root = loadedRoot;
        finalizeDeserialization(data);
        if (root) {
            std::size_t totalNodes = 0;
            std::size_t partCount = 0;
            std::size_t meshGroupCount = 0;
            std::size_t pathDeformerCount = 0;
            std::size_t gridDeformerCount = 0;
            std::size_t compositeCount = 0;
            std::function<void(const std::shared_ptr<nodes::Node>&)> countNodes = [&](const std::shared_ptr<nodes::Node>& n) {
                if (!n) return;
                ++totalNodes;
                if (std::dynamic_pointer_cast<nodes::Part>(n)) ++partCount;
                if (std::dynamic_pointer_cast<nodes::MeshGroup>(n)) ++meshGroupCount;
                if (std::dynamic_pointer_cast<nodes::PathDeformer>(n)) ++pathDeformerCount;
                if (std::dynamic_pointer_cast<nodes::GridDeformer>(n)) ++gridDeformerCount;
                if (std::dynamic_pointer_cast<nodes::Composite>(n)) ++compositeCount;
                for (const auto& c : n->childrenRef()) countNodes(c);
            };
            countNodes(root);
            NJCX_DBG_LOG("[nicxlive] load types total=%zu part=%zu meshgroup=%zu pathDeformer=%zu gridDeformer=%zu composite=%zu\n",
                         totalNodes, partCount, meshGroupCount, pathDeformerCount, gridDeformerCount, compositeCount);
        }
    } catch (const std::exception& ex) {
        return std::string(ex.what());
    }
    return std::nullopt;
}

std::shared_ptr<nodes::Node> resolvePuppetNodeById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid) {
    if (!puppet || uuid == 0) return nullptr;
    return puppet->findNodeById(uuid);
}

std::shared_ptr<param::Parameter> resolvePuppetParameterById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid) {
    if (!puppet || uuid == 0) return nullptr;
    return puppet->findParameter(uuid);
}

std::shared_ptr<Resource> resolvePuppetResourceById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid) {
    const auto owner = d_runtime::ngRequireObject(puppet);
    const auto node = owner->findNode(owner->root, uuid);
    if (node) return node;
    return owner->findParameter(uuid);
}

} // namespace nicxlive::core

namespace nicxlive::core {
void Puppet::serializeSelf(serde::TypedJsonWriter& serializer) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    serializer.putKey("meta");
    serializer.serializeValue(meta);
    serializer.putKey("physics");
    serializer.serializeValue(physics);
    serializer.putKey("nodes");
    serializer.serializeValue(root);
    serializer.putKey("param");
    serializer.serializeValue(parameters);
    serializer.putKey("automation");
    serializer.serializeValue(automation);
    serializer.putKey("animations");
    if (animations.isNull()) serializer.putValue(nullptr);
    else {
        const auto object = serializer.structBegin();
        for (auto& [name, animation] : animations) serializer.putField(name, animation);
        serializer.structEnd(object);
    }
}
void Puppet::serialize(serde::TypedJsonWriter& serializer) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto object = serializer.structBegin();
    serializeSelf(serializer);
    serializer.structEnd(object);
}
void Puppet::serializeSelf(serde::TypedJsonWriter& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    serializer.putField("meta", meta);
    serializer.putField("physics", physics);
    serializer.putField("nodes", root);
    serializer.putField("param", parameters);
    serializer.putField("automation", automation);
    serializer.putKey("animations");
    if (animations.isNull()) serializer.putValue(nullptr);
    else {
        const auto object = serializer.structBegin();
        for (const auto& [name, animation] : animations) serializer.putField(name, animation);
        serializer.structEnd(object);
    }
}
void Puppet::serialize(serde::TypedJsonWriter& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto object = serializer.structBegin();
    serializeSelf(serializer);
    serializer.structEnd(object);
}
}

#include "puppet_metadata_reader.hpp"

namespace nicxlive::core {
serde::ReadError Puppet::deserializeFromFghj(serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (auto error = serde::deserializeValue(data.findFirst("meta"), meta)) return error;
    const auto physicsData = data.findFirst("physics");
    if (!physicsData.isMissing())
        if (auto error = serde::deserializeValue(physicsData, physics)) return error;
    const auto rootData = data.findFirst("nodes");
    if (auto error = serde::metadata_detail::prepare(rootData, root, "Node")) return error;
    if (root) if (auto error = root->deserializeFromFghj(rootData)) return error;

    const auto parameterData = data.findFirst("param");
    for (const auto value : parameterData.byElement()) parameters.push_back(param::inParameterCreate(value));
    const auto automationData = data.findFirst("automation");
    for (const auto value : automationData.byElement()) {
        d_runtime::RetainedString type;
        if (auto error = serde::deserializeValue(value.findFirst("type"), type)) return error;
        if (inHasAutomationTypeRetained(type)) {
            auto item = inInstantiateAutomationRetained(type, shared_from_this());
            // The independent original intentionally discards this returned error.
            (void)d_runtime::ngRequireObject(item)->deserializeFromFghj(value);
            automation.push_back(item);
        }
    }
    const auto animationData = data.findFirst("animations");
    if (!animationData.isMissing()) {
        // Preserve the authored ignored return while using the complete real AA reader.
        (void)serde::deserializeValue(animationData, animations);
    }
    finalizeDeserialization(data);
    return {};
}
void Puppet::finalizeDeserialization(serde::TypedJsonView) {
    reconstruct();
    finalize();
}
}
