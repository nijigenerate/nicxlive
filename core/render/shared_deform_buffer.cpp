#include "shared_deform_buffer.hpp"
#include "common.hpp"
#include "../runtime_state.hpp"
#include <mutex>
#include "../debug_log.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace nicxlive::core::render {
using ::nicxlive::core::nodes::Vec2Array;

namespace {
bool traceSharedEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_SHARED");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

struct SharedVecAtlas {
    struct Binding {
        Vec2Array* target{nullptr};
        std::size_t* offsetSink{nullptr};
        std::size_t length{0};
        std::size_t offset{0};
    };

    Vec2Array storage{};
    std::vector<Binding> bindings{};
    std::unordered_map<Vec2Array*, std::size_t> lookup{};
    bool dirty{false};
    std::size_t revision{0};

    void registerArray(Vec2Array& target, std::size_t* offsetSink) {
        auto ptr = &target;
        auto it = lookup.find(ptr);
        if (it != lookup.end()) {
            bindings[it->second].offsetSink = offsetSink;
            return;
        }
        std::size_t idx = bindings.size();
        lookup[ptr] = idx;
        bindings.push_back(Binding{ptr, offsetSink, target.size(), 0});
        rebuild();
        if (traceSharedEnabled()) {
            NJCX_DBG_LOG("[nicxlive][shared-atlas] register target=%p len=%zu bindings=%zu stride=%zu\n",
                         static_cast<void*>(ptr), target.size(), bindings.size(), storage.size());
        }
    }

    void unregisterArray(Vec2Array& target) {
        auto ptr = &target;
        auto it = lookup.find(ptr);
        if (it == lookup.end()) return;
        std::size_t idx = it->second;
        std::size_t last = bindings.size() - 1;
        lookup.erase(it);
        if (idx != last) {
            bindings[idx] = bindings[last];
            lookup[bindings[idx].target] = idx;
        }
        bindings.pop_back();
        if (bindings.empty()) {
            std::vector<Binding>().swap(bindings);
            std::unordered_map<Vec2Array*, std::size_t>().swap(lookup);
        }
        rebuild();
        if (traceSharedEnabled()) {
            NJCX_DBG_LOG("[nicxlive][shared-atlas] unregister target=%p bindings=%zu stride=%zu\n",
                         static_cast<void*>(ptr), bindings.size(), storage.size());
        }
    }

    void resizeArray(Vec2Array& target, std::size_t newLength) {
        auto ptr = &target;
        auto it = lookup.find(ptr);
        if (it == lookup.end()) return;
        auto idx = it->second;
        if (bindings[idx].length == newLength) return;
        bindings[idx].length = newLength;
        rebuild();
        if (traceSharedEnabled()) {
            NJCX_DBG_LOG("[nicxlive][shared-atlas] resize target=%p newLen=%zu bindings=%zu stride=%zu\n",
                         static_cast<void*>(ptr), newLength, bindings.size(), storage.size());
        }
    }

    std::size_t stride() const { return storage.size(); }
    Vec2Array& data() { return storage; }
    bool isDirty() const { return dirty; }
    void markDirty() {
        bumpRevision();
        dirty = true;
    }
    void markUploaded() { dirty = false; }
    std::size_t currentRevision() const { return revision; }

private:
    void bumpRevision() {
        ++revision;
        if (revision == 0) revision = 1;
    }

    void rebuild() {
        std::size_t total = 0;
        for (const auto& binding : bindings) total += binding.length;
        Vec2Array newStorage;
        if (total) {
            newStorage.setLength(total);
            std::size_t offset = 0;
            for (auto& binding : bindings) {
                auto len = binding.length;
                if (len) {
                    auto copyLen = std::min(len, binding.target->size());
                    auto dstX = newStorage.dataXMutable() + offset;
                    auto dstY = newStorage.dataYMutable() + offset;
                    const float* srcX = binding.target->dataX();
                    const float* srcY = binding.target->dataY();
                    if (copyLen && srcX && srcY) {
                        std::copy_n(srcX, copyLen, dstX);
                        std::copy_n(srcY, copyLen, dstY);
                    }
                    if (copyLen < len) {
                        std::fill(dstX + static_cast<std::ptrdiff_t>(copyLen), dstX + static_cast<std::ptrdiff_t>(len), 0.0f);
                        std::fill(dstY + static_cast<std::ptrdiff_t>(copyLen), dstY + static_cast<std::ptrdiff_t>(len), 0.0f);
                    }
                } else {
                    binding.target->clear();
                }
                binding.offset = offset;
                offset += len;
            }
        } else {
            for (auto& binding : bindings) {
                binding.offset = 0;
                binding.target->clear();
            }
        }
        storage = std::move(newStorage);
        for (auto& binding : bindings) {
            if (binding.length) {
                binding.target->bindExternalStorage(storage, binding.offset, binding.length);
            } else {
                binding.target->clear();
            }
            if (binding.offsetSink) *binding.offsetSink = binding.offset;
        }
        bumpRevision();
        dirty = true;
        if (traceSharedEnabled()) {
            std::size_t maxEnd = 0;
            for (const auto& binding : bindings) {
                const auto end = binding.offset + binding.length;
                if (end > maxEnd) maxEnd = end;
            }
            NJCX_DBG_LOG("[nicxlive][shared-atlas] rebuild bindings=%zu stride=%zu maxEnd=%zu dirty=%d\n",
                         bindings.size(), storage.size(), maxEnd, dirty ? 1 : 0);
        }
    }
};

SharedVecAtlas deformAtlas;
SharedVecAtlas vertexAtlas;
SharedVecAtlas uvAtlas;
} // namespace

