#include <atomic>
// Shallow/source factory port: Inochi2D Project 2020; nijigenerate Project 2024.
// BSD-2-Clause; see licenses/nijilive-shallow-texture.txt.
#include <bit>
#include <charconv>
#include <stdexcept>
#include "third_party/stb_image_full.h"

#include <cstring>
#include "texture.hpp"
#include "compat/assert_error.hpp"
#include "compat/system_error.hpp"
#include "common/object_key_policy.hpp"
#include "runtime_state.hpp"
#include "imagefmt/writer.hpp"
#include "imagefmt/reader.hpp"
#include "compat/phobos_file_read.hpp"
#include "compat/phobos_file_write.hpp"
#include "compat/scope_exit.hpp"
#include "render/common.hpp"
#include "render/resource_lifetime_internal.hpp"
#include "render/backend_queue.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <new>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nicxlive::core {

namespace {
template<class Operation> decltype(auto) allocateTextureBytes(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
    }
}
d_runtime::RetainedString copyTextureEnforceMessage(std::string_view formatted) {
    return allocateTextureBytes([&] { return d_runtime::RetainedString(formatted); });
}
void registerTextureClassName() {
    d_runtime::ngRegisterObjectClassName(typeid(Texture),
        d_runtime::RetainedString::fromStaticLiteral("nijilive.core.texture.Texture"));
}
} // namespace

// Source Object defaults, object.d (Digital Mars 2000-2011), BSL-1.0.
// See licenses/druntime-associative-array.txt for the complete notice.
d_runtime::RetainedString Texture::toString() {
    registerTextureClassName();
    return d_runtime::ngObjectClassName(typeid(*this));
}

std::size_t Texture::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(objectIdentity());
    return address ^ (address >> 4);
}

bool Texture::opEquals(std::shared_ptr<Texture> other) {
    return other && objectIdentity() == other->objectIdentity();
}

int Texture::opCmp(std::shared_ptr<Texture>) {
    registerTextureClassName();
    std::string message = "need opCmp for class ";
    message += d_runtime::ngObjectClassName(typeid(*this)).view();
    throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
        d_runtime::RetainedString::fromStaticLiteral("object.d"), 195);
}

bool ngTextureObjectEquals(std::shared_ptr<Texture> left, std::shared_ptr<Texture> right) {
    return common::ngClassObjectEquals(std::move(left), std::move(right), registerTextureClassName);
}

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

void Texture::bind(std::uint32_t unit) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
#ifndef NDEBUG
    if (unit > 31) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        d_runtime::RetainedString::fromStaticLiteral("Outside maximum texture unit value"),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 463);
#endif
    if (sourceInitialized_) {
        if (!sourceHandle_) return;
        const auto backend = currentRenderBackend();
        backend->bindTextureHandle(sourceHandle_, unit);
        return;
    }
    if (backendId_ == 0) return;
    const auto backend = currentRenderBackend();
    backend->bindTextureId(backendId_, unit);
}

std::uint32_t Texture::getTextureId() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (sourceInitialized_) {
        if (!sourceHandle_) return 0;
        const auto backend = tryRenderBackend();
        if (!backend) return 0;
        return static_cast<std::uint32_t>(backend->textureNativeHandle(sourceHandle_));
    }
    if (backendId_ == 0) return 0;
    const auto backend = tryRenderBackend();
    if (!backend) return 0;
    return static_cast<std::uint32_t>(backend->textureNativeHandle(backendId_));
}

void Texture::save(d_runtime::RetainedString file) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto imageWidth = width();
    const auto imageHeight = height();
    if (!sourceInitialized_) {
        const auto pixels = getTextureData(true);
        const auto bytes = pixels ? std::span<const std::uint8_t>(*pixels) : std::span<const std::uint8_t>{};
        (void)imagefmt::writeImageFile(file.view(), imageWidth, imageHeight, bytes, channels_);
        return;
    }
    const auto pixels = getSourceTextureData(true);
    const auto bytes = std::span<const std::uint8_t>(pixels.data(), pixels.size());
    // The original void method deliberately ignores write_image's error result.
    (void)imagefmt::writeImageFile(file.view(), imageWidth, imageHeight, bytes, channels_);
}

