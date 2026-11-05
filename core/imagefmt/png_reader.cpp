// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of imagefmt 2.1.2 PNG reader. See LICENSE.imagefmt.
#include "reader.hpp"
#include "png_state.hpp"
#include <compat/assert_error.hpp>
#include <compat/array_range_error.hpp>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <utility>

namespace nicxlive::core::imagefmt::detail {
namespace {
using Bytes = std::span<std::uint8_t>;
using ConstBytes = std::span<const std::uint8_t>;

constexpr std::array<std::uint8_t, 8> signature{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
constexpr std::array<std::uint8_t, 8> headerSignature{0, 0, 0, 13, 'I', 'H', 'D', 'R'};

template<class T> std::span<T> checkedSlice(std::span<T> input, std::size_t first, std::size_t last, std::size_t line) {
    if (first > last || last > input.size()) throw compat::ArraySliceError(first, last, input.size(),
        compat::RetainedString::fromStaticLiteral("imagefmt/png.d"), line);
    return input.subspan(first, last - first);
}

template<class T> T& checkedAt(std::span<T> input, std::size_t index, std::size_t line) {
    if (index >= input.size()) throw compat::ArrayIndexError(index, input.size(),
        compat::RetainedString::fromStaticLiteral("imagefmt/png.d"), line);
    return input[index];
}

std::uint32_t loadBig32(ConstBytes bytes) noexcept {
    return (std::uint32_t(bytes[0]) << 24) + (std::uint32_t(bytes[1]) << 16) +
        (std::uint32_t(bytes[2]) << 8) + bytes[3];
}

std::array<std::uint8_t, 4> big32(std::uint32_t value) noexcept {
    return {static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
        static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)};
}

struct Crc {
    std::uint32_t value = 0xffffffffU;
    void put(ConstBytes bytes) noexcept {
        for (const auto byte : bytes) {
            value ^= byte;
            for (unsigned bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ (0xedb88320U & (0U - (value & 1U)));
        }
    }
    std::array<std::uint8_t, 4> finish() noexcept {
        const auto result = big32(value ^ 0xffffffffU);
        value = 0xffffffffU;
        return result;
    }
    static std::array<std::uint8_t, 4> of(ConstBytes bytes) noexcept {
        Crc crc;
        crc.put(bytes);
        return crc.finish();
    }
};

struct PngHeader {
    int w = 0, h = 0;
    std::uint8_t bits = 0, colorType = 0, compression = 0, filter = 0, interlace = 0;
};

std::uint8_t channels(std::uint8_t colorType) noexcept {
    switch (colorType) {
    case 0: return 1;
    case 2: return 3;
    case 3: return 3;
    case 4: return 2;
    case 6: return 4;
    default: return 0;
    }
}

Error readHeader(Reader& reader, PngHeader& header) {
    header = {};
    std::array<std::uint8_t, 33> bytes{};
    reader.block(bytes);
    if (reader.fail) return Error::stream;
    if (!std::equal(signature.begin(), signature.end(), bytes.begin()) ||
        !std::equal(headerSignature.begin(), headerSignature.end(), bytes.begin() + 8))
        return Error::data;
    const auto crc = Crc::of(ConstBytes(bytes).subspan(12, 17));
    if (!std::equal(crc.begin(), crc.end(), bytes.begin() + 29)) return Error::data;
    header.w = std::bit_cast<std::int32_t>(loadBig32(ConstBytes(bytes).subspan(16, 4)));
    header.h = std::bit_cast<std::int32_t>(loadBig32(ConstBytes(bytes).subspan(20, 4)));
    header.bits = bytes[24];
    header.colorType = bytes[25];
    header.compression = bytes[26];
    header.filter = bytes[27];
    header.interlace = bytes[28];
    return Error::none;
}

bool haveTransparency(Reader& reader) {
    std::array<std::uint8_t, 12> meta{};
    reader.block(Bytes(meta).subspan(4));
    while (!reader.fail) {
        auto length = loadBig32(ConstBytes(meta).subspan(4, 4));
        if (length > std::numeric_limits<int>::max()) return false;
        const auto type = loadBig32(ConstBytes(meta).subspan(8, 4));
        if (type == 0x74524e53U) return true;
        if (type == 0x49444154U || type == 0x49454e44U) return false;
        while (length > 0) {
            const auto bytes = reader.slice(static_cast<int>(length));
            if (bytes.empty()) return false;
            length -= static_cast<std::uint32_t>(bytes.size());
        }
        reader.block(meta);
    }
    return false;
}

struct Decoder {
    Reader& reader;
    Error& error;
    int w = 0, h = 0;
    std::uint8_t sourceBits = 0, targetBits = 0, sourceChannels = 0, targetChannels = 0;
    bool indexed = false, interlaced = false;
    std::array<std::uint8_t, 12> meta{};
    Crc crc;
    Bytes output, palette, transparency;
    z_stream* z = nullptr;
    std::uint32_t availableIdat = 0;
    ConstBytes idatWindow;
};

void setError(Decoder& decoder, Error error) noexcept {
    if (decoder.error == Error::none) decoder.error = error;
}

bool getError(Decoder& decoder, Error& error) noexcept {
    error = Error::none;
    if (decoder.error == Error::none) return false;
    error = decoder.error;
    return true;
}

Bytes allocateBytes(std::size_t count, Error& error) {
    Buffer descriptor;
    if (!descriptor.sourceAllocate(count)) { error = Error::oom; return {}; }
    return {descriptor.data(), descriptor.size()};
}

void freeBytes(Bytes bytes) {
    auto descriptor = Buffer::sourceDescriptor(bytes.data(), bytes.size());
    descriptor.sourceFree();
}

std::int32_t sourceAdd(std::int32_t left, std::int32_t right) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(left) + std::uint32_t(right));
}

std::int32_t sourceMultiply(std::int32_t left, std::int32_t right) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(left) * std::uint32_t(right));
}

