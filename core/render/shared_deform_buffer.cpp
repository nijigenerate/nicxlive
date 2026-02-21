#include "shared_deform_buffer.hpp"

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
            std::fprintf(stderr, "[nicxlive][shared-atlas] register target=%p len=%zu bindings=%zu stride=%zu\n",
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
        rebuild();
        if (traceSharedEnabled()) {
            std::fprintf(stderr, "[nicxlive][shared-atlas] unregister target=%p bindings=%zu stride=%zu\n",
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
            std::fprintf(stderr, "[nicxlive][shared-atlas] resize target=%p newLen=%zu bindings=%zu stride=%zu\n",
                         static_cast<void*>(ptr), newLength, bindings.size(), storage.size());
        }
    }

    std::size_t stride() const { return storage.size(); }
    Vec2Array& data() {
        syncInPlace();
        return storage;
    }
    bool isDirty() const { return dirty; }
    void markDirty() { dirty = true; }
    void markUploaded() { dirty = false; }

private:
    void syncInPlace() {
        for (const auto& binding : bindings) {
            auto len = binding.length;
            if (len == 0) continue;
            auto copyLen = std::min(len, binding.target->size());
            for (std::size_t i = 0; i < copyLen; ++i) {
                storage.x[binding.offset + i] = binding.target->x[i];
                storage.y[binding.offset + i] = binding.target->y[i];
            }
            if (copyLen < len) {
                std::fill(storage.x.begin() + static_cast<std::ptrdiff_t>(binding.offset + copyLen),
                          storage.x.begin() + static_cast<std::ptrdiff_t>(binding.offset + len), 0.0f);
                std::fill(storage.y.begin() + static_cast<std::ptrdiff_t>(binding.offset + copyLen),
                          storage.y.begin() + static_cast<std::ptrdiff_t>(binding.offset + len), 0.0f);
            }
        }
    }

    void rebuild() {
        std::size_t total = 0;
        for (const auto& binding : bindings) total += binding.length;
        Vec2Array newStorage(total);
        std::size_t offset = 0;
        for (auto& binding : bindings) {
            auto len = binding.length;
            if (len) {
                auto copyLen = std::min(len, binding.target->size());
                for (std::size_t i = 0; i < copyLen; ++i) {
                    newStorage.x[offset + i] = binding.target->x[i];
                    newStorage.y[offset + i] = binding.target->y[i];
                }
                if (copyLen < len) {
                    std::fill(newStorage.x.begin() + static_cast<std::ptrdiff_t>(offset + copyLen),
                              newStorage.x.begin() + static_cast<std::ptrdiff_t>(offset + len), 0.0f);
                    std::fill(newStorage.y.begin() + static_cast<std::ptrdiff_t>(offset + copyLen),
                              newStorage.y.begin() + static_cast<std::ptrdiff_t>(offset + len), 0.0f);
                }
            } else {
                binding.target->clear();
            }
            binding.offset = offset;
            offset += len;
        }
        storage = std::move(newStorage);
        for (auto& binding : bindings) {
            if (binding.offsetSink) *binding.offsetSink = binding.offset;
        }
        dirty = true;
        if (traceSharedEnabled()) {
            std::size_t maxEnd = 0;
            for (const auto& binding : bindings) {
                const auto end = binding.offset + binding.length;
                if (end > maxEnd) maxEnd = end;
            }
            std::fprintf(stderr, "[nicxlive][shared-atlas] rebuild bindings=%zu stride=%zu maxEnd=%zu dirty=%d\n",
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

void sharedVertexRegister(Vec2Array& target, std::size_t* offsetSink) { vertexAtlas.registerArray(target, offsetSink); }
void sharedVertexUnregister(Vec2Array& target) { vertexAtlas.unregisterArray(target); }
void sharedVertexResize(Vec2Array& target, std::size_t newLength) { vertexAtlas.resizeArray(target, newLength); }
std::size_t sharedVertexAtlasStride() { return vertexAtlas.stride(); }
Vec2Array& sharedVertexBufferData() { return vertexAtlas.data(); }
bool sharedVertexBufferDirty() { return vertexAtlas.isDirty(); }
void sharedVertexMarkDirty() { vertexAtlas.markDirty(); }
void sharedVertexMarkUploaded() { vertexAtlas.markUploaded(); }

void sharedUvRegister(Vec2Array& target, std::size_t* offsetSink) { uvAtlas.registerArray(target, offsetSink); }
void sharedUvUnregister(Vec2Array& target) { uvAtlas.unregisterArray(target); }
void sharedUvResize(Vec2Array& target, std::size_t newLength) { uvAtlas.resizeArray(target, newLength); }
std::size_t sharedUvAtlasStride() { return uvAtlas.stride(); }
Vec2Array& sharedUvBufferData() { return uvAtlas.data(); }
bool sharedUvBufferDirty() { return uvAtlas.isDirty(); }
void sharedUvMarkDirty() { uvAtlas.markDirty(); }
void sharedUvMarkUploaded() { uvAtlas.markUploaded(); }

} // namespace nicxlive::core::render
