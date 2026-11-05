#pragma once
// Shallow/source factory port: Inochi2D Project 2020; nijigenerate Project 2024.
// BSD-2-Clause; see licenses/nijilive-shallow-texture.txt.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <span>
#include <algorithm>
#include <iterator>
#include <initializer_list>
#include <functional>
#include <type_traits>
#include <utility>
#include "compat/retained_string.hpp"
#include "common/shared_array.hpp"
#include "render/backend_handles.hpp"
#include "math/types.hpp"

namespace nicxlive::core {

class RenderBackend;

using TextureData = std::shared_ptr<std::vector<uint8_t>>;
using SourceTextureBytes = common::SharedArray<uint8_t>;

enum class Filtering {
    Linear,
    Nearest,
    Point = Nearest,
};

enum class Wrapping {
    Clamp,
    Repeat,
    Mirror,
};

// One canonical byte descriptor; native vector bridges do not add a second store.
class ShallowTextureBytes : public SourceTextureBytes {
public:
    using SourceTextureBytes::SourceTextureBytes;
    using SourceTextureBytes::assign;
    ShallowTextureBytes() = default;
    ShallowTextureBytes(SourceTextureBytes bytes) : SourceTextureBytes(std::move(bytes)) {}
    ShallowTextureBytes& operator=(SourceTextureBytes bytes) {
        SourceTextureBytes::operator=(std::move(bytes));
        return *this;
    }
    ShallowTextureBytes& operator=(std::vector<uint8_t> bytes) {
        return *this = SourceTextureBytes(std::move(bytes));
    }
    ShallowTextureBytes& operator=(std::initializer_list<uint8_t> bytes) {
        return *this = SourceTextureBytes(bytes);
    }
    template<std::input_iterator Iterator> void assign(Iterator first, Iterator last) {
        if (first == last) { *this = SourceTextureBytes{}; return; }
        SourceTextureBytes bytes(static_cast<std::size_t>(std::distance(first, last)), 0);
        std::copy(first, last, bytes.begin());
        *this = std::move(bytes);
    }
    operator std::vector<uint8_t>() const {
        return empty() ? std::vector<uint8_t>{} : std::vector<uint8_t>(begin(), end());
    }
};

struct ShallowTexture {
    ShallowTextureBytes data{};
    int width{0};
    int height{0};
    int channels{0};
    int convChannels{0};

    ShallowTexture() = default;
    ShallowTexture(d_runtime::RetainedString file, int channels = 0);
    ShallowTexture(const std::string& file, int channels = 0);
    ShallowTexture(const char* file, int channels = 0);
    ShallowTexture(const std::vector<uint8_t>& buffer, int channels = 0);
    ShallowTexture(std::span<const uint8_t> buffer, int channels = 0);
    ShallowTexture(SourceTextureBytes buffer, int channels = 0);
    ShallowTexture(const std::vector<uint8_t>& buffer, int w, int h, int channels = 4);
    ShallowTexture(const std::vector<uint8_t>& buffer, int w, int h, int channels, int convChannels);
    ShallowTexture(SourceTextureBytes buffer, int w, int h, int channels = 4);
    ShallowTexture(SourceTextureBytes buffer, int w, int h, int channels, int convChannels);
    void save(d_runtime::RetainedString file);
};

class Texture : public std::enable_shared_from_this<Texture> {
public:
    Texture() = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;
    Texture(const std::string& file, int channels = 0, bool useMipmaps = true);
    Texture(const ShallowTexture& shallow, bool useMipmaps = true);
    Texture(int w, int h, int channels = 4, bool stencil = false, bool useMipmaps = true);
    Texture(const std::vector<uint8_t>& data, int width, int height, int inChannels = 4, int outChannels = 4, bool stencil = false, bool useMipmaps = true);

    virtual ~Texture() noexcept(false);

    virtual void setFiltering(bool point);
    virtual void setFiltering(Filtering mode);
    virtual void setWrapping(Wrapping mode);
    virtual void setAnisotropy(float val);
    virtual void setData(const std::vector<uint8_t>& data, int inChannels);
    // D slice-compatible API: locked reads and writes retain shared byte ownership.
    virtual TextureData getTextureData(bool unmultiply = false);
    virtual void save(d_runtime::RetainedString file);
    virtual void setData(TextureData data, int inChannels = -1);
    virtual void genMipmap();
    virtual void dispose();
    virtual void lock();
    virtual void unlock();
    virtual uint32_t backendId() const;
    virtual std::uint32_t getTextureId();

    virtual uint32_t getRuntimeUUID() const { return runtimeUUID_; }
    virtual void setRuntimeUUID(uint32_t v) { runtimeUUID_ = v; }

    const std::vector<uint8_t>& data() const { return data_; }
    virtual int width() const { return width_; }
    virtual int height() const { return height_; }
    int channels() const { return channels_; }
    bool stencil() const { return stencil_; }
    bool locked() const { return locked_; }
    bool modified() const { return modified_; }

    virtual void bind(std::uint32_t unit = 0);

private:
    static uint32_t nextUUID();
    static std::shared_ptr<Texture> allocateSourceReceiver();
    void initFromData(const std::vector<uint8_t>& data, int width, int height, int inChannels, int outChannels, bool stencil, bool useMipmaps);
    void disposeDeferred();

