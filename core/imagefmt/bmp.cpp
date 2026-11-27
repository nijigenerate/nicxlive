// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of imagefmt 2.1.2 write_bmp. See LICENSE.imagefmt.
#include "writer.hpp"
#include <array>
#include <cstring>
#include <bit>
#include <compat/array_range_error.hpp>

namespace nicxlive::core::imagefmt::detail {
Error writeBmp(Writer& writer, int width, int height, std::span<const std::uint8_t> data,
    int requestedChannels) {
    if (width < 1 || height < 1 || width > 0x7fff || height > 0x7fff) return Error::dim;
    const int sourceChannels=std::bit_cast<int>(static_cast<std::uint32_t>(data.size()/width/height));
    const int sourceProduct=std::bit_cast<int>(std::uint32_t(sourceChannels)*std::uint32_t(width)*std::uint32_t(height));
    if (sourceChannels<1 || sourceChannels>4 || static_cast<std::size_t>(sourceProduct)!=data.size())
        return Error::dim;
    if (requestedChannels != 0 && requestedChannels != 3 && requestedChannels != 4) return Error::unsupp;
    const int targetChannels = requestedChannels ? requestedChannels :
        (sourceChannels == 1 || sourceChannels == 3 ? 3 : 4);
    const std::size_t lineSize = static_cast<std::size_t>(width) * targetChannels;
    const std::size_t padding = 3 - ((lineSize - 1) % 4);
    constexpr std::uint32_t offset = 14 + 108;
    const std::size_t fileSize = offset + static_cast<std::size_t>(height) * (lineSize + padding);
    if (fileSize > 0xffffffffULL) return Error::bigimg;
    const std::array<std::uint8_t, 64> zeros{};
    writer.byte('B'); writer.byte('M');
    writer.little32(static_cast<std::uint32_t>(fileSize));
    writer.little32(0); writer.little32(offset); writer.little32(108);
    writer.little32(width); writer.little32(height);
    writer.little16(1); writer.little16(static_cast<std::uint16_t>(targetChannels * 8));
    writer.little32(targetChannels == 3 ? 0 : 3);
    writer.block(std::span(zeros).first(20));
    if (targetChannels == 3) writer.block(std::span(zeros).first(16));
    else {
        const std::array<std::uint8_t, 16> masks{
            0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 0, 0, 0, 0, 255
        };
        writer.block(masks);
    }
    writer.byte('B'); writer.byte('G'); writer.byte('R'); writer.byte('s');
    writer.block(std::span(zeros).first(48));
    if (writer.failed) return Error::stream;
    Buffer line;
    if (!line.sourceAllocate(lineSize+padding)) { line.sourceFree(); return Error::oom; }
    const int sourceLineSize=width*sourceChannels;
    const int stride=std::bit_cast<int>((0U-std::uint32_t(sourceLineSize))*std::uint32_t(verticalOrientationWrite));
    int sourceIndex=std::bit_cast<int>(std::uint32_t(height-1)*std::uint32_t(sourceLineSize)*
        std::uint32_t(verticalOrientationWrite==1));
    for (int row=0;row<height;++row) {
        const auto lower=static_cast<std::size_t>(sourceIndex);
        const auto upper=static_cast<std::size_t>(std::bit_cast<int>(
            std::uint32_t(sourceIndex)+std::uint32_t(sourceLineSize)));
        if (lower>upper || upper>data.size()) throw compat::ArraySliceError(lower,upper,data.size(),
            compat::RetainedString::fromStaticLiteral("imagefmt/bmp.d"),375);
        convertLine(data.subspan(lower,upper-lower),{line.data(),lineSize},
            sourceChannels,targetChannels==3 ? 5 : 6);
        writer.block({line.data(),line.size()});
        sourceIndex=std::bit_cast<int>(std::uint32_t(sourceIndex)+std::uint32_t(stride));
    }
    const auto error=writer.failed ? Error::stream : Error::none;
    line.sourceFree();
    return error;
}
} // namespace nicxlive::core::imagefmt::detail
