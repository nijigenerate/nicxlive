#pragma once

#include "common/shared_array.hpp"
#include "common/resource_key_map.hpp"
#include "common/object_key_policy.hpp"
#include "compat/native_access_fault.hpp"
#include "compat/switch_error.hpp"
#include "nodes/node.hpp"
#include "nodes/composite.hpp"
#include "nodes/dynamic_composite.hpp"
#include "nodes/part.hpp"
#include "nodes/driver.hpp"
#include "nodes/filter.hpp"
#include "automation/package.hpp"
#include "render.hpp"
#include "param/parameter.hpp"
#include "render/graph_builder.hpp"
#include "render/scheduler.hpp"
#include "render/command_emitter.hpp"
#include "texture.hpp"
#include "puppet_metadata.hpp"
#include "puppet_ext.hpp"
#include "animation/animation.hpp"
#include "animation/animation_map.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace nicxlive::core {

// Forward declarations for automation/animation/physics metadata.
using ::nicxlive::core::param::Parameter;


class Puppet : public std::enable_shared_from_this<Puppet> {
public:
    virtual ~Puppet() = default;
    virtual d_runtime::RetainedString toString() const;
    Puppet();
    explicit Puppet(const std::shared_ptr<nodes::Node>& rootNode);
    static std::shared_ptr<Puppet> create();
    static std::shared_ptr<Puppet> create(std::shared_ptr<nodes::Node> rootNode);

    // lifecycle / tasks
    void scanParts(bool reparent = true, const std::shared_ptr<nodes::Node>& node = nullptr);
    struct FrameChangeState {
        bool attributeDirty{false};
        bool structureDirty{false};

        bool any() const { return attributeDirty || structureDirty; }
        void mark(nodes::NotifyReason reason) {
            switch (reason) {
            case nodes::NotifyReason::StructureChanged:
                structureDirty = true;
                attributeDirty = true;
                break;
            case nodes::NotifyReason::AttributeChanged:
            case nodes::NotifyReason::Initialized:
            case nodes::NotifyReason::Transformed:
                attributeDirty = true;
                break;
            default:
                throw d_runtime::ngStaticError<d_runtime::SwitchError>(
                    d_runtime::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
                    d_runtime::RetainedString::fromStaticLiteral("nijilive/core/puppet.d"), 261);
            }
        }
    };
    FrameChangeState consumeFrameChanges();
    void rebuildRenderTasks(const std::shared_ptr<nodes::Node>& rootNode);
    void updateParametersAndDrivers(const std::shared_ptr<nodes::Node>& rootNode);
    void update();
    void resetDrivers();

    // Original D mutable collection queries, used by SceneItem attachment.
    auto& getRootParts() { return rootParts; }
    auto& getDrivers() { return drivers; }
    auto& getParameterDrivers() { return drivenParameters; }
    auto& getAnimations() { return animations; }
    // queries
    std::shared_ptr<nodes::Node> findNode(const std::shared_ptr<nodes::Node>& n, const std::string& name);
    std::shared_ptr<nodes::Node> findNode(std::shared_ptr<nodes::Node> n, uint32_t uuid);
    std::ptrdiff_t findParameterIndex(const std::string& name);
    virtual std::shared_ptr<Parameter> findParameter(uint32_t uuid);
    std::shared_ptr<nodes::Node> findNodeByName(const std::string& name);
    std::shared_ptr<nodes::Node> findNodeById(uint32_t uuid);
    std::shared_ptr<Parameter> findParameterByName(const std::string& name);
    std::shared_ptr<Parameter> findParameterById(uint32_t uuid);
    template<class T>
        requires (std::is_base_of_v<nodes::Node, T> || std::is_base_of_v<nodes::NodeFilter, T>)
    common::SharedArray<std::shared_ptr<T>> findNodesType(std::shared_ptr<nodes::Node> node) {
        common::SharedArray<std::shared_ptr<T>> result;
        if (auto item = std::dynamic_pointer_cast<T>(node)) result.push_back(item);
        const auto children = d_runtime::ngRequireObject(node)->childrenList();
        for (auto child : children) {
            const auto descendants = findNodesType<T>(child);
            result.appendRange(descendants);
        }
        return result;
    }
    void recordNodeChange(nodes::NotifyReason reason);
    // Internal snapshot access, matching the independent runtime's package scope.
    std::shared_ptr<nodes::Node> getPuppetRootNode() { return puppetRootNode; }
    void requestFullRenderRebuild() {
        forceFullRebuild = true;
        schedulerCacheValid = false;
    }