ShallowTexture::ShallowTexture(const std::string& file, int channels)
    : ShallowTexture(d_runtime::RetainedString(file), channels) {}

ShallowTexture::ShallowTexture(const char* file, int channels)
    : ShallowTexture(d_runtime::RetainedString(file), channels) {}

ShallowTexture::ShallowTexture(d_runtime::RetainedString file, int channels) {
    const auto fileData = d_runtime::ngReadBinaryFile(file);
    auto image = imagefmt::readImage({fileData.data(), fileData.size()}, 0, 8);
    if (image.e != imagefmt::Error::none) {
        const auto label = std::string_view(imagefmt::errorText[static_cast<std::size_t>(image.e)]);
        const auto filename = file.view();
        const auto formatted = allocateTextureBytes([&] {
            std::string message(label);
            message.append(": ");
            message.append(filename);
            return message;
        });
        throw d_runtime::ThrowableException(copyTextureEnforceMessage(formatted),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 158);
    }
    d_runtime::ngRunWithScopeExit([&] {
        const auto bytes = image.buf8();
        if (bytes.empty()) data.clear();
        else allocateTextureBytes([&] { data.assign(bytes.begin(), bytes.end()); });
        width = image.w;
        height = image.h;
        this->channels = image.c;
        convChannels = channels == 0 ? image.c : channels;
    }, [&] { image.free(); });
}

ShallowTexture::ShallowTexture(const std::vector<uint8_t>& buffer, int channels)
    : ShallowTexture(std::span<const uint8_t>(buffer), channels) {}

ShallowTexture::ShallowTexture(std::span<const uint8_t> buffer, int channels) {
    auto image = imagefmt::readImage(buffer, 0, 8);
    if (image.e != imagefmt::Error::none) {
        const auto label = std::string_view(imagefmt::errorText[static_cast<std::size_t>(image.e)]);
        const auto formatted = allocateTextureBytes([&] { return std::string(label); });
        throw d_runtime::ThrowableException(copyTextureEnforceMessage(formatted),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 186);
    }
    d_runtime::ngRunWithScopeExit([&] {
        const auto bytes = image.buf8();
        if (bytes.empty()) data.clear();
        else allocateTextureBytes([&] { data.assign(bytes.begin(), bytes.end()); });
        width = image.w;
        height = image.h;
        this->channels = image.c;
        convChannels = channels == 0 ? image.c : channels;
    }, [&] { image.free(); });
}

ShallowTexture::ShallowTexture(SourceTextureBytes buffer, int channels)
    : ShallowTexture(std::span<const uint8_t>(buffer.data(), buffer.size()), channels) {}

ShallowTexture::ShallowTexture(const std::vector<uint8_t>& buffer, int w, int h, int channels)
    : ShallowTexture(buffer, w, h, channels, channels) {}

ShallowTexture::ShallowTexture(const std::vector<uint8_t>& buffer, int w, int h, int channels, int convChannels) {
    allocateTextureBytes([&] { data = buffer; });
    width = w;
    height = h;
    this->channels = channels;
    this->convChannels = convChannels;
}

ShallowTexture::ShallowTexture(SourceTextureBytes buffer, int w, int h, int channels)
    : ShallowTexture(std::move(buffer), w, h, channels, channels) {}

ShallowTexture::ShallowTexture(SourceTextureBytes buffer, int w, int h, int channels, int convChannels) {
    data = std::move(buffer);
    width = w;
    height = h;
    this->channels = channels;
    this->convChannels = convChannels;
}

void ShallowTexture::save(d_runtime::RetainedString file) {
    const auto imageWidth = width;
    const auto imageHeight = height;
    const SourceTextureBytes pixels = data;
    const auto channelCount = channels;
    auto image = imagefmt::writeImageMemory(imagefmt::png, imageWidth, imageHeight,
        std::span<const uint8_t>(pixels.data(), pixels.size()), channelCount);
    if (image.error != imagefmt::Error::none) {
        const auto label = std::string_view(imagefmt::errorText[static_cast<std::size_t>(image.error)]);
        const auto formatted = allocateTextureBytes([&] { return std::string(label); });
        throw d_runtime::ThrowableException(copyTextureEnforceMessage(formatted),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 234);
    }
    d_runtime::phobos::ngFileWrite(file,
        std::string_view(reinterpret_cast<const char*>(image.buffer.data()), image.length));
    // The original frees only after the real write returns normally.
    std::free(image.buffer.detach());
}

