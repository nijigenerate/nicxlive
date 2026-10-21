#include <atomic>
#include <stdexcept>
#include "third_party/stb_image_full.h"

#include <cstring>
#include "texture.hpp"
#include "render/common.hpp"
#include "render/backend_queue.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace nicxlive::core {

namespace {
struct PendingTextureDisposal {
    std::weak_ptr<RenderBackend> backend;
    uint32_t backendId{0};
};

std::mutex gPendingTextureDisposalsMutex;
std::atomic<bool> gPendingTextureDisposalsAlive{true};
struct PendingTextureDisposalQueue : std::vector<PendingTextureDisposal> {
    ~PendingTextureDisposalQueue() { gPendingTextureDisposalsAlive.store(false); }
};
PendingTextureDisposalQueue gPendingTextureDisposals;

std::vector<uint8_t> loadImage(const std::string& file, int& w, int& h, int& comp, int reqComp) {
    int width = 0, height = 0, components = 0;
    stbi_uc* data = stbi_load(file.c_str(), &width, &height, &components, reqComp);
    if (!data) return {};
    int outComp = (reqComp == 0) ? components : reqComp;
    std::size_t bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(outComp);
    std::vector<uint8_t> result(bytes);
    std::memcpy(result.data(), data, bytes);
    stbi_image_free(data);
    w = width;
    h = height;
    comp = outComp;
    return result;
}
std::vector<uint8_t> loadImageBuffer(const std::vector<uint8_t>& buffer, int& w, int& h, int& comp, int reqComp) {
    int width = 0, height = 0, components = 0;
    stbi_uc* data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, &components, reqComp);
    if (!data) return {};
    int outComp = (reqComp == 0) ? components : reqComp;
    std::size_t bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(outComp);
    std::vector<uint8_t> result(bytes);
    std::memcpy(result.data(), data, bytes);
    stbi_image_free(data);
    w = width;
    h = height;
    comp = outComp;
    return result;
}
} // namespace

ShallowTexture::ShallowTexture(const std::string& file, int channels) {
    int w = 0, h = 0, comp = 0;
    auto img = loadImage(file, w, h, comp, 0);
    if (img.empty()) throw std::runtime_error("Cannot decode image: " + file);
    data = img;
    width = w;
    height = h;
    this->channels = comp;
    convChannels = channels == 0 ? comp : channels;
}

ShallowTexture::ShallowTexture(const std::vector<uint8_t>& buffer, int channels) {
    int w = 0, h = 0, comp = 0;
    auto img = loadImageBuffer(buffer, w, h, comp, 0);
    if (img.empty()) throw std::runtime_error("Cannot decode image buffer");
    data = img;
    width = w;
    height = h;
    this->channels = comp;
    convChannels = channels == 0 ? comp : channels;
}

ShallowTexture::ShallowTexture(const std::vector<uint8_t>& buffer, int w, int h, int channels)
    : ShallowTexture(buffer, w, h, channels, channels) {}

ShallowTexture::ShallowTexture(const std::vector<uint8_t>& buffer, int w, int h, int channels, int convChannels) {
    data = buffer;
    width = w;
    height = h;
    this->channels = channels;
    this->convChannels = convChannels;
}

Texture::Texture(const std::string& file, int channels, bool useMipmaps) {
    int w = 0, h = 0, comp = 0;
    auto img = loadImage(file, w, h, comp, channels);
    if (!img.empty()) {
        initFromData(img, w, h, comp, channels == 0 ? comp : channels, false, useMipmaps);
    }
}

Texture::Texture(const ShallowTexture& shallow, bool useMipmaps) {
    if (!shallow.data.empty() && shallow.width > 0 && shallow.height > 0) {
        initFromData(shallow.data, shallow.width, shallow.height, shallow.channels, shallow.convChannels, false, useMipmaps);
    } else {
        int w = shallow.width;
        int h = shallow.height;
        int comp = shallow.channels;
        auto img = loadImageBuffer(shallow.data, w, h, comp, shallow.convChannels);
        if (!img.empty()) {
            initFromData(img, w, h, comp, shallow.convChannels == 0 ? comp : shallow.convChannels, false, useMipmaps);
        } else {
            initFromData(std::vector<uint8_t>{}, shallow.width, shallow.height, shallow.channels, shallow.convChannels, false, useMipmaps);
        }
    }
}

Texture::Texture(int w, int h, int channels, bool stencil, bool useMipmaps) {
    std::vector<uint8_t> empty;
    if (!stencil) empty.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * static_cast<std::size_t>(channels));
    initFromData(empty, w, h, channels, channels, stencil, useMipmaps);
}

Texture::Texture(const std::vector<uint8_t>& data, int width, int height, int inChannels, int outChannels, bool stencil, bool useMipmaps) {
    initFromData(data, width, height, inChannels, outChannels, stencil, useMipmaps);
}

Texture::~Texture() {
    disposeDeferred();
}

