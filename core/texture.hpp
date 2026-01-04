#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nicxlive::core {

enum class Filtering {
    Linear,
    Nearest,
};

enum class Wrapping {
    Clamp,
    Repeat,
};

struct ShallowTexture {
    std::vector<uint8_t> data{};
    int width{0};
    int height{0};
    int channels{0};
    int convChannels{0};

    ShallowTexture() = default;
    ShallowTexture(const std::string& file, int channels = 0);
    ShallowTexture(const std::vector<uint8_t>& buffer, int channels = 0);
    ShallowTexture(const std::vector<uint8_t>& buffer, int w, int h, int channels = 4);
    ShallowTexture(const std::vector<uint8_t>& buffer, int w, int h, int channels, int convChannels);
};

class Texture {
public:
    Texture() = default;
    Texture(const std::string& file, int channels = 0, bool useMipmaps = true);
    Texture(const ShallowTexture& shallow, bool useMipmaps = true);
    Texture(int w, int h, int channels = 4, bool stencil = false, bool useMipmaps = true);
    Texture(const std::vector<uint8_t>& data, int width, int height, int inChannels = 4, int outChannels = 4, bool stencil = false, bool useMipmaps = true);

    virtual ~Texture() = default;

    virtual void setFiltering(bool point);
    virtual void setFiltering(Filtering mode);
    virtual void setWrapping(Wrapping mode);
    virtual void setAnisotropy(float val);
    virtual void setData(const std::vector<uint8_t>& data, int inChannels);
    virtual void dispose();
    virtual void lock();
    virtual void unlock();
    virtual uint32_t backendId() const { return backendId_; }

    virtual uint32_t getRuntimeUUID() const { return runtimeUUID_; }
    virtual void setRuntimeUUID(uint32_t v) { runtimeUUID_ = v; }

    const std::vector<uint8_t>& data() const { return data_; }
    int width() const { return width_; }
    int height() const { return height_; }
    int channels() const { return channels_; }
    bool stencil() const { return stencil_; }
    bool locked() const { return locked_; }
    bool modified() const { return modified_; }

private:
    static uint32_t nextUUID();
    void initFromData(const std::vector<uint8_t>& data, int width, int height, int inChannels, int outChannels, bool stencil, bool useMipmaps);

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
    uint32_t runtimeUUID_{0};
    uint32_t backendId_{0};
};

} // namespace nicxlive::core