    // rendering
    void draw();
    void rescanNodes();
    void updateTextureState();
    RenderCommandEmitter* commandEmitter();
    void setRenderBackend(const std::shared_ptr<::nicxlive::core::RenderBackend>& backend);
    bool isRenderGraphEmpty() const;
    std::size_t rootPartCount() const;
    virtual bool getIsNodeBound(const std::shared_ptr<nodes::Node>& node);
    virtual void removeParameter(const std::shared_ptr<Parameter>& parameter);
    virtual void serializeSelf(serde::TypedJsonWriter& serializer) const;
    virtual void reconstruct();
    virtual void finalize();
    virtual void finalizeDeserialization(const ::nicxlive::core::serde::Fghj& data);
    virtual serde::ReadError deserializeFromFghj(serde::TypedJsonView data);
    virtual void finalizeDeserialization(serde::TypedJsonView data);
    virtual ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data);

    // textures
    uint32_t addTextureToSlot(const std::shared_ptr<::nicxlive::core::Texture>& texture);
    void populateTextureSlots();
    std::shared_ptr<::nicxlive::core::Texture> resolveTextureSlot(uint32_t runtimeUUID);
    std::shared_ptr<::nicxlive::core::Texture> findTextureByRuntimeUUID(uint32_t uuid);
    void setThumbnail(const std::shared_ptr<::nicxlive::core::Texture>& texture);
    std::ptrdiff_t getTextureSlotIndexFor(const std::shared_ptr<::nicxlive::core::Texture>& texture);
    void clearThumbnail(bool deleteTexture = false);

    // Data
    std::shared_ptr<PuppetMeta> meta;
    std::shared_ptr<PuppetPhysics> physics;
    std::shared_ptr<nodes::Node> root{};
    common::SharedArray<std::shared_ptr<Parameter>> parameters{};
    common::SharedArray<std::shared_ptr<Automation>> automation{};
    AnimationMap animations{};
    common::SharedArray<std::shared_ptr<::nicxlive::core::Texture>> textureSlots{};
    bool renderParameters{true};
    bool enableDrivers{true};
    nodes::Transform transform{};
    PuppetExtData extData{};

protected:
    // Derived factories allocate their real shared owner before source base construction.
    struct DeferredConstruction {};
    explicit Puppet(DeferredConstruction) {}
    void constructOwned();
    void constructOwned(std::shared_ptr<nodes::Node> rootNode);

