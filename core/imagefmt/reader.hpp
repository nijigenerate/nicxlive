// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of imagefmt 2.1.2. See LICENSE.imagefmt.
#pragma once
#include "writer.hpp"

namespace nicxlive::core::imagefmt {
extern thread_local std::uint64_t maximumImageSize;
extern thread_local int verticalOrientationRead;
struct ImageInfo { int w{}, h{}; std::uint8_t c{}; Error e{}; };
struct Image {
    int w{}, h{};
    std::uint8_t c{}, cinfile{}, bpc{};
    Error e{};
    Buffer buffer;
    void free() { buffer.sourceFree(); }
    std::span<std::uint8_t> buf8() { return {buffer.data(), buffer.size()}; }
    std::span<std::uint16_t> buf16() {
        return {reinterpret_cast<std::uint16_t*>(buffer.data()), buffer.size() / 2};
    }
};
struct Read {
    void* stream{};
    int (*read)(void*, std::uint8_t*, int){};
    int (*seek)(void*, int){};
};
Image readImage(std::string_view file, int channels = 0, int bits = 8);
Image readImage(std::FILE* file, int channels = 0, int bits = 8);
Image readImage(std::span<const std::uint8_t>, int channels = 0, int bits = 8);
Image readImage(Read, int channels = 0, int bits = 8);
ImageInfo readInfo(std::string_view file);
ImageInfo readInfo(std::FILE* file);
ImageInfo readInfo(std::span<const std::uint8_t>);
ImageInfo readInfo(Read);
namespace detail {
struct Reader {
    Read io{};
    const std::uint8_t* buf{};
    int cap{}, a{}, b{}, bufpos{}, iopos{};
    bool fail{};
    Error initialize(std::span<const std::uint8_t>, Read = {});
    void fill();
    void reset();
    void skip(int);
    void skipTo(int);
    std::uint8_t byte();
    std::uint16_t little16();
    std::uint16_t big16();
    std::uint32_t little32();
    std::uint32_t big32();
    void block(std::span<std::uint8_t>);
    std::span<const std::uint8_t> slice(int);
};
bool detectBmp(Reader&);
bool detectTga(Reader&);
bool detectPng(Reader&);
bool detectJpeg(Reader&);
Error readBmp(Reader&, Image&, int, int);
Error readTga(Reader&, Image&, int, int);
Error readPng(Reader&, Image&, int, int);
Error readJpeg(Reader&, Image&, int, int);
ImageInfo readBmpInfo(Reader&);
ImageInfo readTgaInfo(Reader&);
ImageInfo readPngInfo(Reader&);
ImageInfo readJpegInfo(Reader&);
bool convertBits(Buffer&, int sourceBits, int targetBits);
// Format channels: Y=1, YA=2, RGB=3, RGBA=4, BGR=5, BGRA=6.
void convertPixels(std::span<const std::uint8_t>, std::span<std::uint8_t>, int, int);
void convertPixels(std::span<const std::uint16_t>, std::span<std::uint16_t>, int, int);
} // namespace detail
} // namespace nicxlive::core::imagefmt
