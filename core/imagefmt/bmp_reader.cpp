// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of imagefmt 2.1.2 bmp.d reader. See LICENSE.imagefmt.
#include "reader.hpp"
#include <compat/array_range_error.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <limits>

namespace nicxlive::core::imagefmt::detail {
namespace {
struct BmpHeader {
    int w{}, h{}, planes{}, bitspp{};
    std::uint32_t dataoff{}, alphamask{}, rmask{}, gmask{}, bmask{}, compress{}, palettelen{}, dibsize{};
    std::uint8_t dibv{};
};
int signed32(std::uint32_t x) { return std::bit_cast<std::int32_t>(x); }
int add(int a, int b) { return signed32(std::uint32_t(a) + std::uint32_t(b)); }
int mul(int a, int b) { return signed32(std::uint32_t(a) * std::uint32_t(b)); }
int absolute(int x) { return x >= 0 ? x : signed32(0u - std::uint32_t(x)); }
std::span<std::uint8_t> part(Buffer& b, std::size_t first, std::size_t size, std::size_t line) {
    if (first > first+size || first+size > b.size()) throw compat::ArraySliceError(first,first+size,b.size(),
        compat::RetainedString::fromStaticLiteral("imagefmt/bmp.d"),line);
    return {b.data() + first, size};
}
template<class C> decltype(auto) bmpAt(C& values,std::size_t index,std::size_t line) {
    if (index>=values.size()) throw compat::ArrayIndexError(index,values.size(),
        compat::RetainedString::fromStaticLiteral("imagefmt/bmp.d"),line);
    return values[index];
}

Error header(Reader& r, BmpHeader& h) {
    h = {};
    const auto b = r.byte(), m = r.byte();
    r.skip(8); h.dataoff = r.little32(); h.dibsize = r.little32();
    if (r.fail) return Error::stream;
    if (b != 'B' || m != 'M') return Error::data;
    switch (h.dibsize) {
    case 12: h.dibv = 0; break; case 40: h.dibv = 1; break;
    case 52: h.dibv = 2; break; case 56: h.dibv = 3; break;
    case 108: h.dibv = 4; break; case 124: h.dibv = 5; break;
    default: return Error::unsupp;
    }
    if (h.dibsize <= 12) { h.w = r.little16(); h.h = r.little16(); }
    else { h.w = signed32(r.little32()); h.h = signed32(r.little32()); }
    h.planes = r.little16(); h.bitspp = r.little16();
    if (h.dibsize >= 40) {
        h.compress = r.little32(); r.skip(12); h.palettelen = r.little32(); r.skip(4);
    }
    if (h.dibsize >= 52) { h.rmask = r.little32(); h.gmask = r.little32(); h.bmask = r.little32(); }
    if (h.dibsize >= 56) h.alphamask = r.little32();
    if (h.dibsize >= 108) r.skip(4 + 36 + 12);
    if (h.dibsize >= 124) r.skip(8);
    if (r.fail) return Error::stream;
    if (h.w == std::numeric_limits<int>::min() || h.h == std::numeric_limits<int>::min()) return Error::data;
    return Error::none;
}
bool maskIndex(std::uint32_t mask, int& index) {
    index = 0;
    switch (mask) {
    case 0xff000000: index = 3; return false;
    case 0x00ff0000: index = 2; return false;
    case 0x0000ff00: index = 1; return false;
    case 0x000000ff: index = 0; return false;
    default: return true;
    }
}
}
bool detectBmp(Reader& r) {
    bool result = false;
    if (r.byte() == 'B' && r.byte() == 'M') {
        r.skip(12); const auto size = r.little32();
        result = (size == 12 || size == 40 || size == 52 || size == 56 || size == 108 || size == 124) && !r.fail;
    }
    r.reset(); return result;
}
ImageInfo readBmpInfo(Reader& r) {
    BmpHeader h; ImageInfo info;
    info.e = header(r, h);
    if (info.e != Error::none) return info;
    info.w = absolute(h.w); info.h = absolute(h.h);
    info.c = h.dibv >= 3 && h.alphamask != 0 && h.bitspp == 32 ? 4 : 3;
    return info;
}
Error readBmp(Reader& r, Image& image, int requestedChannels, int requestedBits) {
    image = {};
    if (std::uint32_t(requestedChannels) > 4) return Error::arg;
    const auto bits = static_cast<std::uint8_t>(requestedBits ? requestedBits : 8);
    if (bits != 8 && bits != 16) return Error::unsupp;
    BmpHeader h;
    if (auto e = header(r, h); e != Error::none) return e;
    if (h.w < 1 || h.h == 0) return Error::dim;
    if (h.dataoff < 14u + h.dibsize || h.dataoff > 0xffffff) return Error::data;
    if (h.planes != 1) return Error::unsupp;
    int bytes = 1, paletteLength = 256, paletteBytes = 3;
    bool paletted = true, masked = false;
    if (h.dibv >= 1) {
        if (h.palettelen > 256) return Error::dim;
        if (h.bitspp <= 8 && (h.palettelen == 0 || h.compress != 0)) return Error::unsupp;
        if (h.compress != 0 && h.compress != 3) return Error::unsupp;
        switch (h.bitspp) {
        case 8: bytes = 1; paletted = true; break;
        case 24: bytes = 3; paletted = false; break;
        case 32: bytes = 4; paletted = false; break;
        default: return Error::unsupp;
        }
        paletteLength = static_cast<int>(h.palettelen); masked = h.compress == 3; paletteBytes = 4;
    }
    int red = 2, green = 1, blue = 0;
    if (masked) {
        if (h.dibv < 2) return Error::data;
        if (maskIndex(h.rmask, red) || maskIndex(h.gmask, green) || maskIndex(h.bmask, blue)) return Error::unsupp;
    }
    bool alphaMasked = false; int alpha = 0;
    if (bytes == 4 && h.dibv >= 3 && h.alphamask != 0) {
        alphaMasked = true;
        if (maskIndex(h.alphamask, alpha)) return Error::unsupp;
    }
    const int channels = requestedChannels > 0 ? requestedChannels : alphaMasked ? 4 : 3;
    const int sourceFormat = paletted && paletteBytes == 3 ? 5 : 6;
    const int sourceSize = mul(h.w, bytes), targetSize = mul(h.w, channels);
    const int pad = 3 - (add(sourceSize, -1) % 4), height = absolute(h.h);
    const bool flip = (h.h < 0) != (verticalOrientationRead == 1);
    const int stride = flip ? signed32(0u - std::uint32_t(targetSize)) : targetSize;
    const int start = flip ? mul(height - 1, targetSize) : 0;
    const auto limit = std::uint32_t(mul(height, targetSize));
    if (std::uint64_t(h.w) * height * channels > maximumImageSize) return Error::bigimg;
    Error error = Error::none;
    auto result = Buffer::sourceDescriptor(nullptr, 0);
    auto work = Buffer::sourceDescriptor(nullptr, 0);
    auto palette = Buffer::sourceDescriptor(nullptr, 0);
    if (!result.sourceAllocate(static_cast<std::size_t>(mul(mul(h.w, height), channels)))) return Error::oom;
    const auto finish = [&] {
        work.sourceFree(); palette.sourceFree();
        image.w = h.w; image.h = height; image.c = static_cast<std::uint8_t>(channels);
        image.cinfile = h.dibv >= 3 && h.alphamask != 0 && h.bitspp == 32 ? 4 : 3;
        image.bpc = bits;
        if (bits == 16 && result.data() && !convertBits(result, 8, 16) && error == Error::none) error = Error::oom;
        image.buffer = std::move(result); return error;
    };
    const auto failure = [&] { result.sourceFree(); return finish(); };
    if (!work.sourceAllocate(static_cast<std::size_t>(add(add(sourceSize, pad), mul(h.w, 4))))) {
        error = Error::oom; return failure();
    }
    auto line = part(work, 0, static_cast<std::size_t>(add(sourceSize, pad)),220);
    auto expanded = part(work, line.size(), static_cast<std::size_t>(mul(h.w, 4)),221);
    if (paletted) {
        if (!palette.sourceAllocate(static_cast<std::size_t>(paletteLength * paletteBytes))) {
            error = Error::oom; return failure();
        }
        r.block({palette.data(), palette.size()});
    }
    r.skipTo(static_cast<int>(h.dataoff));
    if (r.fail) { error = Error::stream; return failure(); }
    for (int target = start; std::uint32_t(target) < limit; target = add(target, stride)) {
        r.block(line);
        if (!paletted) {
            for (std::size_t source = 0, dest = 0; source < static_cast<std::size_t>(sourceSize); source += bytes, dest += 4) {
                bmpAt(expanded,dest,241)=bmpAt(line,source+blue,241);
                bmpAt(expanded,dest+1,242)=bmpAt(line,source+green,242);
                bmpAt(expanded,dest+2,243)=bmpAt(line,source+red,243);
                bmpAt(expanded,dest+3,244)=alphaMasked ? bmpAt(line,source+alpha,244) : 255;
            }
        } else {
            std::size_t dest = 0;
            for (int source = 0; source < sourceSize; ++source) {
                const auto index = line[source];
                // The original comparison is strictly greater, not greater-or-equal.
                if (index > paletteLength) { error = Error::data; return failure(); }
                const auto color=std::span<std::uint8_t>(palette.data(),palette.size());
                const auto offset=std::size_t(index)*paletteBytes;
                bmpAt(expanded,dest,260)=bmpAt(color,offset,260);
                bmpAt(expanded,dest+1,261)=bmpAt(color,offset+1,261);
                bmpAt(expanded,dest+2,262)=bmpAt(color,offset+2,262);
                if (paletteBytes==4) bmpAt(expanded,dest+3,264)=255;
                dest += paletteBytes;
            }
        }
        convertPixels(std::span<const std::uint8_t>(expanded), part(result, static_cast<std::size_t>(target), targetSize,paletted ? 267 : 247), sourceFormat, channels);
    }
    // The original failure path deliberately leaves error unchanged here.
    if (r.fail) return failure();
    return finish();
}
}