Texture::Texture(const std::string& file, int channels, bool useMipmaps) {
    int w = 0, h = 0, comp = 0;
    auto img = loadImage(file, w, h, comp, channels);
    if (!img.empty()) {
        initFromData(img, w, h, comp, channels == 0 ? comp : channels, false, useMipmaps);
    }
}

Texture::Texture(const ShallowTexture& shallow, bool useMipmaps) {
    initFromData(shallow.data, shallow.width, shallow.height,
        shallow.channels, shallow.convChannels, false, useMipmaps);
}

Texture::Texture(int w, int h, int channels, bool stencil, bool useMipmaps) {
    std::vector<uint8_t> empty;
    if (!stencil) empty.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * static_cast<std::size_t>(channels));
    initFromData(empty, w, h, channels, channels, stencil, useMipmaps);
}

Texture::Texture(const std::vector<uint8_t>& data, int width, int height, int inChannels, int outChannels, bool stencil, bool useMipmaps) {
    initFromData(data, width, height, inChannels, outChannels, stencil, useMipmaps);
}

Texture::~Texture() noexcept(false) {
    if (sourceInitialized_) dispose();
    else disposeDeferred();
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
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (sourceInitialized_) {
        if (!sourceHandle_) return;
        const auto backend = currentRenderBackend();
        backend->applyTextureFiltering(sourceHandle_, mode, useMipmaps_);
        return;
    }
    filtering_ = mode;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (backend) {
        backend_ = backend;
        if (backendId_ != 0) backend->setTextureParams(backendId_, filtering_, wrapping_, anisotropy_);
    }
}

void Texture::setWrapping(Wrapping mode) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (sourceInitialized_) {
        if (!sourceHandle_) return;
        const auto backend = currentRenderBackend();
        backend->applyTextureWrapping(sourceHandle_, mode);
        return;
    }
    wrapping_ = mode;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (backend) {
        backend_ = backend;
        if (backendId_ != 0) backend->setTextureParams(backendId_, filtering_, wrapping_, anisotropy_);
    }
}

void Texture::setAnisotropy(float val) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (sourceInitialized_) {
        if (!sourceHandle_) return;
        const auto backend = currentRenderBackend();
        const auto handle = sourceHandle_;
        const auto maximum = incGetMaxAnisotropy();
#ifndef NDEBUG
        if (1.0f > maximum) throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("Lower can't be greater than upper."),
            d_runtime::RetainedString::fromStaticLiteral("std/algorithm/comparison.d"), 588);
#endif
        const auto amount = val < 1.0f ? 1.0f : val > maximum ? maximum : val;
        backend->applyTextureAnisotropy(handle, amount);
        return;
    }
    anisotropy_ = val;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (backend) {
        backend_ = backend;
        if (backendId_ != 0) backend->setTextureParams(backendId_, filtering_, wrapping_, anisotropy_);
    }
}

