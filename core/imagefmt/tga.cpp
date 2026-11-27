// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of the imagefmt 2.1.2 TGA writer closure. See LICENSE.imagefmt.
#include "writer.hpp"
#include <array>
#include <cstring>
#include <bit>
#include <compat/array_range_error.hpp>

namespace nicxlive::core::imagefmt::detail {
namespace {
template<class T> std::span<T> tgaSlice(std::span<T> input,std::size_t lower,std::size_t upper,std::size_t line) {
    if (lower>upper || upper>input.size()) throw compat::ArraySliceError(lower,upper,input.size(),
        compat::RetainedString::fromStaticLiteral("imagefmt/tga.d"),line);
    return input.subspan(lower,upper-lower);
}
std::size_t rleCompress(std::span<const std::uint8_t> line, std::span<std::uint8_t> compressed,
    std::size_t width, int bytesPerPixel) {
    const int rleLimit = bytesPerPixel > 1 ? 2 : 3;
    std::size_t rawLength = 0, rawStart = 0, output = 0, pixelsLeft = width;
    for (std::size_t i = bytesPerPixel; pixelsLeft; i += bytesPerPixel) {
        std::size_t runLength = 1;
        const auto* pixel = line.data() + i - bytesPerPixel;
        while (i < line.size() && std::memcmp(line.data() + i, pixel, bytesPerPixel) == 0 && runLength < 128) {
            ++runLength;
            i += bytesPerPixel;
        }
        pixelsLeft -= runLength;
        if (runLength < static_cast<std::size_t>(rleLimit)) {
            rawLength += runLength;
            if (rawLength >= 128) {
                const auto count = static_cast<std::size_t>(128 * bytesPerPixel);
                compressed[output++] = 0x7f;
                std::memcpy(compressed.data() + output, line.data() + rawStart, count);
                output += count; rawStart += count; rawLength -= 128;
            }
        } else {
            if (rawLength) {
                const auto count = rawLength * bytesPerPixel;
                compressed[output++] = static_cast<std::uint8_t>(rawLength - 1);
                std::memcpy(compressed.data() + output, line.data() + rawStart, count);
                output += count; rawLength = 0;
            }
            compressed[output++] = static_cast<std::uint8_t>(0x80 | (runLength - 1));
            std::memcpy(compressed.data() + output, pixel, bytesPerPixel);
            output += bytesPerPixel; rawStart = i;
        }
    }
    if (rawLength) {
        const auto count = rawLength * bytesPerPixel;
        compressed[output++] = static_cast<std::uint8_t>(rawLength - 1);
        std::memcpy(compressed.data() + output, line.data() + rawStart, count);
        output += count;
    }
    return output;
}

Error writeTgaData(Writer& writer, int width, int height, std::span<const std::uint8_t> data,
    int sourceChannels, int targetChannels) {
    const int targetFormat = targetChannels == 3 ? 5 : targetChannels == 4 ? 6 : targetChannels;
    const auto sourceLineSize = static_cast<std::size_t>(width) * sourceChannels;
    const auto targetLineSize = static_cast<std::size_t>(width) * targetChannels;
    const auto maximumPackets = (targetLineSize + 127) / 128;
    Buffer work;
    const auto error=work.sourceAllocate(2*targetLineSize+maximumPackets) ? Error::none : Error::oom;
    // The source does not branch on allocation failure before these actual slices.
    const auto storage=std::span<std::uint8_t>(work.data(),work.size());
    const auto line=tgaSlice(storage,0,targetLineSize,272);
    const auto compressed=tgaSlice(storage,targetLineSize,2*targetLineSize+maximumPackets,273);
    const auto sourceSize=std::uint32_t(height)*static_cast<std::uint32_t>(sourceLineSize);
    const auto stride=(0U-static_cast<std::uint32_t>(sourceLineSize))*std::uint32_t(verticalOrientationWrite);
    auto sourceIndex=std::uint32_t(height-1)*static_cast<std::uint32_t>(sourceLineSize)*
        std::uint32_t(verticalOrientationWrite==1);
    for (;sourceIndex<sourceSize;sourceIndex+=stride) {
        const auto upper=sourceIndex+static_cast<std::uint32_t>(sourceLineSize);
        convertLine(tgaSlice(data,sourceIndex,upper,276),line,sourceChannels,targetFormat);
        const auto length=rleCompress(line,compressed,static_cast<std::size_t>(width),targetChannels);
        writer.block(tgaSlice(compressed,0,length,278));
    }
    work.sourceFree();
    return writer.failed ? Error::stream : error;
}
} // namespace

Error writeTga(Writer& writer, int width, int height, std::span<const std::uint8_t> data,
    int requestedChannels) {
    if (width < 1 || height < 1 || width > 65535 || height > 65535) return Error::dim;
    const int sourceChannels=std::bit_cast<int>(static_cast<std::uint32_t>(data.size()/width/height));
    const int sourceProduct=std::bit_cast<int>(std::uint32_t(sourceChannels)*std::uint32_t(width)*std::uint32_t(height));
    if (sourceChannels<1 || sourceChannels>4 || static_cast<std::size_t>(sourceProduct)!=data.size())
        return Error::dim;
    if (static_cast<unsigned>(requestedChannels) > 4) return Error::unsupp;
    const int targetChannels = requestedChannels ? requestedChannels : static_cast<int>(sourceChannels);
    const bool hasAlpha = targetChannels == 2 || targetChannels == 4;
    const std::array<std::uint8_t, 16> zeros{};
    writer.byte(0); writer.byte(0); writer.byte(targetChannels >= 3 ? 10 : 11);
    writer.block(std::span(zeros).first(9));
    writer.little16(static_cast<std::uint16_t>(width));
    writer.little16(static_cast<std::uint16_t>(height));
    writer.byte(static_cast<std::uint8_t>(targetChannels * 8));
    writer.byte(hasAlpha ? 8 : 0);
    if (writer.failed) return Error::stream;
    const auto error = writeTgaData(writer, width, height, data, static_cast<int>(sourceChannels), targetChannels);
    writer.block(std::span(zeros).first(8));
    constexpr std::uint8_t footer[]{'T','R','U','E','V','I','S','I','O','N','-','X','F','I','L','E','.',0};
    writer.block(footer);
    return writer.failed ? Error::stream : error;
}
} // namespace nicxlive::core::imagefmt::detail
