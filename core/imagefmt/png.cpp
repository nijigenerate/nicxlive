// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of the imagefmt 2.1.2 PNG writer closure. See LICENSE.imagefmt.
#include "writer.hpp"
#include "png_state.hpp"
#include <compat/scope_exit.hpp>
#include <compat/array_range_error.hpp>
#include <zlib.h>
#include <array>
#include <cstring>
#include <limits>

namespace nicxlive::core::imagefmt::detail {
namespace {
constexpr std::size_t maximumChunkSize = 8192;
// Preserve the D thread-local first-error state, including its sticky-error semantics.
thread_local Error pngError = Error::none;
void setError(Error error) { if (pngError == Error::none) pngError = error; }

std::array<std::uint8_t, 4> bigEndian(std::uint32_t value) {
    return {static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
        static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)};
}
struct Crc {
    std::uint32_t value = 0xffffffffU;
    void put(std::span<const std::uint8_t> bytes) {
        for (auto byte : bytes) {
            value ^= byte;
            for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ (0xedb88320U & (0U - (value & 1U)));
        }
    }
    std::array<std::uint8_t, 4> finish() {
        const auto result = bigEndian(value ^ 0xffffffffU);
        value = 0xffffffffU;
        return result;
    }
};

std::uint8_t paeth(std::uint8_t a, std::uint8_t b, std::uint8_t c) {
    int pa = static_cast<int>(b) - c;
    int pb = static_cast<int>(a) - c;
    int pc = pa + pb;
    if (pa < 0) pa = -pa;
    if (pb < 0) pb = -pb;
    if (pc < 0) pc = -pc;
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

struct Encoder {
    Writer& writer;
    z_stream stream{};
    std::span<std::uint8_t> output;
    Crc crc;
    void flush() {
        if (writer.failed) return;
        const auto length = output.size() - stream.avail_out;
        constexpr std::uint8_t tag[]{'I','D','A','T'};
        crc.put(tag); crc.put(output.first(length));
        writer.block(bigEndian(static_cast<std::uint32_t>(length)));
        writer.block(tag); writer.block(output.first(length)); writer.block(crc.finish());
        stream.next_out = output.data();
        stream.avail_out = static_cast<uInt>(output.size());
        if (writer.failed) setError(Error::stream);
    }
    void compress(std::span<const std::uint8_t> line) {
        stream.avail_in = static_cast<uInt>(line.size());
        stream.next_in = const_cast<Bytef*>(line.data());
        while (stream.avail_in) {
            if (deflate(&stream, Z_NO_FLUSH) != Z_OK) { setError(Error::zstream); return; }
            if (stream.avail_out == 0) {
                flush();
            }
        }
    }
};

Error writeIdat(Writer& writer, int width, int height, std::span<const std::uint8_t> data,
    int sourceChannels, int targetChannels) {
    Encoder encoder{writer};
    if (deflateInit(&encoder.stream, Z_DEFAULT_COMPRESSION) != Z_OK) return Error::zinit;
    Error completed=Error::none;
    d_runtime::ngRunWithScopeExit([&] {
        completed=[&]() -> Error {
            const auto sourceLineSize = static_cast<std::size_t>(width) * sourceChannels;
            const auto targetLineSize = static_cast<std::size_t>(width) * targetChannels + 1;
            Buffer work;
            if (!work.sourceAllocate(3 * targetLineSize + maximumChunkSize)) return Error::oom;
            std::memset(work.data(), 0, work.size());
            std::span<std::uint8_t> current(work.data(), targetLineSize);
            std::span<std::uint8_t> previous(work.data() + targetLineSize, targetLineSize);
            const std::span<std::uint8_t> filtered(work.data() + 2 * targetLineSize, targetLineSize);
            encoder.output = {work.data() + 3 * targetLineSize, maximumChunkSize};
            encoder.stream.next_out = encoder.output.data();
            encoder.stream.avail_out = static_cast<uInt>(encoder.output.size());
            setError(Error::none);
            const auto step = static_cast<std::size_t>(targetChannels);
            const auto sourceSize=static_cast<std::size_t>(height)*sourceLineSize;
            const auto stride=sourceLineSize*static_cast<std::size_t>(verticalOrientationWrite);
            auto sourceIndex=(static_cast<std::size_t>(height)-1)*sourceLineSize*(verticalOrientationWrite==-1);
            for (; sourceIndex<sourceSize; sourceIndex+=stride) {
                const auto upper=sourceIndex+sourceLineSize;
                if (sourceIndex>upper || upper>data.size())
                    throw compat::ArraySliceError(sourceIndex,upper,data.size(),
                        compat::RetainedString::fromStaticLiteral("imagefmt/png.d"),806);
                convertLine(data.subspan(sourceIndex,sourceLineSize),
                    current.subspan(1), sourceChannels, targetChannels);
                for (std::size_t i = 1; i <= step; ++i)
                    filtered[i] = static_cast<std::uint8_t>(current[i] - paeth(0, previous[i], 0));
                for (std::size_t i = step + 1; i < targetLineSize; ++i)
                    filtered[i] = static_cast<std::uint8_t>(current[i] - paeth(current[i - step], previous[i], previous[i - step]));
                filtered[0] = 4;
                encoder.compress(filtered);
                std::swap(current, previous);
            }
            while (pngError == Error::none) {
                const int result = deflate(&encoder.stream, Z_FINISH);
                if (encoder.output.size() - encoder.stream.avail_out > 0) encoder.flush();
                if (result == Z_STREAM_END) break;
                if (result == Z_OK) continue;
                setError(Error::zstream);
            }
            work.sourceFree();
            return pngError;
        }();
    },[&] { deflateEnd(&encoder.stream); });
    return completed;
}
} // namespace

Error& pngErrorState() noexcept {
    return pngError;
}

Error writePng(Writer& writer, int width, int height, std::span<const std::uint8_t> data,
    int requestedChannels) {
    if (width < 1 || height < 1) return Error::dim;
    const auto sourceChannels=static_cast<std::uint32_t>(data.size()/static_cast<std::size_t>(width)/height);
    const auto sourceProduct=sourceChannels*static_cast<std::uint32_t>(width)*static_cast<std::uint32_t>(height);
    if (sourceChannels<1 || sourceChannels>4 || sourceProduct!=data.size()) return Error::dim;
    if (static_cast<unsigned>(requestedChannels) > 4) return Error::unsupp;
    const int targetChannels = requestedChannels ? requestedChannels : static_cast<int>(sourceChannels);
    constexpr std::uint8_t colorTypes[]{0, 0, 4, 2, 6};
    std::array<std::uint8_t, 13> header{};
    const auto w = bigEndian(static_cast<std::uint32_t>(width));
    const auto h = bigEndian(static_cast<std::uint32_t>(height));
    std::memcpy(header.data(), w.data(), 4); std::memcpy(header.data() + 4, h.data(), 4);
    header[8] = 8; header[9] = colorTypes[targetChannels];
    constexpr std::uint8_t signature[]{137, 'P', 'N', 'G', 13, 10, 26, 10};
    constexpr std::uint8_t headerSignature[]{0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    Crc crc;
    crc.put(std::span(headerSignature).subspan(4)); crc.put(header);
    writer.block(signature); writer.block(headerSignature); writer.block(header); writer.block(crc.finish());
    if (writer.failed) return Error::stream;
    const auto error = writeIdat(writer, width, height, data, static_cast<int>(sourceChannels), targetChannels);
    if (error != Error::none) return error;
    constexpr std::uint8_t end[]{0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xae, 0x42, 0x60, 0x82};
    writer.block(end);
    return writer.failed ? Error::stream : Error::none;
}
} // namespace nicxlive::core::imagefmt::detail
