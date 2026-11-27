#pragma once
// Independent nijilive/phys/system.d, Copyright Asahi Lina/Inochi2D 2022,
// nijigenerate 2024. BSD-2-Clause; see the SDK LICENSE.
#include "../math/types.hpp"
#include "../math/mat4.hpp"
#include "../common/shared_array.hpp"
#include "../common/resource_key_map.hpp"
#include "../compat/static_error.hpp"
#include "../compat/native_access_fault.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace nicxlive::core::nodes { class PhysicsSystem; }

namespace nicxlive::phys {
class PhysicsSystem : public std::enable_shared_from_this<PhysicsSystem> {
public:
    using Vec2 = core::math::Vec2;
    using Mat4 = core::math::Mat4;
private:
    // The existing private native compatibility class shares these actual cells.
    friend class ::nicxlive::core::nodes::PhysicsSystem;
    using Values = core::common::SharedArray<float>;
    inline static constexpr char sourceFile[] = "source/nijilive/phys/system.d";
    struct PointerPolicy {
        struct Hash {
            std::size_t operator()(const float* value) const noexcept {
                const auto address = reinterpret_cast<std::size_t>(value);
                return address ^ (address >> 4);
            }
        };
        struct Equal {
            bool operator()(const float* left, const float* right) const noexcept { return left == right; }
        };
    };
    core::common::PolicyKeyMap<float*, std::uint64_t, PointerPolicy> variableMap_;
    std::vector<float*> refs;
    std::vector<float> derivative;
    float t_{std::numeric_limits<float>::quiet_NaN()};

    template<class Array> static decltype(auto) at(Array& values, std::uint64_t index, std::size_t line) {
        if (index >= values.size()) throw core::d_runtime::ngStaticError<core::d_runtime::ArrayIndexError>(
            static_cast<std::size_t>(index), values.size(),
            core::d_runtime::RetainedString::fromStaticLiteral(sourceFile), line);
        return values[static_cast<std::size_t>(index)];
    }
    static float load(float* pointer) {
        if (!pointer) core::d_runtime::ngFailNullAccess(false);
        return *pointer;
    }
    static void store(float* pointer, float value) {
        if (!pointer) core::d_runtime::ngFailNullAccess(true);
        *pointer = value;
    }
    std::uint64_t variableIndex(float* pointer, std::size_t line) {
        const auto found = variableMap_.find(pointer);
        if (found == variableMap_.end()) throw core::d_runtime::ngStaticError<core::d_runtime::RangeError>(
            core::d_runtime::RetainedString::fromStaticLiteral(sourceFile), line);
        return found->second;
    }
    void stage(Values current, Values slopes, float h, bool half, std::size_t line) {
        for (std::size_t i = 0; i < current.size(); ++i) {
            auto* destination = at(refs, i, line);
            const float initial = at(current, i, line);
            const float slope = at(slopes, i, line);
            const float product = h * slope;
            const float value = initial + (half ? product / 2.0f : product);
            store(destination, value);
        }
    }
protected:
    virtual std::uint64_t addVariable(float* variable) {
        const std::uint64_t index = refs.size();
        variableMap_[variable] = index;
        refs.push_back(variable);
        return index;
    }
    virtual std::uint64_t addVariable(Vec2* variable) {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        const auto address = reinterpret_cast<std::uintptr_t>(variable);
        const auto index = addVariable(reinterpret_cast<float*>(address + offsetof(Vec2, x)));
        addVariable(reinterpret_cast<float*>(address + offsetof(Vec2, y)));
        return index;
    }
    virtual void setD(std::uint64_t index, float value) { at(derivative, index, 46) = value; }
    virtual void setD(float& variable, float value) {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        [[maybe_unused]] const auto index = variableIndex(&variable, 53);
        setD(variableIndex(&variable, 54), value);
    }
    virtual void setD(Vec2& variable, Vec2 value) {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        setD(variable.x, value.x);
        setD(variable.y, value.y);
    }
    virtual Values getState() {
        Values values;
        for (auto* pointer : refs) values.push_back(load(pointer));
        return values;
    }
    virtual void setState(Values values) {
        const auto length = refs.size();
        for (std::size_t i = 0; i < length; ++i) {
            auto* destination = refs[i];
            const float value = at(values, i, 77);
            store(destination, value);
        }
    }
    virtual void eval(float time) = 0;

public:
    virtual ~PhysicsSystem() = default;
    virtual void tick(float h) {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        const Values current = getState();
        [[maybe_unused]] Values temporary(current.size(), std::numeric_limits<float>::quiet_NaN());
        derivative.resize(current.size(), std::numeric_limits<float>::quiet_NaN());
        for (std::size_t i = 0; i < derivative.size(); ++i) derivative[i] = 0;
        eval(t_);
        const Values k1{std::vector<float>(derivative)};
        stage(current, k1, h, true, 102);
        eval(t_ + h / 2.0f);
        const Values k2{std::vector<float>(derivative)};
        stage(current, k2, h, true, 107);
        eval(t_ + h / 2.0f);
        const Values k3{std::vector<float>(derivative)};
        stage(current, k3, h, false, 112);
        eval(t_ + h);
        const Values k4{std::vector<float>(derivative)};
        for (std::size_t i = 0; i < current.size(); ++i) {
            auto* destination = at(refs, i, 117);
            const float initial = at(current, i, 117);
            const float a = at(k1, i, 117);
            const float b = at(k2, i, 117);
            const float c = at(k3, i, 117);
            const float d = at(k4, i, 117);
            store(destination, initial + h * (a + 2 * b + 2 * c + d) / 6.0f);
            if (!std::isfinite(load(at(refs, i, 118)))) {
                for (std::size_t j = 0; j < current.size(); ++j) {
                    auto* rollback = at(refs, j, 121);
                    const float value = at(current, j, 121);
                    store(rollback, value);
                }
                break;
            }
        }
        t_ += h;
    }
    virtual void updateAnchor() = 0;
    virtual void drawDebug(Mat4 transform = Mat4::identity()) = 0;

};
} // namespace nicxlive::phys
