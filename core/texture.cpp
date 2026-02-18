#include "third_party/stb_image_full.h"

#include <cstring>
#include "texture.hpp"
#include "render/common.hpp"
#include "render/backend_queue.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace nicxlive::core {

namespace {
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
    auto img = loadImage(file, w, h, comp, channels);
    data = img;
    width = w;
    height = h;
    channels = channels == 0 ? comp : channels;
    convChannels = channels;
}

ShallowTexture::ShallowTexture(const std::vector<uint8_t>& buffer, int channels) {
    int w = 0, h = 0, comp = 0;
    auto img = loadImageBuffer(buffer, w, h, comp, channels);
    data = img;
    width = w;
    height = h;
    this->channels = channels == 0 ? comp : channels;
    convChannels = this->channels;
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

void Texture::initFromData(const std::vector<uint8_t>& data, int width, int height, int inChannels, int outChannels, bool stencil, bool useMipmaps) {
    width_ = width;
    height_ = height;
    channels_ = outChannels;
    stencil_ = stencil;
    useMipmaps_ = useMipmaps;
    data_ = data;
    filtering_ = Filtering::Linear;
    wrapping_ = Wrapping::Clamp;
    anisotropy_ = 1.0f;
    if (runtimeUUID_ == 0) runtimeUUID_ = nextUUID();
    if (auto backend = std::dynamic_pointer_cast<render::QueueRenderBackend>(getCurrentRenderBackend())) {
        backendId_ = backend->createTexture(data_, width_, height_, outChannels, stencil_);
    }
}

void Texture::setFiltering(bool point) {
    setFiltering(point ? Filtering::Nearest : Filtering::Linear);
}

void Texture::setFiltering(Filtering mode) {
    filtering_ = mode;
    if (auto backend = std::dynamic_pointer_cast<render::QueueRenderBackend>(getCurrentRenderBackend())) {
        if (backendId_ != 0) backend->setTextureParams(backendId_, filtering_, wrapping_, anisotropy_);
    }
}

void Texture::setWrapping(Wrapping mode) {
    wrapping_ = mode;
    if (auto backend = std::dynamic_pointer_cast<render::QueueRenderBackend>(getCurrentRenderBackend())) {
        if (backendId_ != 0) backend->setTextureParams(backendId_, filtering_, wrapping_, anisotropy_);
    }
}

void Texture::setAnisotropy(float val) {
    anisotropy_ = val;
    if (auto backend = std::dynamic_pointer_cast<render::QueueRenderBackend>(getCurrentRenderBackend())) {
        if (backendId_ != 0) backend->setTextureParams(backendId_, filtering_, wrapping_, anisotropy_);
    }
}

void Texture::setData(const std::vector<uint8_t>& data, int inChannels) {
    channels_ = inChannels;
    if (locked_) {
        lockedData_ = data;
        modified_ = true;
        return;
    }
    data_ = data;
    modified_ = true;
    if (auto backend = std::dynamic_pointer_cast<render::QueueRenderBackend>(getCurrentRenderBackend())) {
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
    if (auto backend = std::dynamic_pointer_cast<render::QueueRenderBackend>(getCurrentRenderBackend())) {
        if (backendId_ != 0) backend->disposeTexture(backendId_);
    }
    backendId_ = 0;
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
        if (auto backend = std::dynamic_pointer_cast<render::QueueRenderBackend>(getCurrentRenderBackend())) {
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
