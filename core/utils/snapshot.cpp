#include "snapshot.hpp"
#include "../compat/throwable.hpp"
#include <bit>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nicxlive::utils {
using Node = core::nodes::Node;
namespace {
// These calls allocate passive bytes or arrays of handles/delegate descriptors.
// Actual callback invocation and virtual scene operations stay outside.
template<class Allocate> decltype(auto) allocateSnapshotBacking(Allocate&& allocate) {
    try {
        return std::forward<Allocate>(allocate)();
    } catch (const std::bad_alloc&) {
        throw core::d_runtime::ngStaticError<core::d_runtime::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw core::d_runtime::ngStaticError<core::d_runtime::OutOfMemoryError>(std::source_location::current());
    }
}

void prepareSnapshotClassInfo() {
    static const bool registered = [] {
        core::d_runtime::ngRegisterObjectClassName(typeid(Snapshot),
            core::d_runtime::RetainedString::fromStaticLiteral("nijilive.utils.snapshot.Snapshot"));
        return true;
    }();
    (void)registered;
}
}

// Object defaults, object.d (Digital Mars 2000-2011), BSL-1.0.
// Complete notice: licenses/druntime-associative-array.txt.
core::d_runtime::RetainedString Snapshot::toString() {
    const auto lifetime = weak_from_this().lock();
    prepareSnapshotClassInfo();
    return core::d_runtime::ngObjectClassName(typeid(*this));
}

std::size_t Snapshot::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}

bool Snapshot::opEquals(std::shared_ptr<Snapshot> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}

int Snapshot::opCmp(std::shared_ptr<Snapshot>) {
    const auto lifetime = weak_from_this().lock();
    prepareSnapshotClassInfo();
    const auto className = core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = allocateSnapshotBacking([&] {
        std::string text = "need opCmp for class ";
        text += className.view();
        return core::d_runtime::RetainedString(std::move(text));
    });
    throw core::d_runtime::ThrowableException(message,
        core::d_runtime::RetainedString::fromStaticLiteral("object.d"), 195);
}

bool ngSnapshotObjectEquals(std::shared_ptr<Snapshot> left, std::shared_ptr<Snapshot> right) {
    return core::common::ngClassObjectEquals(std::move(left), std::move(right), &prepareSnapshotClassInfo);
}

thread_local Snapshot::HandleMap Snapshot::handles;
thread_local core::common::SharedArray<Snapshot::PendingRequest> Snapshot::pendingQueue;

