#pragma once

#include <compat/static_error.hpp>
#include <load_diagnostics.hpp>
#include <puppet.hpp>
#include <serde_reader.hpp>
#include <texture.hpp>
#include "binfmt.hpp"
#include <algorithm>
#include <type_traits>

namespace nicxlive::core::fmt {
namespace typed_reader_detail {
inline constexpr auto sourceFile = "nijilive/fmt/package.d";
inline void enforce(bool value, const char* message, std::size_t line) {
    if (!value) throw d_runtime::ThrowableException(d_runtime::RetainedString(message),
        d_runtime::RetainedString::fromBorrowedView(sourceFile), line);
}
struct BufferReader {
    common::SharedArray<std::uint8_t> bytes;
    std::size_t offset = 0;
    common::SharedArray<std::uint8_t> take(std::size_t count, std::size_t line) {
        const auto start = offset;
        offset += count;
        if (start > offset || offset > bytes.size())
            throw d_runtime::ngStaticError<nicxlive::core::d_runtime::ArraySliceError>(
                start, offset, bytes.size(), d_runtime::RetainedString::fromBorrowedView(sourceFile), line);
        return bytes.slice(start, offset);
    }
    std::uint8_t byte(std::size_t line) {
        const auto index = offset++;
        if (index >= bytes.size())
            throw d_runtime::ngStaticError<nicxlive::core::d_runtime::ArrayIndexError>(
                index, bytes.size(), d_runtime::RetainedString::fromBorrowedView(sourceFile), line);
        return bytes.data()[index];
    }
    std::uint32_t word(std::size_t line) {
        auto data = take(4, line);
        return (std::uint32_t(data[0]) << 24) | (std::uint32_t(data[1]) << 16) |
            (std::uint32_t(data[2]) << 8) | data[3];
    }
};
inline bool section(common::SharedArray<std::uint8_t> data, const std::array<std::uint8_t, 8>& expected) {
    return data.size() >= expected.size() && std::equal(expected.begin(), expected.end(), data.begin());
}
inline d_runtime::RetainedString retainedString(common::SharedArray<std::uint8_t> bytes) {
    const auto owner = bytes.retentionOwner();
    const auto data = reinterpret_cast<const char*>(bytes.data());
    if (owner && data) return d_runtime::RetainedString::retainedView(owner, data, bytes.size());
    // A nonnull ownerless Slice is the existing process-static input contract.
    return d_runtime::RetainedString::fromBorrowedView(std::string_view(data, bytes.size()));
}
[[noreturn]] inline void throwReturnedSerde(const nicxlive::core::serde::ReadError& error) {
    using namespace nicxlive::core::serde::source_exceptions;
    if (const auto object = dynamic_cast<const FghjSerdeException*>(error.get())) throw *object;
    if (const auto object = dynamic_cast<const EmptyFghjException*>(error.get())) throw *object;
    throw *d_runtime::ngRequireObject(error.object());
}
}
template<class T> std::shared_ptr<T> inLoadTypedPuppetJson(std::string_view json) {
    static_assert(std::is_same_v<decltype(T::create()), std::shared_ptr<T>>,
        "A Source-derived Puppet must supply its actual retained construction factory");
    nicxlive::core::serde::inLastLoadDiagnostics = {};
    auto document = nicxlive::core::serde::parseTypedJson(json);
    auto puppet = T::create();
    if (const auto error = puppet->deserializeFromFghj(document.root()))
        typed_reader_detail::throwReturnedSerde(error);
    return puppet;
}
template<class T> std::shared_ptr<T> inLoadTypedPuppetBinary(common::SharedArray<std::uint8_t> buffer) {
    using namespace nicxlive::core;
    typed_reader_detail::BufferReader reader{buffer};

    typed_reader_detail::enforce(typed_reader_detail::section(buffer, fmt::MAGIC_BYTES), "Invalid data format for INP puppet", 81);
    reader.offset = 8;
    const auto jsonLength = reader.word(86);
    const auto json = typed_reader_detail::retainedString(reader.take(jsonLength, 88));
    typed_reader_detail::enforce(typed_reader_detail::section(reader.take(8, 90), fmt::TEX_SECTION),
        "Expected Texture Blob section, got nothing!", 90);
    // The selected editor profile is the original InDoesRender configuration.
    inBeginTextureLoading();
    struct TexturePass {
        bool active = true;
        ~TexturePass() { if (active) inEndTextureLoading<false>(); }
    } texturePass;
    const auto count = reader.word(98);
    common::SharedArray<std::shared_ptr<Texture>> slots;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto length = reader.word(104);
        [[maybe_unused]] const auto textureType = reader.byte(106);
        if (length == 0) inAddTextureBinary(ShallowTexture(SourceTextureBytes{}, 0, 0, 4));
        else {
            const auto bytes = reader.take(length, 109);
            inAddTextureBinary(ShallowTexture(std::span<const uint8_t>{bytes.data(), bytes.size()}));
        }
        slots.push_back(inGetLatestTexture());
    }
    auto puppet = inLoadTypedPuppetJson<T>(json.view());
    puppet->textureSlots = slots;
    puppet->updateTextureState();
    inEndTextureLoading();
    texturePass.active = false;
    if (buffer.size() >= reader.offset + 8 && typed_reader_detail::section(reader.take(8, 139), fmt::EXT_SECTION)) {
        const auto sectionCount = reader.word(141);
        for (std::uint32_t i = 0; i < sectionCount; ++i) {
            const auto nameLength = reader.word(148);
            const auto name = typed_reader_detail::retainedString(reader.take(nameLength, 149));
            const auto payloadLength = reader.word(153);
            const auto payload = reader.take(payloadLength, 156);
            puppet->extData[name] = payload;
        }
    }
    return puppet;
}
} // namespace nicxlive::core::fmt