void Texture::setData(const std::vector<uint8_t>& data, int inChannels) {
    if (sourceInitialized_) {
        setData(SourceTextureBytes(std::vector<uint8_t>(data)), inChannels);
        return;
    }
    const auto converted = convertTextureChannels(data, width_, height_, inChannels == -1 ? channels_ : inChannels, channels_);
    if (locked_) {
        lockedData_ = std::make_shared<std::vector<uint8_t>>(converted);
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

TextureData Texture::getTextureData(bool unmultiply) {
    if (sourceInitialized_) {
        const auto bytes = getSourceTextureData(unmultiply);
        auto owner = bytes.vectorBacking();
        if (!owner && bytes.isNull()) return {};
        if (owner && bytes.data() == owner->data() && bytes.size() == owner->size()) return owner;
        // Only this explicit legacy API materializes a descriptor its return type cannot carry.
        return std::make_shared<std::vector<uint8_t>>(bytes.begin(), bytes.end());
    }
    if (locked_) return lockedData_;
    auto buffer = std::make_shared<std::vector<uint8_t>>(
        static_cast<std::size_t>(std::max(0, width_)) * static_cast<std::size_t>(std::max(0, height_))
            * static_cast<std::size_t>(std::max(0, channels_)));
    if (backendId_ == 0) return buffer->empty() ? TextureData{} : buffer;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (!backend) throw std::runtime_error("RenderBackend is not available.");
    backend->readTextureData(backendId_, channels_, stencil_, *buffer);
    if (unmultiply && channels_ == 4) {
        for (std::size_t i = 0; i < buffer->size() / 4; ++i) {
            const auto offset = i * 4;
            const int alpha = (*buffer)[offset + 3];
            if (alpha == 0) continue;
            for (std::size_t channel = 0; channel < 3; ++channel)
                (*buffer)[offset + channel] = static_cast<uint8_t>(int((*buffer)[offset + channel]) * 255 / alpha);
        }
    }
    return buffer->empty() ? TextureData{} : buffer;
}

void Texture::setData(TextureData data, int inChannels) {
    if (sourceInitialized_) {
        setData(SourceTextureBytes(data), inChannels);
        return;
    }
    const int actualChannels = inChannels == -1 ? channels_ : inChannels;
    if (locked_) {
        lockedData_ = std::move(data);
        modified_ = true;
        return;
    }
    if (backendId_ == 0) return;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (!backend) throw std::runtime_error("RenderBackend is not available.");
    static const std::vector<uint8_t> empty;
    backend->uploadTextureData(backendId_, width_, height_, actualChannels, channels_, stencil_, data ? *data : empty);
    // Retain the legacy committed CPU mirror without using it for readback.
    data_ = data ? *data : empty;
    genMipmap();
}

void Texture::genMipmap() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (sourceInitialized_) {
        if (stencil_ || !sourceHandle_ || !useMipmaps_) return;
        const auto backend = currentRenderBackend();
        backend->generateTextureMipmap(sourceHandle_);
        return;
    }
    if (stencil_ || backendId_ == 0 || !useMipmaps_) return;
    auto backend = backend_.lock();
    if (!backend) backend = getCurrentRenderBackend();
    if (!backend) throw std::runtime_error("RenderBackend is not available.");
    backend->generateTextureMipmap(backendId_);
}

void Texture::dispose() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (sourceInitialized_) {
        if (sourceHandle_) {
            const auto backend = tryRenderBackend();
            if (backend) backend->destroyTextureHandle(sourceHandle_);
        }
        sourceHandle_.reset();
        return;
    }
    // Preserve the existing legacy constructor/API domain.
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
    render::detail::drainDrawableReleases();
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
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (sourceInitialized_) {
        if (!locked_) {
            sourceLockedData_ = getSourceTextureData();
            modified_ = false;
            locked_ = true;
        }
        return;
    }
    if (locked_) return;
    lockedData_ = getTextureData();
    modified_ = false;
    locked_ = true;
}

void Texture::unlock() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (sourceInitialized_) {
        if (locked_) {
            locked_ = false;
            if (modified_) setData(sourceLockedData_, channels_);
            modified_ = false;
            sourceLockedData_ = {};
        }
        return;
    }
    if (!locked_) return;
    locked_ = false;
    if (modified_) setData(lockedData_, channels_);
    modified_ = false;
    lockedData_.reset();
}

uint32_t Texture::nextUUID() {
    return nodes::Node::inCreateUUID();
}

void inTexUnPremuliply(std::vector<std::uint8_t>& data) {
    for (std::size_t i = 0; i < data.size() / 4; ++i) {
        const auto alpha = data[i * 4 + 3];
        if (alpha == 0) continue;
        for (std::size_t channel = 0; channel < 3; ++channel)
            data[i * 4 + channel] = static_cast<std::uint8_t>(
                int(data[i * 4 + channel]) * 255 / int(alpha));
    }
}

