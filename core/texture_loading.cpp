#include "texture.hpp"
// Inochi2D Project 2020; nijigenerate Project 2024. BSD-2-Clause.
// See licenses/nijilive-shallow-texture.txt.
#include "compat/static_error.hpp"

namespace nicxlive::core {
namespace {
// D module globals are thread-local. Begin intentionally does not clear slots,
// and adding a texture/getting the latest one does not require a started pass.
thread_local common::SharedArray<std::shared_ptr<Texture>> textureBindings;
thread_local bool textureLoadingStarted = false;
}
void inBeginTextureLoading() {
    if (textureLoadingStarted) throw d_runtime::ThrowableException(
        d_runtime::RetainedString("Texture loading pass already started!"),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 610);
    textureLoadingStarted = true;
}
std::shared_ptr<Texture> inGetTextureFromId(std::uint32_t id) {
    if (!textureLoadingStarted) throw d_runtime::ThrowableException(
        d_runtime::RetainedString("Texture loading pass not started!"),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 618);
    const auto index = static_cast<std::size_t>(id);
    if (index >= textureBindings.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, textureBindings.size(),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 619);
    return textureBindings.data()[index];
}
std::shared_ptr<Texture> inGetLatestTexture() {
    const auto index = textureBindings.size() - 1;
    if (index >= textureBindings.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, textureBindings.size(),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 626);
    return textureBindings.data()[index];
}
void inAddTextureBinary(const ShallowTexture& data) {
    const auto input = data;
    textureBindings.push_back(Texture::createSource(input));
}
namespace detail {
void endTextureLoading(bool checkErrors) {
    if (checkErrors && !textureLoadingStarted) throw d_runtime::ThrowableException(
        d_runtime::RetainedString("Texture loading pass not started!"),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/core/texture.d"), 640);
    textureLoadingStarted = false;
    textureBindings.resize(0);
}
}
} // namespace nicxlive::core