std::uint8_t paeth(std::uint8_t a, std::uint8_t b, std::uint8_t c) noexcept {
    int pc = c;
    int pa = int(b) - pc;
    int pb = int(a) - pc;
    pc = pa + pb;
    if (pa < 0) pa = -pa;
    if (pb < 0) pb = -pb;
    if (pc < 0) pc = -pc;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

void reconstruct(Decoder& decoder, Bytes current, ConstBytes previous, std::size_t step) {
    const auto filter = checkedAt(current, 0, 583);
    current = checkedSlice(current, 1, current.size(), 584);
    previous = checkedSlice(previous, 1, previous.size(), 585);
    switch (filter) {
    case 0: break;
    case 1:
        for (auto i = step; i < current.size(); ++i)
            checkedAt(current, i, 591) += checkedAt(current, i - step, 591);
        break;
    case 2:
        for (std::size_t i = 0; i < current.size(); ++i)
            checkedAt(current, i, 595) += checkedAt(previous, i, 595);
        break;
    case 3:
        for (std::size_t i = 0; i < step; ++i)
            checkedAt(current, i, 599) += checkedAt(previous, i, 599) / 2;
        for (auto i = step; i < current.size(); ++i)
            checkedAt(current, i, 601) += static_cast<std::uint8_t>(
                (std::uint32_t(checkedAt(current, i - step, 602)) + checkedAt(previous, i, 602)) / 2);
        break;
    case 4:
        for (std::size_t i = 0; i < step; ++i)
            checkedAt(current, i, 606) += paeth(0, checkedAt(previous, i, 606), 0);
        for (auto i = step; i < current.size(); ++i)
            checkedAt(current, i, 608) += paeth(checkedAt(current, i - step, 608),
                checkedAt(previous, i, 608), checkedAt(previous, i - step, 608));
        break;
    default: setError(decoder, Error::unsupp); break;
    }
}

void depalette(Decoder& decoder, ConstBytes palette, ConstBytes transparency, ConstBytes source, Bytes target) {
    if (!transparency.empty()) {
        for (std::size_t s = 0, d = 0; s < source.size(); ++s, d += 4) {
            const auto index = checkedAt(source, s, 564);
            const auto paletteIndex = std::size_t(index) * 3;
            if (paletteIndex + 3 > palette.size()) { setError(decoder, Error::data); return; }
            const auto from = checkedSlice(palette, paletteIndex, paletteIndex + 3, 568);
            const auto to = checkedSlice(target, d, d + 3, 568);
            std::copy(from.begin(), from.end(), to.begin());
            checkedAt(target, d + 3, 569) = checkedAt(transparency, index, 569);
        }
    } else {
        for (std::size_t s = 0, d = 0; s < source.size(); ++s, d += 3) {
            const auto paletteIndex = std::size_t(checkedAt(source, s, 573)) * 3;
            if (paletteIndex + 3 > palette.size()) { setError(decoder, Error::data); return; }
            const auto from = checkedSlice(palette, paletteIndex, paletteIndex + 3, 576);
            const auto to = checkedSlice(target, d, d + 3, 576);
            std::copy(from.begin(), from.end(), to.begin());
        }
    }
}

void uncompressLine(Decoder& decoder, Bytes target) {
    auto& z = *decoder.z;
    z.avail_out = static_cast<uInt>(target.size());
    z.next_out = target.data();
    for (;;) {
        if (!z.avail_in) {
            if (!decoder.availableIdat) {
                decoder.reader.block(decoder.meta);
                const auto crc = decoder.crc.finish();
                if (!std::equal(crc.begin(), crc.end(), decoder.meta.begin())) {
                    setError(decoder, Error::data); return;
                }
                decoder.availableIdat = loadBig32(ConstBytes(decoder.meta).subspan(4, 4));
                if (decoder.reader.fail || !decoder.availableIdat) { setError(decoder, Error::data); return; }
                if (loadBig32(ConstBytes(decoder.meta).subspan(8, 4)) != 0x49444154U) {
                    setError(decoder, Error::lackdata); return;
                }
                decoder.crc.put(ConstBytes(decoder.meta).subspan(8, 4));
            }
            const auto requested = std::bit_cast<std::int32_t>(decoder.availableIdat);
            decoder.idatWindow = decoder.reader.slice(requested);
            if (decoder.idatWindow.empty()) { setError(decoder, Error::stream); return; }
            decoder.crc.put(decoder.idatWindow);
            decoder.availableIdat -= static_cast<std::uint32_t>(decoder.idatWindow.size());
            z.avail_in = static_cast<uInt>(decoder.idatWindow.size());
            z.next_in = const_cast<Bytef*>(decoder.idatWindow.data());
        }
        const auto result = inflate(&z, Z_NO_FLUSH);
        if (z.avail_out == 0) return;
        if (result != Z_OK) { setError(decoder, Error::zstream); return; }
    }
}

void line16FromBytes(ConstBytes source, std::span<std::uint16_t> target) {
    for (std::size_t s = 0, t = 0; s < source.size(); s += 2, ++t)
        checkedAt(target, t, 504) = static_cast<std::uint16_t>(
            (std::uint16_t(checkedAt(source, s, 504)) << 8) | checkedAt(source, s + 1, 504));
}

std::size_t verticalFlip(std::size_t row, std::size_t lastRow, bool flip) noexcept {
    return row + (lastRow - 2 * row) * flip;
}

std::size_t adam7Target(unsigned pass, std::size_t x, std::size_t y,
        std::size_t width, std::size_t lastRow, bool flip) noexcept {
    switch (pass) {
    case 0: return verticalFlip(y * 8, lastRow, flip) * width + x * 8;
    case 1: return verticalFlip(y * 8, lastRow, flip) * width + x * 8 + 4;
    case 2: return verticalFlip(y * 8 + 4, lastRow, flip) * width + x * 4;
    case 3: return verticalFlip(y * 4, lastRow, flip) * width + x * 4 + 2;
    case 4: return verticalFlip(y * 4 + 2, lastRow, flip) * width + x * 2;
    case 5: return verticalFlip(y * 2, lastRow, flip) * width + x * 2 + 1;
    default: return verticalFlip(y * 2 + 1, lastRow, flip) * width + x;
    }
}

template<class T> void sling(std::span<const T> line, std::span<T> target, unsigned pass,
        std::size_t reducedWidth, std::size_t row, int width, int lastRow, int channels, bool flip) {
    for (std::size_t x = 0, sourceIndex = 0; x < reducedWidth; ++x, sourceIndex += channels) {
        const auto targetIndex = adam7Target(pass, x, row, width, lastRow, flip) * channels;
        const auto from = checkedSlice(line, sourceIndex, sourceIndex + channels, (sizeof(T) == 1 ? 513 : 522));
        const auto to = checkedSlice(target, targetIndex, targetIndex + channels, (sizeof(T) == 1 ? 513 : 522));
        std::copy(from.begin(), from.end(), to.begin());
    }
}

std::array<std::size_t, 7> reducedWidths(int width) noexcept {
    return {std::size_t(sourceAdd(width, 7) / 8), std::size_t(sourceAdd(width, 3) / 8),
        std::size_t(sourceAdd(width, 3) / 4), std::size_t(sourceAdd(width, 1) / 4),
        std::size_t(sourceAdd(width, 1) / 2), std::size_t(width / 2), std::size_t(width)};
}

std::array<std::size_t, 7> reducedHeights(int height) noexcept {
    return {std::size_t(sourceAdd(height, 7) / 8), std::size_t(sourceAdd(height, 7) / 8),
        std::size_t(sourceAdd(height, 3) / 8), std::size_t(sourceAdd(height, 3) / 4),
        std::size_t(sourceAdd(height, 1) / 4), std::size_t(sourceAdd(height, 1) / 2), std::size_t(height / 2)};
}

std::span<std::uint16_t> words(Bytes bytes) noexcept {
    return {reinterpret_cast<std::uint16_t*>(bytes.data()), bytes.size() / 2};
}

Error readIdat8(Decoder& decoder) {
    const std::size_t step = decoder.indexed ? 1 : decoder.sourceChannels;
    const std::size_t lineSize = decoder.w * step + 1;
    const auto intermediateSize = std::size_t(sourceMultiply(
        sourceMultiply(decoder.w, decoder.sourceChannels), decoder.indexed));
    const auto reducedSize = std::size_t(sourceMultiply(
        sourceMultiply(decoder.w, decoder.targetChannels), decoder.interlaced));
    const auto workSize = 2 * lineSize + intermediateSize + reducedSize;
    const bool flip = verticalOrientationRead == -1;
    Error error = Error::none;
    auto result = allocateBytes(std::size_t(sourceMultiply(
        sourceMultiply(decoder.w, decoder.h), decoder.targetChannels)), error);
    if (error != Error::none) return error;
    auto work = allocateBytes(workSize, error);
    if (error != Error::none) {
        freeBytes(result);
        result = {};
        freeBytes(work);
        decoder.output = result;
        return error;
    }
    auto current = checkedSlice(work, 0, lineSize, 347);
    auto previous = checkedSlice(work, lineSize, 2 * lineSize, 348);
    auto intermediate = decoder.indexed
        ? checkedSlice(work, 2 * lineSize, 2 * lineSize + intermediateSize, 349) : Bytes{};
    auto reduced = decoder.interlaced ? checkedSlice(work, work.size() - reducedSize, work.size(), 350) : Bytes{};
    std::fill(work.begin(), work.end(), 0);
    setError(decoder, Error::none);
    if (!decoder.interlaced) {
        const auto targetLine = std::size_t(sourceMultiply(decoder.w, decoder.targetChannels));
        const auto stride = flip ? std::size_t(0) - targetLine : targetLine;
        auto targetIndex = flip ? (decoder.h - 1) * targetLine : 0;
        for (int y = 0; y < decoder.h; ++y) {
            uncompressLine(decoder, current);
            reconstruct(decoder, current, ConstBytes(previous), step);
            if (decoder.indexed) {
                depalette(decoder, decoder.palette, decoder.transparency,
                    checkedSlice(ConstBytes(current), 1, current.size(), 363), intermediate);
                convertPixels(ConstBytes(intermediate), checkedSlice(result, targetIndex, targetIndex + targetLine, 364),
                    decoder.sourceChannels, decoder.targetChannels);
            } else {
                convertPixels(checkedSlice(ConstBytes(current), 1, current.size(), 372),
                    checkedSlice(result, targetIndex, targetIndex + targetLine, 372),
                    decoder.sourceChannels, decoder.targetChannels);
            }
            targetIndex += stride;
            std::swap(current, previous);
        }
    } else {
        const auto widths = reducedWidths(decoder.w);
        const auto heights = reducedHeights(decoder.h);
        const int lastRow = decoder.h - 1;
        for (unsigned pass = 0; pass < 7; ++pass) {
            const auto sourceLine = widths[pass] * decoder.sourceChannels;
            const auto targetLine = widths[pass] * decoder.targetChannels;
            auto passCurrent = checkedSlice(current, 0, widths[pass] * step + 1, 386);
            auto passPrevious = checkedSlice(previous, 0, widths[pass] * step + 1, 387);
            std::fill(passPrevious.begin(), passPrevious.end(), 0);
            for (std::size_t row = 0; row < heights[pass]; ++row) {
                uncompressLine(decoder, passCurrent);
                reconstruct(decoder, passCurrent, ConstBytes(passPrevious), step);
                if (decoder.indexed) {
                    depalette(decoder, decoder.palette, decoder.transparency,
                        checkedSlice(ConstBytes(passCurrent), 1, passCurrent.size(), 394), intermediate);
                    convertPixels(checkedSlice(ConstBytes(intermediate), 0, sourceLine, 395),
                        checkedSlice(reduced, 0, targetLine, 395), decoder.sourceChannels, decoder.targetChannels);
                } else {
                    convertPixels(checkedSlice(ConstBytes(passCurrent), 1, 1 + sourceLine, 403),
                        checkedSlice(reduced, 0, targetLine, 403), decoder.sourceChannels, decoder.targetChannels);
                }
                sling(ConstBytes(reduced), result, pass, widths[pass], row,
                    decoder.w, lastRow, decoder.targetChannels, flip);
                std::swap(passCurrent, passPrevious);
            }
        }
    }
    if (getError(decoder, error)) { freeBytes(result); result = {}; }
    freeBytes(work);
    decoder.output = result;
    return error;
}

Error readIdat16(Decoder& decoder) {
    const std::size_t step = decoder.sourceChannels * 2;
    const auto lineSize = decoder.w * step + 1;
    const auto intermediateSize = std::size_t(sourceMultiply(
        sourceMultiply(decoder.w, decoder.sourceChannels), 2));
    const auto reducedSize = std::size_t(sourceMultiply(sourceMultiply(sourceMultiply(
        sourceMultiply(decoder.w, decoder.h), decoder.targetChannels), 2), decoder.interlaced));
    const auto workSize = 2 * lineSize + intermediateSize + reducedSize;
    const bool flip = verticalOrientationRead == -1;
    Error error = Error::none;
    const auto count = std::size_t(sourceMultiply(sourceMultiply(decoder.w, decoder.h), decoder.targetChannels));
    auto resultBytes = allocateBytes(count * sizeof(std::uint16_t), error);
    if (error != Error::none) return error;
    auto result = words(resultBytes);
    auto work = allocateBytes(workSize, error);
    if (error != Error::none) {
        freeBytes(resultBytes);
        resultBytes = {};
        freeBytes(work);
        decoder.output = resultBytes;
        return error;
    }
    auto current = checkedSlice(work, 0, lineSize, 445);
    auto previous = checkedSlice(work, lineSize, 2 * lineSize, 446);
    auto intermediate = words(checkedSlice(work, 2 * lineSize, 2 * lineSize + intermediateSize, 447));
    auto reduced = decoder.interlaced
        ? words(checkedSlice(work, work.size() - reducedSize, work.size(), 448)) : std::span<std::uint16_t>{};
    std::fill(work.begin(), work.end(), 0);
    setError(decoder, Error::none);
    if (!decoder.interlaced) {
        const auto targetLine = std::size_t(sourceMultiply(decoder.w, decoder.targetChannels));
        const auto stride = flip ? std::size_t(0) - targetLine : targetLine;
        auto targetIndex = flip ? (decoder.h - 1) * targetLine : 0;
        for (int row = 0; row < decoder.h; ++row) {
            uncompressLine(decoder, current);
            reconstruct(decoder, current, ConstBytes(previous), step);
            line16FromBytes(checkedSlice(ConstBytes(current), 1, current.size(), 460), intermediate);
            convertPixels(std::span<const std::uint16_t>(intermediate),
                checkedSlice(result, targetIndex, targetIndex + targetLine, 461), decoder.sourceChannels, decoder.targetChannels);
            targetIndex += stride;
            std::swap(current, previous);
        }
    } else {
        const auto widths = reducedWidths(decoder.w);
        const auto heights = reducedHeights(decoder.h);
        const int lastRow = decoder.h - 1;
        for (unsigned pass = 0; pass < 7; ++pass) {
            const auto sourceLine = widths[pass] * decoder.sourceChannels;
            const auto targetLine = widths[pass] * decoder.targetChannels;
            auto passCurrent = checkedSlice(current, 0, widths[pass] * step + 1, 474);
            auto passPrevious = checkedSlice(previous, 0, widths[pass] * step + 1, 475);
            std::fill(passPrevious.begin(), passPrevious.end(), 0);
            for (std::size_t row = 0; row < heights[pass]; ++row) {
                uncompressLine(decoder, passCurrent);
                reconstruct(decoder, passCurrent, ConstBytes(passPrevious), step);
                line16FromBytes(checkedSlice(ConstBytes(passCurrent), 1, passCurrent.size(), 481),
                    checkedSlice(intermediate, 0, sourceLine, 481));
                convertPixels(checkedSlice(std::span<const std::uint16_t>(intermediate), 0, sourceLine, 482),
                    checkedSlice(reduced, 0, targetLine, 482), decoder.sourceChannels, decoder.targetChannels);
                sling(std::span<const std::uint16_t>(reduced), result, pass, widths[pass], row,
                    decoder.w, lastRow, decoder.targetChannels, flip);
                std::swap(passCurrent, passPrevious);
            }
        }
    }
    if (getError(decoder, error)) { freeBytes(resultBytes); resultBytes = {}; }
    freeBytes(work);
    decoder.output = resultBytes;
    return error;
}

Error readIdatChunks(Decoder& decoder, std::uint32_t length) {
    z_stream stream{};
    if (inflateInit(&stream) != Z_OK) return Error::zinit;
    decoder.z = &stream;
    decoder.availableIdat = length;
    Error error = Error::none;
    switch (decoder.sourceBits) {
    case 8: error = readIdat8(decoder); break;
    case 16: error = readIdat16(decoder); break;
    default: error = Error::unsupp; break;
    }
    inflateEnd(&stream);
    return error;
}

bool checkChunkCrc(Decoder& decoder) noexcept {
    const auto crc = decoder.crc.finish();
    return std::equal(crc.begin(), crc.end(), decoder.meta.begin());
}

Error readChunks(Decoder& decoder) {
    enum Stage { headerDone, paletteDone, dataDone, endDone };
    auto stage = headerDone;
    decoder.reader.block(Bytes(decoder.meta).subspan(4));
    while (stage != endDone && !decoder.reader.fail) {
        auto length = loadBig32(ConstBytes(decoder.meta).subspan(4, 4));
        if (length > std::numeric_limits<int>::max()) return Error::data;
        decoder.crc.put(ConstBytes(decoder.meta).subspan(8, 4));
        switch (loadBig32(ConstBytes(decoder.meta).subspan(8, 4))) {
        case 0x49444154U: {
            if (stage != headerDone && (stage != paletteDone || !decoder.indexed)) return Error::data;
            decoder.sourceChannels = decoder.indexed && !decoder.transparency.empty() ? 4 : decoder.sourceChannels;
            decoder.targetChannels = decoder.targetChannels ? decoder.targetChannels : decoder.sourceChannels;
            if (std::uint64_t(decoder.w) * decoder.h * decoder.targetChannels > maximumImageSize)
                return Error::bigimg;
            const auto error = readIdatChunks(decoder, length);
            if (error != Error::none) return error;
            decoder.reader.block(decoder.meta);
            if (!checkChunkCrc(decoder)) return Error::data;
            stage = dataDone;
            break;
        }
        case 0x504c5445U: {
            if (stage != headerDone) return Error::data;
            const auto entries = length / 3;
            if (entries * 3 != length || entries > 256) return Error::data;
            Error error = Error::none;
            decoder.palette = allocateBytes(length, error);
            if (error != Error::none) return error;
            decoder.reader.block(decoder.palette);
            decoder.crc.put(decoder.palette);
            decoder.reader.block(decoder.meta);
            if (!checkChunkCrc(decoder)) return Error::data;
            stage = paletteDone;
            break;
        }
        case 0x74524e53U: {
            if (!(stage == headerDone || (stage == paletteDone && decoder.indexed))) return Error::data;
            if ((decoder.indexed && length * 3 > decoder.palette.size()) || length > 256) return Error::data;
            if (!decoder.indexed) return Error::unsupp;
            Error error = Error::none;
            decoder.transparency = allocateBytes(256, error);
            if (error != Error::none) return error;
            decoder.reader.block(checkedSlice(decoder.transparency, 0, length, 262));
            const auto tail = checkedSlice(decoder.transparency, length, decoder.transparency.size(), 263);
            std::fill(tail.begin(), tail.end(), 255);
            decoder.reader.block(decoder.meta);
            if (decoder.reader.fail) return Error::stream;
            decoder.crc.put(checkedSlice(ConstBytes(decoder.transparency), 0, length, 266));
            if (!checkChunkCrc(decoder)) return Error::data;
            break;
        }
        case 0x49454e44U: {
            if (stage != dataDone) return Error::data;
            constexpr std::array<std::uint8_t, 4> endCrc{0xae, 0x42, 0x60, 0x82};
            decoder.reader.block(Bytes(decoder.meta).first(4));
            if (length != 0 || !std::equal(endCrc.begin(), endCrc.end(), decoder.meta.begin())) return Error::data;
            stage = endDone;
            break;
        }
        case 0x49484452U: return Error::data;
        default:
            while (length > 0) {
                const auto bytes = decoder.reader.slice(static_cast<int>(length));
                if (bytes.empty()) return Error::data;
                length -= static_cast<std::uint32_t>(bytes.size());
                decoder.crc.put(bytes);
            }
            decoder.reader.block(decoder.meta);
            if (!checkChunkCrc(decoder)) return Error::data;
            break;
        }
    }
    return decoder.reader.fail ? Error::stream : Error::none;
}

Error decodePng(Decoder& decoder, int requestedChannels, int requestedBits) {
    if (std::uint32_t(requestedChannels) > 4) return Error::arg;
    if (requestedBits != 0 && requestedBits != 8 && requestedBits != 16) return Error::unsupp;
    PngHeader header;
    const auto headerError = readHeader(decoder.reader, header);
    if (headerError != Error::none) return headerError;
    if (header.w < 1 || header.h < 1 || std::uint64_t(header.w) * header.h > std::numeric_limits<int>::max())
        return Error::dim;
    if (header.bits != 8 && header.bits != 16) return Error::unsupp;
    if (header.colorType != 0 && header.colorType != 2 && header.colorType != 3 &&
            header.colorType != 4 && header.colorType != 6) return Error::unsupp;
    if (header.colorType == 3 && header.bits != 8) return Error::unsupp;
    if (header.compression != 0 || header.filter != 0 || header.interlace > 1) return Error::unsupp;
    decoder.w = header.w;
    decoder.h = header.h;
    decoder.sourceBits = header.bits;
    decoder.targetBits = static_cast<std::uint8_t>(requestedBits ? requestedBits : header.bits);
    decoder.sourceChannels = channels(header.colorType);
    decoder.targetChannels = static_cast<std::uint8_t>(requestedChannels);
    decoder.indexed = header.colorType == 3;
    decoder.interlaced = header.interlace == 1;
    const auto error = readChunks(decoder);
    freeBytes(decoder.palette);
    freeBytes(decoder.transparency);
    return error;
}

Error finishImage(Decoder& decoder, Image& image, Buffer output) {
    try {
        switch (32 * decoder.sourceBits + decoder.targetBits) {
        case 32 * 8 + 8:
        case 32 * 16 + 16:
            break;
        case 32 * 8 + 16:
        case 32 * 16 + 8:
            convertBits(output, decoder.sourceBits, decoder.targetBits);
            break;
        default:
            compat::ngAssertUnreachable(nullptr, "imagefmt/png.d", 187);
        }
    } catch (...) {
        // The source has no output cleanup on an escaping conversion/Error path.
        output.detach();
        throw;
    }
    // A reader callback may have replaced the out descriptor since entry reset.
    image.buffer.detach();
    image.buffer = std::move(output);
    if (!image.buffer.data()) return Error::oom;
    image.w = decoder.w;
    image.h = decoder.h;
    image.c = decoder.targetChannels;
    image.bpc = decoder.targetBits;
    image.cinfile = decoder.sourceChannels;
    return Error::none;
}
} // namespace

bool detectPng(Reader& reader) {
    std::array<std::uint8_t, 8> bytes{};
    reader.block(bytes);
    reader.reset();
    return !reader.fail && bytes == signature;
}

ImageInfo readPngInfo(Reader& reader) {
    PngHeader header;
    ImageInfo info;
    info.e = readHeader(reader, header);
    info.w = header.w;
    info.h = header.h;
    info.c = channels(header.colorType);
    if (header.colorType == 3 && haveTransparency(reader)) info.c = 4;
    else if (info.c == 0 && info.e == Error::none) info.e = Error::data;
    return info;
}

Error readPng(Reader& reader, Image& image, int requestedChannels, int requestedBits) {
    // Source out IFImage clears its descriptor without freeing the previous allocation.
    image.buffer.detach();
    image = Image{};
    Decoder decoder{reader, pngErrorState()};
    const auto error = decodePng(decoder, requestedChannels, requestedBits);
    if (error != Error::none) return error;
    auto output = Buffer::sourceDescriptor(decoder.output.data(), decoder.output.size());
    return finishImage(decoder, image, std::move(output));
}
} // namespace nicxlive::core::imagefmt::detail
