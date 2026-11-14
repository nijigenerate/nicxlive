#pragma once

#include <texture.hpp>
#include <puppet.hpp>
#include "binfmt.hpp"
#include "io.hpp"
#include "serialize.hpp"
#include "typed_reader.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>

namespace nicxlive::core::fmt {

inline std::vector<uint8_t> encodeTga(const std::vector<uint8_t>& data, int width, int height, int channels) {
    if (width <= 0 || height <= 0 || (channels != 3 && channels != 4)) return {};
    std::vector<uint8_t> out;
    out.resize(18); // header
    out[2] = 2;     // uncompressed true-color
    out[12] = static_cast<uint8_t>(width & 0xFF);
    out[13] = static_cast<uint8_t>((width >> 8) & 0xFF);
    out[14] = static_cast<uint8_t>(height & 0xFF);
    out[15] = static_cast<uint8_t>((height >> 8) & 0xFF);
    out[16] = static_cast<uint8_t>(channels * 8);
    out[17] = static_cast<uint8_t>((channels == 4 ? 8 : 0) | 0x20); // origin top-left, alpha bits

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::size_t expected = pixelCount * static_cast<std::size_t>(channels);
    if (data.size() < expected) return {};
    out.reserve(out.size() + expected);
    for (std::size_t i = 0; i < expected; i += static_cast<std::size_t>(channels)) {
        // TGA uses BGR(A)
        out.push_back(data[i + 2]); // B
        out.push_back(data[i + 1]); // G
        out.push_back(data[i + 0]); // R
        if (channels == 4) out.push_back(data[i + 3]); // A
    }
    return out;
}

inline bool& inpModeFlag() {
    static thread_local bool flag = false;
    return flag;
}

inline bool inIsINPMode() {
    return inpModeFlag();
}

// JSON-based load/save
template <typename T>
std::shared_ptr<T> inLoadJSONPuppet(const std::string& data) {
    inpModeFlag() = false;
    return inLoadJsonDataFromMemory<T>(data);
}

template <typename T>
std::shared_ptr<T> inLoadPuppetFromMemory(common::SharedArray<uint8_t> data) {
    const bool binary = data.size() >= MAGIC_BYTES.size() &&
        std::equal(MAGIC_BYTES.begin(), MAGIC_BYTES.end(), data.begin());
    inpModeFlag() = binary;
    if (binary) return inLoadTypedPuppetBinary<T>(data);
    return inLoadJSONPuppet<T>(
        data.empty() ? std::string{} : std::string(reinterpret_cast<const char*>(data.data()), data.size()));
}

template <typename T>
std::shared_ptr<T> inLoadPuppetFromMemory(const std::vector<uint8_t>& data) {
    return inLoadPuppetFromMemory<T>(common::SharedArray<uint8_t>(data));
}

template <typename T>
std::shared_ptr<T> inLoadPuppet(const std::string& file) {
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open puppet file: " + file);
    }
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    const auto ext = std::filesystem::path(file).extension().string();
    if (ext == ".inp" || ext == ".inx") {
        if (!inVerifyMagicBytes(buffer)) {
            throw std::runtime_error("Invalid data format for INP/INX puppet");
        }
        return inLoadPuppetFromMemory<T>(common::SharedArray<uint8_t>(std::move(buffer)));
    }
    throw std::runtime_error("Invalid file format for puppet: " + ext);
}

inline std::vector<uint8_t> inWriteINPPuppetMemory(const class Puppet& p) {
    inpModeFlag() = true;
    std::vector<uint8_t> app;
    auto appendU32 = [&](uint32_t v) {
        app.push_back((v >> 24) & 0xFF);
        app.push_back((v >> 16) & 0xFF);
        app.push_back((v >> 8) & 0xFF);
        app.push_back(v & 0xFF);
    };

    // Magic
    app.insert(app.end(), MAGIC_BYTES.begin(), MAGIC_BYTES.end());

    // Puppet JSON
    std::string puppetJson = inToJson(p);
    if (puppetJson.empty()) puppetJson = "{}";
    appendU32(static_cast<uint32_t>(puppetJson.size()));
    app.insert(app.end(), puppetJson.begin(), puppetJson.end());

    // Texture section
    app.insert(app.end(), TEX_SECTION.begin(), TEX_SECTION.end());
    uint32_t slotCount = static_cast<uint32_t>(p.textureSlots.size());
    appendU32(slotCount);
    for (auto& tex : p.textureSlots) {
        // length
        if (!tex || tex->data().empty()) {
            appendU32(0);
            app.push_back(1); // IN_TEX_TGA
            continue;
        }
        const auto& data = tex->data();
        auto tga = encodeTga(data, tex->width(), tex->height(), tex->channels());
        appendU32(static_cast<uint32_t>(tga.size()));
        app.push_back(1); // IN_TEX_TGA
        app.insert(app.end(), tga.begin(), tga.end());
    }

    // EXT section
    if (!p.extData.empty()) {
        app.insert(app.end(), EXT_SECTION.begin(), EXT_SECTION.end());
        appendU32(static_cast<uint32_t>(p.extData.size()));
        for (auto& kv : p.extData) {
            const auto& name = kv.first;
            const auto& payload = kv.second;
            appendU32(static_cast<uint32_t>(name.size()));
            app.insert(app.end(), name.data(), name.data() + name.size());
            appendU32(static_cast<uint32_t>(payload.size()));
            app.insert(app.end(), payload.begin(), payload.end());
        }
    }

    return app;
}

inline void inWriteINPPuppet(const class Puppet& p, const std::string& file) {
    auto data = inWriteINPPuppetMemory(p);
    std::ofstream ofs(file, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

} // namespace nicxlive::core::fmt