Snapshot::Snapshot() {
    prepareSnapshotClassInfo();
}
Snapshot::Snapshot(SourceSnapshotConstructionTag) {
    prepareSnapshotClassInfo();
}
Snapshot::Snapshot(std::shared_ptr<Puppet> puppet) {
    prepareSnapshotClassInfo();
    sharedCount = 1;
    setup(std::move(puppet));
}
void Snapshot::setup(std::shared_ptr<Puppet> puppet) {
    const auto lifetime = weak_from_this().lock();
    if (dcomposite && dcomposite->textures[0]) {
        const auto texture = dcomposite->textures[0];
        texture->dispose();
        core::d_runtime::ngRequireObject(dcomposite)->textures[0].reset();
    }
    dcomposite = Composite::create();
    snapshotPuppet = puppet;
    {
        const auto receiver = dcomposite;
        core::d_runtime::ngRequireObject(receiver)->setName(core::common::NullableString::fromStaticLiteral("Snapshot"));
    }
    {
        const auto receiver = dcomposite;
        const auto parent = core::d_runtime::ngRequireObject(puppet)->getPuppetRootNode();
        core::d_runtime::ngRequireObject(receiver)->setParentSource(parent);
    }
    {
        const auto receiver = core::d_runtime::ngRequireObject(puppet)->root;
        core::d_runtime::ngRequireObject(receiver)->setParentSource(dcomposite);
    }
    {
        const auto receiver = dcomposite;
        core::d_runtime::ngRequireObject(receiver)->setPuppet(puppet);
    }
    core::d_runtime::ngRequireObject(puppet)->rescanNodes();
    core::d_runtime::ngRequireObject(puppet)->requestFullRenderRebuild();
    {
        const auto receiver = dcomposite;
        core::d_runtime::ngRequireObject(receiver)->build();
    }
}
std::shared_ptr<Snapshot> Snapshot::get(std::shared_ptr<Puppet> puppet) {
    if (handles.contains(puppet)) {
        {
            const auto receiver = core::d_runtime::ngRequireObject(handles.at(puppet));
            receiver->sharedCount = std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(receiver->sharedCount) + 1u);
        }
        // Source performs a fresh lookup; hash/equality callbacks can change the table.
        return handles.at(puppet);
    }
    auto result = ngCreateSnapshotSource(puppet);
    handles[puppet] = result;
    return result;
}
void Snapshot::release() {
    const auto lifetime = shared_from_this();
    sharedCount = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(sharedCount) - 1u);
    if (sharedCount <= 0 && snapshotPuppet) {
        if (dcomposite) {
            if (core::d_runtime::ngRequireObject(
                    core::d_runtime::ngRequireObject(snapshotPuppet)->root)->parentPtr() == dcomposite) {
                const auto receiver = core::d_runtime::ngRequireObject(snapshotPuppet)->root;
                const auto parent = core::d_runtime::ngRequireObject(snapshotPuppet)->getPuppetRootNode();
                core::d_runtime::ngRequireObject(receiver)->reparent(parent, Node::OFFSET_END, true);
            }
            {
                const auto receiver = dcomposite;
                core::d_runtime::ngRequireObject(receiver)->reparent(nullptr, Node::OFFSET_END, true);
            }
            if (!core::d_runtime::ngRequireObject(dcomposite)->textures.empty()
                && core::d_runtime::ngRequireObject(dcomposite)->textures[0]) {
                const auto texture = dcomposite->textures[0];
                texture->dispose();
                core::d_runtime::ngRequireObject(dcomposite)->textures[0].reset();
            }
        }
        core::common::SharedArray<PendingRequest> filtered;
        const auto requests = pendingQueue;
        for (auto request : requests) {
            if (request.snap.get() != this)
                allocateSnapshotBacking([&] { filtered.push_back(request); });
        }
        pendingQueue = filtered;
        {
            const auto receiver = snapshotPuppet;
            core::d_runtime::ngRequireObject(receiver)->rescanNodes();
        }
        {
            const auto receiver = snapshotPuppet;
            core::d_runtime::ngRequireObject(receiver)->requestFullRenderRebuild();
        }
        handles.erase(snapshotPuppet);
        snapshotPuppet.reset();
        dcomposite.reset();
    }
}
void Snapshot::onCaptured(CaptureCallback callback) {
    const auto lifetime = shared_from_this();
    allocateSnapshotBacking([&] { pendingCallbacks.push_back(callback); });
}
void Snapshot::capture() {
    const auto lifetime = shared_from_this();
    CaptureCallback aggregate;
    if (!pendingCallbacks.empty()) {
        aggregate = [self = lifetime](std::shared_ptr<Texture> texture) {
            const auto callbacks = self->pendingCallbacks;
            for (auto callback : callbacks) {
                if (callback) callback(texture);
            }
            self->pendingCallbacks.resize(0);
        };
    }
    requestCapture(lifetime, [aggregate](std::shared_ptr<Texture> texture) {
        if (aggregate) aggregate(texture);
    });
}
void Snapshot::requestCapture(std::shared_ptr<Snapshot> snapshot, CaptureCallback callback) {
    if (!snapshot) return;
    allocateSnapshotBacking([&] { pendingQueue.push_back(PendingRequest{snapshot, callback}); });
}
void Snapshot::processPending() {
    if (pendingQueue.empty()) return;
    const auto queue = allocateSnapshotBacking([] { return pendingQueue.duplicate(); });
    pendingQueue.resize(0);
    for (auto request : queue) {
        if (!request.snap || !request.snap->dcomposite) continue;
        if (request.snap->dcomposite->textures.empty()) continue;
        const auto texture = request.snap->dcomposite->textures[0];
        if (!texture) continue;
        if (request.cb) request.cb(texture);
    }
}
core::math::Vec2 Snapshot::position() {
    const auto lifetime = shared_from_this();
    if (dcomposite) {
        const auto receiver = dcomposite;
        const auto translation = core::d_runtime::ngRequireObject(receiver)->transform().translation;
        const auto offset = core::d_runtime::ngRequireObject(dcomposite)->textureOffset;
        return {translation.x + offset.x, translation.y + offset.y};
    }
    return {0, 0};
}
} // namespace nicxlive::utils