namespace {
std::vector<uint8_t> convertTextureChannels(const std::vector<uint8_t>& data, int w, int h, int from, int to) {
    if (from < 1 || from > 4 || to < 1 || to > 4 || w < 0 || h < 0) throw std::invalid_argument("Invalid texture dimensions/channels");
    if (data.empty()) return {};
    const size_t count = static_cast<size_t>(w) * h;
    if (data.size() < count * from) throw std::invalid_argument("Incomplete texture data");
    std::vector<uint8_t> result(count * to);
    for (size_t i=0;i<count;++i) {
        const auto* src = data.data() + i * from;
        auto* dst = result.data() + i * to;
        const uint8_t gray = from <= 2 ? src[0]
            : static_cast<uint8_t>((77 * src[0] + 150 * src[1] + 29 * src[2]) >> 8);
        const uint8_t alpha = from == 2 ? src[1] : (from == 4 ? src[3] : 255);
        if (to <= 2) { dst[0] = gray; if (to == 2) dst[1] = alpha; }
        else {
            for (int c=0;c<3;++c) dst[c] = from <= 2 ? gray : src[c];
            if (to == 4) dst[3] = alpha;
        }
    }
    return result;
}
}
void Texture::initFromData(const std::vector<uint8_t>& data, int width, int height, int inChannels, int outChannels, bool stencil, bool useMipmaps) {
    width_ = width;
    height_ = height;
    channels_ = outChannels;
    stencil_ = stencil;
    useMipmaps_ = useMipmaps;
    data_ = convertTextureChannels(data, width, height, inChannels, outChannels);
    filtering_ = Filtering::Linear;
    wrapping_ = Wrapping::Clamp;
    anisotropy_ = 1.0f;
    if (runtimeUUID_ == 0) runtimeUUID_ = nextUUID();
    if (auto backend = getCurrentRenderBackend()) {
        backend_ = backend;
        backendId_ = backend->createTexture(data_, width_, height_, outChannels, stencil_);
    }
}

void Texture::setFiltering(bool point) {
    setFiltering(point ? Filtering::Nearest : Filtering::Linear);
}

void Texture::setFiltering(Filtering mode) {
    filtering_ = mode;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (backend) {
        backend_ = backend;
        if (backendId_ != 0) backend->setTextureParams(backendId_, filtering_, wrapping_, anisotropy_);
    }
}

void Texture::setWrapping(Wrapping mode) {
    wrapping_ = mode;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (backend) {
        backend_ = backend;
        if (backendId_ != 0) backend->setTextureParams(backendId_, filtering_, wrapping_, anisotropy_);
    }
}

void Texture::setAnisotropy(float val) {
    anisotropy_ = val;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (backend) {
        backend_ = backend;
        if (backendId_ != 0) backend->setTextureParams(backendId_, filtering_, wrapping_, anisotropy_);
    }
}

void Texture::setData(const std::vector<uint8_t>& data, int inChannels) {
    const auto converted = convertTextureChannels(data, width_, height_, inChannels == -1 ? channels_ : inChannels, channels_);
    if (locked_) {
        lockedData_ = converted;
        modified_ = true;
        return;
    }
    data_ = converted;
    modified_ = true;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (backend) {
        backend_ = backend;
        if (backendId_ == 0) {
            backendId_ = backend->createTexture(data_, width_, height_, channels_, stencil_);
        } else {
            backend->updateTexture(backendId_, data_, width_, height_, channels_);
        }
    }
}

void Texture::dispose() {
    data_.clear();
    width_ = height_ = 0;
    runtimeUUID_ = 0;
    if (auto backend = backend_.lock()) {
        if (backendId_ != 0) backend->disposeTexture(backendId_);
    }
    backendId_ = 0;
    backend_.reset();
}

void Texture::disposeDeferred() {
    if (!gPendingTextureDisposalsAlive.load()) {
        backendId_ = 0;
        backend_.reset();
        return;
    }
    if (backendId_ == 0) return;

    auto backend = backend_.lock();
    if (!backend) {
        backendId_ = 0;
        backend_.reset();
        return;
    }

    std::lock_guard<std::mutex> lock(gPendingTextureDisposalsMutex);
    gPendingTextureDisposals.push_back(PendingTextureDisposal{std::move(backend), backendId_});
    backendId_ = 0;
    backend_.reset();
}

void inDrainPendingTextureDisposals() {
    std::vector<PendingTextureDisposal> pending;
    {
        std::lock_guard<std::mutex> lock(gPendingTextureDisposalsMutex);
        pending.swap(gPendingTextureDisposals);
    }

    const auto current = getCurrentRenderBackend();
    std::vector<PendingTextureDisposal> deferred;
    for (const auto& item : pending) {
        if (auto backend = item.backend.lock(); backend && item.backendId != 0) {
            if (backend == current) backend->disposeTexture(item.backendId);
            else deferred.push_back(item);
        }
    }
    if (!deferred.empty()) {
        std::lock_guard<std::mutex> lock(gPendingTextureDisposalsMutex);
        gPendingTextureDisposals.insert(gPendingTextureDisposals.end(), deferred.begin(), deferred.end());
    }
}

void Texture::lock() {
    if (locked_) return;
    lockedData_ = data_;
    modified_ = false;
    locked_ = true;
}

void Texture::unlock() {
    if (!locked_) return;
    locked_ = false;
    if (modified_) {
        data_ = lockedData_;
        auto backend = backend_.lock();
        if (!backend) backend = getCurrentRenderBackend();
        if (backend) {
            backend_ = backend;
            if (backendId_ == 0) {
                backendId_ = backend->createTexture(data_, width_, height_, channels_, stencil_);
            } else {
                backend->updateTexture(backendId_, data_, width_, height_, channels_);
            }
        }
    }
    modified_ = false;
    lockedData_.clear();
}

uint32_t Texture::nextUUID() {
    static uint32_t counter = 1;
    return counter++;
}

} // namespace nicxlive::core