std::uint32_t Texture::backendId() const {
    if (!sourceInitialized_) return backendId_;
    if (!sourceHandle_) return 0;
    const auto backend = tryRenderBackend();
    if (!backend) return 0;
    return static_cast<std::uint32_t>(backend->textureNativeHandle(sourceHandle_));
}
namespace {
std::size_t sourceTextureByteCount(int width, int height, int channels) {
    // D int arithmetic wraps before conversion to the array length's size_t.
    const std::uint32_t bits = static_cast<std::uint32_t>(width)
        * static_cast<std::uint32_t>(height) * static_cast<std::uint32_t>(channels);
    return static_cast<std::size_t>(std::bit_cast<std::int32_t>(bits));
}

std::shared_ptr<Texture> allocateSourceTexture() {
    try {
        return std::make_shared<Texture>();
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
    }
}
}
std::shared_ptr<Texture> Texture::allocateSourceReceiver() {
    return allocateSourceTexture();
}
std::shared_ptr<Texture> Texture::createSource(int width, int height, int channels,
    bool stencil, bool useMipmaps) {
    auto result = allocateSourceTexture();
    // Original303 intentionally uses the new object's PRE-assignment fields.
    SourceTextureBytes empty(stencil ? 0 : sourceTextureByteCount(result->width_, result->height_, channels), 0);
    result->constructSource(empty, width, height, channels, channels, stencil, useMipmaps);
    return result;
}
std::shared_ptr<Texture> Texture::createSource(SourceTextureBytes data, int width, int height,
    int inChannels, int outChannels, bool stencil, bool useMipmaps) {
    auto result = allocateSourceTexture();
    result->constructSource(data, width, height, inChannels, outChannels, stencil, useMipmaps);
    return result;
}
std::shared_ptr<Texture> Texture::createSource(ShallowTexture shallow, bool useMipmaps) {
    return createSource(shallow.data, shallow.width, shallow.height,
        shallow.channels, shallow.convChannels, false, useMipmaps);
}
std::shared_ptr<Texture> Texture::createSource(d_runtime::RetainedString file,
    int channels, bool useMipmaps) {
    auto result = allocateSourceTexture();
    const auto fileData = d_runtime::ngReadBinaryFile(file);
    auto image = std::make_shared<imagefmt::Image>(
        imagefmt::readImage({fileData.data(), fileData.size()}, 0, 8));
    if (image->e != imagefmt::Error::none) {
        std::string message(imagefmt::errorText[static_cast<std::size_t>(image->e)]);
        message.append(": ");
        message.append(file.view());
        throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 280);
    }
    d_runtime::ngRunWithScopeExit([&] {
        const auto pixels = image->buf8();
        const auto input = pixels.data()
            ? SourceTextureBytes::retainedView(image, pixels.data(), pixels.size()) : SourceTextureBytes{};
        result->constructSource(input, image->w, image->h, image->c,
            channels == 0 ? image->c : channels, false, useMipmaps);
        // Original file constructor assigns a second UUID after delegation.
        result->runtimeUUID_ = nodes::Node::inCreateUUID();
    }, [&] { image->free(); });
    return result;
}
void Texture::constructSource(SourceTextureBytes data, int width, int height,
    int inChannels, int outChannels, bool stencil, bool useMipmaps) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    sourceInitialized_ = true;
    width_ = width;
    height_ = height;
    channels_ = outChannels;
    stencil_ = stencil;
    useMipmaps_ = useMipmaps;
    {
        const auto backend = currentRenderBackend();
        sourceHandle_ = backend->createTextureHandle();
        // A complete real owner exists before these logical-constructor virtual calls.
        setData(data, inChannels);
        setFiltering(Filtering::Linear);
        setWrapping(Wrapping::Clamp);
        setAnisotropy(incGetMaxAnisotropy() / 2.0f);
    }
    runtimeUUID_ = nodes::Node::inCreateUUID();
}
SourceTextureBytes Texture::getSourceTextureData(bool unmultiply) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (locked_) return sourceInitialized_ ? sourceLockedData_ : SourceTextureBytes(lockedData_);
    const auto imageWidth = width();
    const auto imageHeight = height();
    const auto imageChannels = channels_;
    SourceTextureBytes buffer(sourceTextureByteCount(imageWidth, imageHeight, imageChannels), 0);
    if (sourceInitialized_) {
        if (!sourceHandle_) return buffer;
        const auto backend = currentRenderBackend();
        backend->readTextureData(sourceHandle_, channels_, stencil_, buffer);
    } else {
        if (!backendId_) return buffer;
        const auto backend = currentRenderBackend();
        backend->readTextureData(backendId_, channels_, stencil_, buffer);
    }
    if (unmultiply && channels() == 4) inTexUnPremuliply(buffer);
    return buffer;
}
void Texture::setData(SourceTextureBytes data, int inChannels) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto actualChannels = inChannels == -1 ? channels_ : inChannels;
    if (locked_) {
        if (sourceInitialized_) sourceLockedData_ = data;
        else {
            auto owner = data.vectorBacking();
            if (owner && data.data() == owner->data() && data.size() == owner->size()) lockedData_ = owner;
            else if (data.isNull()) lockedData_.reset();
            else lockedData_ = std::make_shared<std::vector<uint8_t>>(data.begin(), data.end());
        }
        modified_ = true;
        return;
    }
    if (sourceInitialized_) {
        if (!sourceHandle_) return;
        const auto backend = currentRenderBackend();
        backend->uploadTextureData(sourceHandle_, width_, height_, actualChannels, channels_, stencil_, data);
    } else {
        if (!backendId_) return;
        const auto backend = currentRenderBackend();
        backend->uploadTextureData(backendId_, width_, height_, actualChannels, channels_, stencil_, data);
    }
    genMipmap();
}
std::shared_ptr<Texture> Texture::dup() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto result = createSource(width_, height_, channels_, stencil_);
    const auto bytes = getSourceTextureData();
    const auto inputChannels = channels_;
    result->setData(bytes, inputChannels);
    return result;
}
float incGetMaxAnisotropy() {
    const auto backend = tryRenderBackend();
    if (backend) return backend->maxTextureAnisotropy();
    return 1;
}
void inTexUnPremuliply(SourceTextureBytes& data) {
    const auto count = data.size() / 4;
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = i * 4;
        if (data[offset + 3] == 0) continue;
        data[offset] = static_cast<uint8_t>(int(data[offset]) * 255 / int(data[offset + 3]));
        data[offset + 1] = static_cast<uint8_t>(int(data[offset + 1]) * 255 / int(data[offset + 3]));
        data[offset + 2] = static_cast<uint8_t>(int(data[offset + 2]) * 255 / int(data[offset + 3]));
    }
}

