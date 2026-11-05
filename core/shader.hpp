#pragma once
// Copyright (c) 2020 Inochi2D Project, 2024 nijigenerate Project.
// Author: Luna Nielsen. Distributed under the 2-Clause BSD License.
// Port of the complete independent nijilive/core/shader.d selected OpenGL profile.

#include "compat/assert_error.hpp"
#include "compat/retained_string.hpp"
#include "compat/throwable.hpp"
#include "common/object_key_policy.hpp"
#include "math/mat4.hpp"
#include "math/types.hpp"
#include "render/backend_handles.hpp"
#include "runtime_state.hpp"
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace nicxlive::core {

class Shader;
inline bool ngShaderObjectEquals(std::shared_ptr<Shader> left, std::shared_ptr<Shader> right);

struct ShaderStageSource {
    d_runtime::RetainedString vertex;
    d_runtime::RetainedString fragment;
};

struct ShaderAsset {
    ShaderStageSource opengl;
    ShaderStageSource directx;
    ShaderStageSource vulkan;

    ShaderStageSource sourceForCurrentBackend() const {
#ifndef NDEBUG
        if (opengl.vertex.empty() || opengl.fragment.empty()) {
            throw d_runtime::ngStaticError<d_runtime::AssertError>(
                d_runtime::RetainedString::fromStaticLiteral("OpenGL shader source is not provided."),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/shader.d"), 27);
        }
#endif
        return opengl;
    }

    static ShaderAsset fromOpenGLSource(d_runtime::RetainedString vertexSource,
            d_runtime::RetainedString fragmentSource) {
        ShaderAsset asset;
        asset.opengl = {std::move(vertexSource), std::move(fragmentSource)};
        return asset;
    }
};

// D string imports are resolved by the resource embedding layer. These template
// arguments are its actual immutable static char arrays, not file paths or an
// evaluator. Retain their literal bytes without building a second source pool.
template<auto& VertexSource, auto& FragmentSource>
    requires (std::is_array_v<std::remove_reference_t<decltype(VertexSource)>> &&
        std::is_array_v<std::remove_reference_t<decltype(FragmentSource)>> &&
        std::is_same_v<std::remove_extent_t<std::remove_reference_t<decltype(VertexSource)>>, const char> &&
        std::is_same_v<std::remove_extent_t<std::remove_reference_t<decltype(FragmentSource)>>, const char>)
inline ShaderAsset shaderAsset() {
    return ShaderAsset::fromOpenGLSource(
        d_runtime::RetainedString::fromStaticLiteral(VertexSource),
        d_runtime::RetainedString::fromStaticLiteral(FragmentSource));
}

class Shader : public std::enable_shared_from_this<Shader> {
    std::shared_ptr<RenderShaderHandle> handle_;
public:
    explicit Shader(ShaderAsset sources) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        const auto variant = sources.sourceForCurrentBackend();
        const auto backend = currentRenderBackend();
        handle_ = backend->createShader(variant.vertex, variant.fragment);
#endif
    }

    Shader(d_runtime::RetainedString vertex, d_runtime::RetainedString fragment)
        : Shader(ShaderAsset::fromOpenGLSource(std::move(vertex), std::move(fragment))) {}

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) = delete;
    Shader& operator=(Shader&&) = delete;

    virtual ~Shader() noexcept(false) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return;
        const auto backend = tryRenderBackend();
        if (backend) backend->destroyShader(handle_);
        handle_.reset();
#endif
    }

    virtual void use() {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return;
        [[maybe_unused]] const auto receiver = shared_from_this();
        const auto backend = currentRenderBackend();
        backend->useShader(handle_);
#endif
    }

    virtual int getUniformLocation(d_runtime::RetainedString name) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return -1;
        [[maybe_unused]] const auto receiver = shared_from_this();
        const auto backend = currentRenderBackend();
        return backend->getShaderUniformLocation(handle_, std::move(name));
#else
        return -1;
#endif
    }

    virtual void setUniform(int uniform, bool value) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return;
        [[maybe_unused]] const auto receiver = shared_from_this();
        const auto backend = currentRenderBackend();
        backend->setShaderUniform(handle_, uniform, value);
#endif
    }

    virtual void setUniform(int uniform, int value) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return;
        [[maybe_unused]] const auto receiver = shared_from_this();
        const auto backend = currentRenderBackend();
        backend->setShaderUniform(handle_, uniform, value);
#endif
    }

    virtual void setUniform(int uniform, float value) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return;
        [[maybe_unused]] const auto receiver = shared_from_this();
        const auto backend = currentRenderBackend();
        backend->setShaderUniform(handle_, uniform, value);
#endif
    }

    virtual void setUniform(int uniform, math::Vec2 value) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return;
        [[maybe_unused]] const auto receiver = shared_from_this();
        const auto backend = currentRenderBackend();
        backend->setShaderUniform(handle_, uniform, value);
#endif
    }

    virtual void setUniform(int uniform, math::Vec3 value) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return;
        [[maybe_unused]] const auto receiver = shared_from_this();
        const auto backend = currentRenderBackend();
        backend->setShaderUniform(handle_, uniform, value);
#endif
    }

    virtual void setUniform(int uniform, math::Vec4 value) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return;
        [[maybe_unused]] const auto receiver = shared_from_this();
        const auto backend = currentRenderBackend();
        backend->setShaderUniform(handle_, uniform, value);
#endif
    }

    virtual void setUniform(int uniform, math::Mat4 value) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        if (!handle_) return;
        [[maybe_unused]] const auto receiver = shared_from_this();
        const auto backend = currentRenderBackend();
        backend->setShaderUniform(handle_, uniform, value);
#endif
    }

    // Inherited Object defaults, object.d (Digital Mars 2000-2011), BSL-1.0.
    // Complete notice: core/licenses/druntime-associative-array.txt.
    virtual d_runtime::RetainedString toString() {
        prepareSourceClassInfo();
        return d_runtime::ngObjectClassName(typeid(*this));
    }
    virtual std::size_t toHash() noexcept {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<Shader> other) {
        return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
    }
    virtual int opCmp(std::shared_ptr<Shader>) {
        prepareSourceClassInfo();
        std::string message = "need opCmp for class ";
        message += d_runtime::ngObjectClassName(typeid(*this)).view();
        throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
            d_runtime::RetainedString::fromStaticLiteral("object.d"), 195);
    }

private:
    friend bool ngShaderObjectEquals(std::shared_ptr<Shader>, std::shared_ptr<Shader>);
    static void prepareSourceClassInfo() {
        static const bool registered = [] {
            d_runtime::ngRegisterObjectClassName(typeid(Shader),
                d_runtime::RetainedString::fromStaticLiteral("nijilive.core.shader.Shader"));
            return true;
        }();
        (void)registered;
    }
};

inline bool ngShaderObjectEquals(std::shared_ptr<Shader> left, std::shared_ptr<Shader> right) {
    return common::ngClassObjectEquals(std::move(left), std::move(right), &Shader::prepareSourceClassInfo);
}

} // namespace nicxlive::core
