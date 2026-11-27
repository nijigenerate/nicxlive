// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of imagefmt 2.1.2 tga.d reader. See LICENSE.imagefmt.
#include "reader.hpp"
#include <compat/array_range_error.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>

namespace nicxlive::core::imagefmt::detail {
namespace {
struct TgaHeader {
    int w{}, h{};
    std::uint8_t idlen{}, paletteType{}, dataType{}, bits{}, flags{};
};
int signed32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
int mul(int a, int b) { return signed32(std::uint32_t(a) * std::uint32_t(b)); }
int add(int a, int b) { return signed32(std::uint32_t(a) + std::uint32_t(b)); }
std::span<std::uint8_t> part(Buffer& buffer, std::size_t first, std::size_t count, std::size_t line) {
    if (first > first+count || first+count > buffer.size()) throw compat::ArraySliceError(first,first+count,buffer.size(),
        compat::RetainedString::fromStaticLiteral("imagefmt/tga.d"),line);
    return {buffer.data() + first, count};
}
Error header(Reader& r, TgaHeader& h) {
    h = {};
    h.idlen = r.byte(); h.paletteType = r.byte(); h.dataType = r.byte();
    const auto paletteStart = r.little16(), paletteLength = r.little16();
    const auto paletteBits = r.byte();
    r.skip(4); h.w = r.little16(); h.h = r.little16(); h.bits = r.byte(); h.flags = r.byte();
    if (h.w < 1 || h.h < 1 || h.paletteType > 1 ||
        (h.paletteType == 0 && (paletteStart || paletteLength || paletteBits)) ||
        (h.dataType > 3 && h.dataType < 9) || h.dataType > 11) return Error::data;
    // No reader.fail check occurs in the original header routine.
    return Error::none;
}
}
bool detectTga(Reader& r) {
    TgaHeader h; const bool result = header(r, h) == Error::none;
    r.reset(); return result;
}
ImageInfo readTgaInfo(Reader& r) {
    TgaHeader h; ImageInfo info;
    info.e = header(r, h);
    if (info.e != Error::none) return info;
    info.w = h.w; info.h = h.h;
    if ((h.dataType == 2 || h.dataType == 3 || h.dataType == 10 || h.dataType == 11) && h.bits % 8 == 0)
        info.c = h.bits / 8;
    info.e = info.c ? Error::none : Error::unsupp;
    return info;
}
Error readTga(Reader& r, Image& image, int requestedChannels, int requestedBits) {
    image = {};
    if (std::uint32_t(requestedChannels) > 4) return Error::arg;
    const auto bits = static_cast<std::uint8_t>(requestedBits ? requestedBits : 8);
    if (bits != 8 && bits != 16) return Error::unsupp;
    TgaHeader h;
    if (const auto e = header(r, h); e != Error::none) return e;
    if (h.w < 1 || h.h < 1) return Error::dim;
    if (h.flags & 0xc0) return Error::unsupp;
    if (h.flags & 0x10) return Error::unsupp;
    const auto attributes = h.flags & 0xf;
    if (attributes != 0 && attributes != 8) return Error::unsupp;
    if (h.paletteType) return Error::unsupp;
    switch (h.dataType) {
    case 2: case 10:
        if (h.bits != 24 && h.bits != 32) return Error::unsupp;
        break;
    case 3: case 11:
        if (h.bits != 8 && !(h.bits == 16 && attributes == 8)) return Error::unsupp;
        break;
    default: return Error::unsupp;
    }
    const bool top = (h.flags & 0x20) > 0, rle = h.dataType >= 9 && h.dataType <= 11;
    const int sourceChannels = h.bits / 8, channels = requestedChannels ? requestedChannels : sourceChannels;
    const int sourceSize = mul(h.w, sourceChannels), targetSize = mul(h.w, channels);
    const bool flip = top != (verticalOrientationRead == 1);
    const int stride = flip ? signed32(0u - std::uint32_t(targetSize)) : targetSize;
    int target = flip ? mul(h.h - 1, targetSize) : 0;
    if (std::uint64_t(h.w) * h.h * channels > maximumImageSize) return Error::bigimg;
    const int format = sourceChannels == 3 ? 5 : sourceChannels == 4 ? 6 : sourceChannels;
    Error error = Error::none;
    auto result = Buffer::sourceDescriptor(nullptr, 0);
    auto line = Buffer::sourceDescriptor(nullptr, 0);
    if (!result.sourceAllocate(static_cast<std::size_t>(mul(mul(h.w, h.h), channels)))) error = Error::oom;
    if (!line.sourceAllocate(static_cast<std::size_t>(sourceSize))) error = Error::oom;
    if (h.idlen) r.skip(h.idlen);
    if (error != Error::none || r.fail) {
        result.sourceFree(); line.sourceFree();
        return error != Error::none ? error : Error::stream;
    }
    if (!rle) {
        for (int row = 0; row < h.h; ++row) {
            r.block({line.data(), line.size()});
            convertPixels(std::span<const std::uint8_t>(line.data(), line.size()),
                part(result, static_cast<std::size_t>(target), targetSize,157), format, channels);
            target = add(target, stride);
        }
    } else {
        std::array<std::uint8_t, 4> pixel{};
        int packetLength = 0; bool run = false;
        for (int row = 0; row < h.h; ++row) {
            int wanted = sourceSize;
            do {
                if (packetLength == 0) {
                    const auto packet = r.byte(); run = (packet & 0x80) != 0;
                    packetLength = ((packet & 0x7f) + 1) * sourceChannels;
                }
                const int gotten = sourceSize - wanted, count = std::min(wanted, packetLength);
                if (run) {
                    // Preserve the source read on each row fragment, including cross-row packets.
                    r.block(std::span<std::uint8_t>(pixel).first(sourceChannels));
                    for (int p = gotten; p < gotten + count; p += sourceChannels)
                        std::copy_n(pixel.begin(), sourceChannels, part(line, p, sourceChannels,178).begin());
                } else r.block(part(line, gotten, count,180));
                wanted -= count; packetLength -= count;
            } while (wanted);
            convertPixels(std::span<const std::uint8_t>(line.data(), line.size()),
                part(result, static_cast<std::size_t>(target), targetSize,185), format, channels);
            target = add(target, stride);
        }
    }
    line.sourceFree();
    if (r.fail) { result.sourceFree(); return Error::stream; }
    image.w = h.w; image.h = h.h; image.c = static_cast<std::uint8_t>(channels);
    image.cinfile = static_cast<std::uint8_t>(sourceChannels); image.bpc = bits;
    if (bits == 16 && !convertBits(result, 8, 16)) return Error::oom;
    image.buffer = std::move(result);
    return error;
}
}