    int width_{0};
    int height_{0};
    int channels_{4};
    bool stencil_{false};
    bool useMipmaps_{true};
    Filtering filtering_{Filtering::Linear};
    Wrapping wrapping_{Wrapping::Clamp};
    float anisotropy_{1.0f};
    std::vector<uint8_t> data_{};
    bool locked_{false};
    bool modified_{false};
    TextureData lockedData_{};
    uint32_t runtimeUUID_{0};
    uint32_t backendId_{0};
    std::weak_ptr<RenderBackend> backend_{};
    bool sourceInitialized_ = false;
    std::shared_ptr<RenderTextureHandle> sourceHandle_;
    SourceTextureBytes sourceLockedData_;
protected:
    // Logical Source constructor: call only after the real most-derived owner exists.
    void constructSource(SourceTextureBytes data, int width, int height, int inChannels,
        int outChannels, bool stencil, bool useMipmaps);
public:
    static std::shared_ptr<Texture> createSource(int width, int height, int channels = 4,
        bool stencil = false, bool useMipmaps = true);
    static std::shared_ptr<Texture> createSource(SourceTextureBytes data, int width, int height,
        int inChannels = 4, int outChannels = 4, bool stencil = false, bool useMipmaps = true);
    static std::shared_ptr<Texture> createSource(ShallowTexture shallow, bool useMipmaps = true);
    template<class DataProducer, class WidthProducer, class HeightProducer,
        class InputChannelsProducer, class OutputChannelsProducer, class StencilProducer, class MipmapsProducer>
    static std::shared_ptr<Texture> createDataSourceDeferred(DataProducer&& makeData,
        WidthProducer&& makeWidth, HeightProducer&& makeHeight, InputChannelsProducer&& makeInputChannels,
        OutputChannelsProducer&& makeOutputChannels, StencilProducer&& makeStencil, MipmapsProducer&& makeMipmaps) {
        auto result = allocateSourceReceiver();
        SourceTextureBytes data = std::invoke(std::forward<DataProducer>(makeData));
        int width = std::invoke(std::forward<WidthProducer>(makeWidth));
        int height = std::invoke(std::forward<HeightProducer>(makeHeight));
        int inChannels = std::invoke(std::forward<InputChannelsProducer>(makeInputChannels));
        int outChannels = std::invoke(std::forward<OutputChannelsProducer>(makeOutputChannels));
        bool stencil = std::invoke(std::forward<StencilProducer>(makeStencil));
        bool useMipmaps = std::invoke(std::forward<MipmapsProducer>(makeMipmaps));
        result->constructSource(data, width, height, inChannels, outChannels, stencil, useMipmaps);
        return result;
    }
    // Source new allocates the real receiver before evaluating constructor arguments.
    template<class Producer>
        requires std::is_same_v<std::remove_cvref_t<std::invoke_result_t<Producer>>, ShallowTexture>
    static std::shared_ptr<Texture> createSourceDeferred(Producer&& producer, bool useMipmaps = true) {
        auto result = allocateSourceReceiver();
        ShallowTexture shallow = std::invoke(std::forward<Producer>(producer));
        result->constructSource(shallow.data, shallow.width, shallow.height,
            shallow.channels, shallow.convChannels, false, useMipmaps);
        return result;
    }
    static std::shared_ptr<Texture> createSource(d_runtime::RetainedString file,
        int channels = 0, bool useMipmaps = true);
    // Append Source mutable slots without changing the existing const API or its slot order.
    virtual int width() { return width_; }
    virtual int height() { return height_; }
    virtual int channels() { return channels_; }
    virtual uint32_t getRuntimeUUID() { return runtimeUUID_; }
    virtual SourceTextureBytes getSourceTextureData(bool unmultiply = false);
    virtual void setData(SourceTextureBytes data, int inChannels = -1);
    virtual std::shared_ptr<RenderTextureHandle> backendHandle() { return sourceHandle_; }
    virtual std::shared_ptr<Texture> dup();

    // Inherited Source Object slots follow every existing Texture virtual slot.
    const void* objectIdentity() const noexcept { return dynamic_cast<const void*>(this); }
    virtual d_runtime::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<Texture> other);
    virtual int opCmp(std::shared_ptr<Texture> other);

    // Remaining Source slots follow the existing vtable entries.
    virtual int colorMode() const;
    virtual math::Vec2i center();
    virtual math::Vec2i size();
    virtual void setDataRegion(SourceTextureBytes data, int x, int y, int width, int height,
        int channels = -1);
};

bool ngTextureObjectEquals(std::shared_ptr<Texture> left, std::shared_ptr<Texture> right);

// Original D texture-loading session; session state and bindings are thread-local.
void inBeginTextureLoading();
std::shared_ptr<Texture> inGetTextureFromId(std::uint32_t id);
std::shared_ptr<Texture> inGetLatestTexture();
void inAddTextureBinary(const ShallowTexture& data);
namespace detail { void endTextureLoading(bool checkErrors); }
template<bool checkErrors = true> void inEndTextureLoading() {
    detail::endTextureLoading(checkErrors);
}

void inDrainPendingTextureDisposals();
void inTexUnPremuliply(std::vector<std::uint8_t>& data);
void inTexUnPremuliply(SourceTextureBytes& data);
void inTexPremultiply(SourceTextureBytes& data, int channels = 4);
float incGetMaxAnisotropy();

} // namespace nicxlive::core