int Texture::colorMode() const {
    switch (channels_) {
    case 1: return 0x1903;
    case 2: return 0x8227;
    case 3: return 0x1907;
    default: return 0x1908;
    }
}

math::Vec2i Texture::center() { return {width_ / 2, height_ / 2}; }
math::Vec2i Texture::size() { return {width_, height_}; }

void Texture::setDataRegion(SourceTextureBytes data, int x, int y, int width, int height, int channels) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto actualChannels = channels == -1 ? channels_ : channels;
    const auto endX = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(x)
        + static_cast<std::uint32_t>(width));
    const auto endY = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(y)
        + static_cast<std::uint32_t>(height));
    // Source enforce evaluates its formatted message only on the failed axis.
    const auto fail = [](char axis, int offset, int bound, std::size_t line) {
        std::string message(1, axis);
        message += " offset is out of bounds (";
        message += axis;
        message += "offset=";
        char digits[12];
        auto number = std::to_chars(digits, digits + sizeof(digits), offset);
        message.append(digits, number.ptr);
        message += ", ";
        message += axis;
        message += "bound=";
        number = std::to_chars(digits, digits + sizeof(digits), bound);
        message.append(digits, number.ptr);
        message += ')';
        throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), line);
    };
    if (!(x >= 0 && endX <= width_)) fail('x', endX, width_, 444);
    if (!(y >= 0 && endY <= height_)) fail('y', endY, height_, 445);
    if (!sourceHandle_) return;
    const auto backend = currentRenderBackend();
    backend->updateTextureRegion(sourceHandle_, x, y, width, height, actualChannels, data);
    genMipmap();
}

void inTexPremultiply(SourceTextureBytes& data, int channels) {
    if (channels < 4) return;
    const auto count = data.size() / static_cast<std::size_t>(channels);
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = i * static_cast<std::size_t>(channels);
        data[offset] = static_cast<std::uint8_t>((int(data[offset]) * int(data[offset + 3])) / 255);
        data[offset + 1] = static_cast<std::uint8_t>((int(data[offset + 1]) * int(data[offset + 3])) / 255);
        data[offset + 2] = static_cast<std::uint8_t>((int(data[offset + 2]) * int(data[offset + 3])) / 255);
    }
}

} // namespace nicxlive::core
