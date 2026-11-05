// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of the imagefmt 2.1.2 writer closure. See LICENSE.imagefmt.
#pragma once
#include "compat.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <cstdio>
#include <string_view>
#include <utility>

namespace nicxlive::core::imagefmt {

enum class Error : int {
    none, fopen, oom, stream, data, oddfmt, unsupp, dim, arg, bigimg,
    nodata, lackdata, zinit, zstream
};
enum Format : int { bmp, tga, png, jpg };
inline constexpr const char* errorText[] = {
    "no error", "cannot open file", "out of memory", "stream error", "bad data",
    "unknown format", "unsupported", "invalid dimensions", "bad argument",
    "image too large", "no data", "not enough data", "zlib init failed", "zlib stream error"
};

// D module variables are thread local. Hooks must be installed as one allocator family.
struct Allocator {
    void* context{};
    void* (*allocate)(void*, std::size_t){};
    void* (*resize)(void*, void*, std::size_t){};
    void (*release)(void*, void*){};
};
extern thread_local Allocator allocator;
extern thread_local int verticalOrientationWrite;

class Buffer {
public:
    Buffer() noexcept : owner_(allocator) {}
    ~Buffer() { if (!sourceLifetime_) reset(); }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    bool resize(std::size_t size) noexcept;
    void reset() noexcept;
    // Source IFImage/ubyte[] descriptors have explicit free, not a scope destructor.
    // Allocation hooks are D module TLS and are read afresh at every operation.
    bool sourceAllocate(std::size_t size);
    bool sourceReallocate(std::size_t size);
    void sourceFree();
    bool hasSourceLifetime() const noexcept { return sourceLifetime_; }
    static Buffer sourceDescriptor(std::uint8_t* pointer, std::size_t size) noexcept {
        Buffer result;
        result.sourceLifetime_ = true;
        result.data_ = pointer;
        result.size_ = size;
        return result;
    }
    static Buffer adopt(std::uint8_t* pointer, std::size_t size, Allocator owner) noexcept {
        Buffer result;
        result.owner_ = owner;
        result.data_ = pointer;
        result.size_ = size;
        return result;
    }
    std::uint8_t* detach() noexcept {
        size_ = 0;
        return std::exchange(data_, nullptr);
    }
    std::uint8_t* data() noexcept { return data_; }
    const std::uint8_t* data() const noexcept { return data_; }
    std::size_t size() const noexcept { return size_; }
private:
    bool sourceLifetime_{};
    Allocator owner_;
    std::uint8_t* data_{};
    std::size_t size_{};
};

struct EncodedImage {
    Buffer buffer;
    std::size_t length{};
    Error error{Error::none};
};

EncodedImage writeImageMemory(int format, int width, int height,
    std::span<const std::uint8_t> data, int requestedChannels);
Error writeImageFile(std::string_view file, int width, int height,
    std::span<const std::uint8_t> data, int requestedChannels = 0);

namespace detail {
struct Writer {
    Buffer buffer = Buffer::sourceDescriptor(nullptr, 0);
    int used{};
    bool failed{};
    std::FILE* stream{};
    bool initialize();
    bool initialize(std::FILE*);
    void weakFlush();
    void moreSpace();
    Error fullFlush();
    void byte(std::uint8_t value);
    void little16(std::uint16_t value);
    void little32(std::uint32_t value);
    void block(std::span<const std::uint8_t> bytes);
};
// Input channels are Y, YA, RGB or RGBA; output 5/6 means BGR/BGRA.
void convertLine(std::span<const std::uint8_t> source, std::span<std::uint8_t> target,
    int sourceChannels, int targetFormat);
Error writeBmp(Writer&, int, int, std::span<const std::uint8_t>, int);
Error writeTga(Writer&, int, int, std::span<const std::uint8_t>, int);
Error writePng(Writer&, int, int, std::span<const std::uint8_t>, int);
} // namespace detail
} // namespace nicxlive::core::imagefmt