private:
    void initializeRenderState();
    std::shared_ptr<nodes::Node> puppetRootNode{};
    common::SharedArray<std::shared_ptr<nodes::Node>> rootParts{};
    common::SharedArray<std::shared_ptr<nodes::Driver>> drivers{};
    struct DrivenParameterHash {
        std::size_t operator()(const std::shared_ptr<Parameter>& parameter) const noexcept {
            return common::ObjectClassKeyPolicy<std::shared_ptr<Parameter>>::Hash{}(parameter);
        }
    };
    struct DrivenParameterEqual {
        bool operator()(const std::shared_ptr<Parameter>& query,
            const std::shared_ptr<Parameter>& stored) const {
            return common::ObjectClassKeyPolicy<std::shared_ptr<Parameter>>::Equal{}(query, stored);
        }
    };
    common::ResourceKeyMap<std::shared_ptr<Parameter>, std::shared_ptr<nodes::Driver>,
        DrivenParameterHash, DrivenParameterEqual> drivenParameters{};

    std::unique_ptr<RenderCommandEmitter> commandEmitterOwned{};
    ::nicxlive::core::RenderGraphBuilder renderGraph{};
    std::shared_ptr<::nicxlive::core::RenderBackend> renderBackend{};
    std::shared_ptr<::nicxlive::core::TaskScheduler> renderScheduler{};
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
public:
    // Append new virtual slots after all existing slots; no base/data changes.
    const void* objectIdentity() const noexcept { return dynamic_cast<const void*>(this); }
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Puppet> other);
    virtual int opCmp(std::shared_ptr<Puppet> other);
    virtual void applyDeformToChildren();
    virtual common::SharedArray<std::shared_ptr<nodes::Part>> getAllParts();
    virtual std::ptrdiff_t findParameterIndex(d_runtime::RetainedString name);
    virtual void drawImmediateFallback();
    virtual std::shared_ptr<nodes::Node> actualRoot();
    virtual void setRootNode(const std::shared_ptr<nodes::Node>& node);
    virtual void serialize(serde::TypedJsonWriter& serializer) const;
    void selfSort();

    // Original mutable class methods; existing const native overloads remain.
    virtual void serializeSelf(serde::TypedJsonWriter& serializer);
    virtual void serialize(serde::TypedJsonWriter& serializer);

    template<bool Reupdate = false> std::array<float, 4> getCombinedBounds() {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        return d_runtime::ngRequireObject(root)->getCombinedBounds(Reupdate, true);
    }

    template<class T = nodes::Node> requires std::is_convertible_v<T*, nodes::Node*>
    std::shared_ptr<T> find(d_runtime::RetainedString name) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        return std::dynamic_pointer_cast<T>(findNodeByName(name));
    }
    template<class T> requires std::is_convertible_v<T*, nodes::Node*>
    std::shared_ptr<T> find(std::uint32_t uuid) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        return std::dynamic_pointer_cast<T>(findNodeById(uuid));
    }
    template<class T = Parameter> requires std::is_convertible_v<T*, Parameter*>
    std::shared_ptr<T> find(d_runtime::RetainedString name) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        return std::dynamic_pointer_cast<T>(findParameterByName(name));
    }
    template<class T> requires std::is_convertible_v<T*, Parameter*>
    std::shared_ptr<T> find(std::uint32_t uuid) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        return std::dynamic_pointer_cast<T>(findParameterById(uuid));
    }
    template<class T> requires (std::is_convertible_v<T*, Resource*> &&
        !std::is_convertible_v<T*, nodes::Node*> && !std::is_convertible_v<T*, Parameter*>)
    std::shared_ptr<T> find(std::uint32_t uuid) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        auto node = findNodeById(uuid);
        if (node) return node;
        return findParameter(uuid);
    }

    // Source names retain their actual counted bytes across authored callbacks.
    std::shared_ptr<nodes::Node> findNode(std::shared_ptr<nodes::Node> node,
        d_runtime::RetainedString name);
    std::shared_ptr<nodes::Node> findNodeByName(d_runtime::RetainedString name);
    std::shared_ptr<Parameter> findParameterByName(d_runtime::RetainedString name);
    template<class Name> requires std::is_convertible_v<Name, const char*>
    std::shared_ptr<nodes::Node> findNode(const std::shared_ptr<nodes::Node>& node, Name&& name) {
        return findNode(node, d_runtime::RetainedString(name));
    }
    template<class Name> requires std::is_convertible_v<Name, const char*>
    std::ptrdiff_t findParameterIndex(Name&& name) {
        return findParameterIndex(d_runtime::RetainedString(name));
    }
    template<class Name> requires std::is_convertible_v<Name, const char*>
    std::shared_ptr<nodes::Node> findNodeByName(Name&& name) {
        return findNodeByName(d_runtime::RetainedString(name));
    }
    template<class Name> requires std::is_convertible_v<Name, const char*>
    std::shared_ptr<Parameter> findParameterByName(Name&& name) {
        return findParameterByName(d_runtime::RetainedString(name));
    }
};



std::shared_ptr<nodes::Node> resolvePuppetNodeById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid);
std::shared_ptr<Resource> resolvePuppetResourceById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid);
bool ngPuppetObjectEquals(std::shared_ptr<Puppet> left,std::shared_ptr<Puppet> right);

} // namespace nicxlive::core