void sharedDeformRegister(Vec2Array& target, std::size_t* offsetSink) { deformAtlas.registerArray(target, offsetSink); }
void sharedDeformUnregister(Vec2Array& target) { deformAtlas.unregisterArray(target); }
void sharedDeformResize(Vec2Array& target, std::size_t newLength) { deformAtlas.resizeArray(target, newLength); }
std::size_t sharedDeformAtlasStride() { return deformAtlas.stride(); }
Vec2Array& sharedDeformBufferData() { return deformAtlas.data(); }
bool sharedDeformBufferDirty() { return deformAtlas.isDirty(); }
void sharedDeformMarkDirty() { deformAtlas.markDirty(); }
void sharedDeformMarkUploaded() { deformAtlas.markUploaded(); }
std::size_t sharedDeformBufferRevision() { return deformAtlas.currentRevision(); }

void sharedVertexRegister(Vec2Array& target, std::size_t* offsetSink) { vertexAtlas.registerArray(target, offsetSink); }
void sharedVertexUnregister(Vec2Array& target) { vertexAtlas.unregisterArray(target); }
void sharedVertexResize(Vec2Array& target, std::size_t newLength) { vertexAtlas.resizeArray(target, newLength); }
std::size_t sharedVertexAtlasStride() { return vertexAtlas.stride(); }
Vec2Array& sharedVertexBufferData() { return vertexAtlas.data(); }
bool sharedVertexBufferDirty() { return vertexAtlas.isDirty(); }
void sharedVertexMarkDirty() { vertexAtlas.markDirty(); }
void sharedVertexMarkUploaded() { vertexAtlas.markUploaded(); }
std::size_t sharedVertexBufferRevision() { return vertexAtlas.currentRevision(); }

void sharedUvRegister(Vec2Array& target, std::size_t* offsetSink) { uvAtlas.registerArray(target, offsetSink); }
void sharedUvUnregister(Vec2Array& target) { uvAtlas.unregisterArray(target); }
void sharedUvResize(Vec2Array& target, std::size_t newLength) { uvAtlas.resizeArray(target, newLength); }
std::size_t sharedUvAtlasStride() { return uvAtlas.stride(); }
Vec2Array& sharedUvBufferData() { return uvAtlas.data(); }
bool sharedUvBufferDirty() { return uvAtlas.isDirty(); }
void sharedUvMarkDirty() { uvAtlas.markDirty(); }
void sharedUvMarkUploaded() { uvAtlas.markUploaded(); }
std::size_t sharedUvBufferRevision() { return uvAtlas.currentRevision(); }

namespace {
struct Retirement { std::weak_ptr<RenderBackend> backend; uint32_t handle; };
struct RetirementQueue { std::mutex mutex; std::vector<Retirement> items; };
// Remains valid during late global/TLS Drawable destruction; no backend is owned.
RetirementQueue& retirements() { static auto* queue=new RetirementQueue;return *queue; }
}
void retireDrawableBuffers(std::weak_ptr<RenderBackend> backend,uint32_t handle) {
    if(!handle || backend.expired())return;
    auto& queue=retirements();std::lock_guard lock(queue.mutex);
    queue.items.push_back({std::move(backend),handle});
}
void collectRenderResources() {
    std::vector<Retirement> pending;
    {auto& queue=retirements();std::lock_guard lock(queue.mutex);pending.swap(queue.items);}
    std::vector<std::shared_ptr<RenderBackend>> touched;
    for(auto& entry:pending)if(auto backend=entry.backend.lock()) {
        backend->disposeDrawableBuffers(entry.handle);
        if(std::find(touched.begin(),touched.end(),backend)==touched.end())touched.push_back(backend);
    }
    for(auto& backend:touched)backend->collectDrawableBuffers();
    syncSharedBuffers(getCurrentRenderBackend());
}

void syncSharedBuffers(const std::shared_ptr<RenderBackend>& backend) {
    if (!backend) return;
    struct UploadState {
        std::weak_ptr<RenderBackend> backend;
        std::size_t vertex, uv, deform;
    };
    static std::vector<UploadState> states;
    std::erase_if(states, [](const auto& state) { return state.backend.expired(); });
    auto it = std::find_if(states.begin(), states.end(), [&](const auto& state) { return state.backend.lock() == backend; });
    if (it == states.end()) {
        states.push_back({backend, static_cast<std::size_t>(-1), static_cast<std::size_t>(-1), static_cast<std::size_t>(-1)});
        it = std::prev(states.end());
    }
    // Empty data releases the driver's allocation. Global dirty flags cannot
    // establish whether another backend has received the latest atlas contents.
    if (it->vertex != sharedVertexBufferRevision()) {
        backend->uploadSharedVertexBuffer(sharedVertexBufferData());
        it->vertex = sharedVertexBufferRevision(); sharedVertexMarkUploaded();
    }
    if (it->uv != sharedUvBufferRevision()) {
        backend->uploadSharedUvBuffer(sharedUvBufferData());
        it->uv = sharedUvBufferRevision(); sharedUvMarkUploaded();
    }
    if (it->deform != sharedDeformBufferRevision()) {
        backend->uploadSharedDeformBuffer(sharedDeformBufferData());
        it->deform = sharedDeformBufferRevision(); sharedDeformMarkUploaded();
    }
}

} // namespace nicxlive::core::render
