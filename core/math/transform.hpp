#pragma once
// Transform Source: nijilive.math.transform, SDK BSD-2-Clause.
// Inmath pretty rows: David Herberth 2011-2021, MIT; see ../licenses/inmath-transform-source.txt.
// Array comparison: Digital Mars 2000-2019, BSL-1.0; see ../licenses/druntime-string-hash.txt.

#include "mat4.hpp"
#include "mat3.hpp"
#include "serialization.hpp"
#include "../serde.hpp"
#include "../serde_reader.hpp"
#include "../compat/phobos_decimal_format.hpp"
#include <algorithm>
#include <array>

#include <cmath>
#include <exception>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace nicxlive::core::math {

struct Transform {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec2 scale{1.0f, 1.0f};
    bool pixelSnap{false};

    // Cached matrix
    Mat4 trs{Mat4::identity()};

    Transform() = default;
    Transform(const Vec3& t, const Vec3& r = Vec3{0.0f, 0.0f, 0.0f}, const Vec2& s = Vec2{1.0f, 1.0f})
        : translation(t), rotation(r), scale(s) {}

    static Transform identity() { return Transform{}; }

    void clear() {
        translation = Vec3{0.0f, 0.0f, 0.0f};
        rotation = Vec3{0.0f, 0.0f, 0.0f};
        scale = Vec2{1.0f, 1.0f};
    }

    Mat4 toMat4() const { return trs; }
    Mat4 matrix() const { return trs; }

    void update() {
        const Mat4 t = Mat4::translation(translation);
        const long double roll = rotation.x;
        const long double pitch = rotation.y;
        const long double yaw = rotation.z;
        // Match inmath: real half-angle arithmetic, then float quaternion fields.
        const long double cr = std::cos(roll / 2.0L);
        const long double cp = std::cos(pitch / 2.0L);
        const long double cy = std::cos(yaw / 2.0L);
        const long double sr = std::sin(roll / 2.0L);
        const long double sp = std::sin(pitch / 2.0L);
        const long double sy = std::sin(yaw / 2.0L);
        const float w = static_cast<float>(cr * cp * cy + sr * sp * sy);
        const float x = static_cast<float>(sr * cp * cy - cr * sp * sy);
        const float y = static_cast<float>(cr * sp * cy + sr * cp * sy);
        const float z = static_cast<float>(cr * cp * sy - sr * sp * cy);
        const float xx = x * x, xy = x * y, xz = x * z, xw = x * w;
        const float yy = y * y, yz = y * z, yw = y * w, zz = z * z, zw = z * w;
        Mat4 rot = Mat4::identity();
        rot.a.a[0][0] = 1 - 2 * (yy + zz);
        rot.a.a[0][1] = 2 * (xy - zw);
        rot.a.a[0][2] = 2 * (xz + yw);
        rot.a.a[1][0] = 2 * (xy + zw);
        rot.a.a[1][1] = 1 - 2 * (xx + zz);
        rot.a.a[1][2] = 2 * (yz - xw);
        rot.a.a[2][0] = 2 * (xz - yw);
        rot.a.a[2][1] = 2 * (yz + xw);
        rot.a.a[2][2] = 1 - 2 * (xx + yy);
        const auto translatedRotation = Mat4::multiply(t, rot);
        const Mat4 s = Mat4::scale(Vec3{scale.x, scale.y, 1.0f});
        trs = Mat4::multiply(translatedRotation, s);
    }

    Transform calcOffset(const Transform& other) const {
        const auto actualOther = other;
        Transform out;
        out.translation = Vec3{translation.x + actualOther.translation.x,
                               translation.y + actualOther.translation.y,
                               translation.z + actualOther.translation.z};
        out.rotation = Vec3{rotation.x + actualOther.rotation.x,
                            rotation.y + actualOther.rotation.y,
                            rotation.z + actualOther.rotation.z};
        out.scale = Vec2{scale.x * actualOther.scale.x,
                         scale.y * actualOther.scale.y};
        out.update();
        return out;
    }

    std::string toString() const {
        const auto number = [](float value) {
            return ::nicxlive::core::d_runtime::decimal_format::ngFormatDecimal(value);
        };
        const auto rowValue = [&](std::size_t row) {
            return std::array<float, 4>{trs.a.a[row][0], trs.a.a[row][1],
                                        trs.a.a[row][2], trs.a.a[row][3]};
        };
        const auto rowLess = [](const std::array<float, 4>& left, const std::array<float, 4>& right) {
            for (std::size_t column = 0; column < 4; ++column) {
                const float a = left[column], b = right[column];
                const int greater = a > b;
                const int less = a < b;
                const int comparison = greater - less;
                if (comparison) return comparison < 0;
            }
            return false;
        };
        const auto rowString = [&](std::array<float, 4> row) {
            std::string text = "[";
            for (std::size_t column = 0; column < 4; ++column) {
                if (column) text += ", ";
                text += number(row[column]);
            }
            return text + "]";
        };
        auto maximum = rowValue(0);
        for (std::size_t row = 1; row < 4; ++row) {
            const auto current = rowValue(row);
            if (rowLess(maximum, current)) maximum = current;
        }
        const auto maximumWidth = rowString(maximum).size();
        auto minimum = rowValue(0);
        for (std::size_t row = 1; row < 4; ++row) {
            const auto current = rowValue(row);
            if (rowLess(current, minimum)) minimum = current;
        }
        const auto minimumWidth = rowString(minimum).size();
        const auto width = (std::max)(maximumWidth, minimumWidth) - 1;
        std::string result = "[";
        for (std::size_t row = 0; row < 4; ++row) {
            if (row) result += "\n ";
            result += "[";
            for (std::size_t column = 0; column < 4; ++column) {
                if (column) result += ", ";
                const auto text = number(trs.a.a[row][column]);
                if (text.size() < width) result.append(width - text.size(), ' ');
                result += text;
            }
            result += "]";
        }
        result += "],\n[";
        const std::array<float, 3> translationValue{translation.x, translation.y, translation.z};
        for (std::size_t i = 0; i < translationValue.size(); ++i) {
            if (i) result += ", ";
            result += number(translationValue[i]);
        }
        result += "],\n[";
        const std::array<float, 3> rotationValue{rotation.x, rotation.y, rotation.z};
        for (std::size_t i = 0; i < rotationValue.size(); ++i) {
            if (i) result += ", ";
            result += number(rotationValue[i]);
        }
        result += "]\n[";
        const std::array<float, 2> scaleValue{scale.x, scale.y};
        for (std::size_t i = 0; i < scaleValue.size(); ++i) {
            if (i) result += ", ";
            result += number(scaleValue[i]);
        }
        return result + "]";
    }

    template<class Serializer>
        requires requires(Serializer& writer, float& element) {
            writer.structBegin();
            writer.listBegin();
            writer.putKey(::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("trans"));
            writer.elemBegin();
            writer.serializeValue(element);
        } && requires(Serializer& writer,
            std::decay_t<decltype(std::declval<Serializer&>().structBegin())>& objectState,
            std::decay_t<decltype(std::declval<Serializer&>().listBegin())>& listState) {
            writer.structEnd(objectState);
            writer.listEnd(listState);
        } && std::is_destructible_v<std::decay_t<decltype(std::declval<Serializer&>().structBegin())>>
          && std::is_destructible_v<std::decay_t<decltype(std::declval<Serializer&>().listBegin())>>
          && (!std::is_reference_v<decltype(std::declval<Serializer&>().structBegin())> ||
              std::is_constructible_v<std::decay_t<decltype(std::declval<Serializer&>().structBegin())>,
                  decltype(std::declval<Serializer&>().structBegin())>)
          && (!std::is_reference_v<decltype(std::declval<Serializer&>().listBegin())> ||
              std::is_constructible_v<std::decay_t<decltype(std::declval<Serializer&>().listBegin())>,
                  decltype(std::declval<Serializer&>().listBegin())>)
    void serialize(Serializer& serializer) const {
        auto state = serializer.structBegin();
        serializer.putKey(::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("trans"));
        math::serialize(std::array<float, 3>{translation.x, translation.y, translation.z}, serializer);
        serializer.putKey(::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("rot"));
        math::serialize(std::array<float, 3>{rotation.x, rotation.y, rotation.z}, serializer);
        serializer.putKey(::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("scale"));
        math::serialize(std::array<float, 2>{scale.x, scale.y}, serializer);
        serializer.structEnd(state);
    }

    void serialize(::nicxlive::core::serde::TypedJsonWriter& serializer) const {
        serialize<::nicxlive::core::serde::TypedJsonWriter>(serializer);
    }

    ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) {
        namespace serde = ::nicxlive::core::serde;
        // Actual scalar refs preserve prefix writes on returned errors and throws.
        // The original Transform intentionally ignores the returned vector errors.
        (void)math::deserialize(translation, data.findFirst("trans"));
        (void)math::deserialize(rotation, data.findFirst("rot"));
        (void)math::deserialize(scale, data.findFirst("scale"));
        return {};
    }

    void serialize(::nicxlive::core::serde::InochiSerializer& serializer) const {
        auto writeVec = [&](const char* key, const float* src, std::size_t n) {
            boost::property_tree::ptree arr;
            for (std::size_t i = 0; i < n; ++i) {
                boost::property_tree::ptree v;
                v.put("", src[i]);
                arr.push_back({"", v});
            }
            serializer.root.add_child(key, arr);
        };
        const float trans[3]{translation.x, translation.y, translation.z};
        const float rot[3]{rotation.x, rotation.y, rotation.z};
        const float scl[2]{scale.x, scale.y};
        writeVec("trans", trans, 3);
        writeVec("rot", rot, 3);
        writeVec("scale", scl, 2);
    }

    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
        try {
            auto readVec = [](const ::nicxlive::core::serde::Fghj& node, float* dst, std::size_t n) {
                std::size_t i = 0;
                for (const auto& elem : node) {
                    if (i >= n) break;
                    dst[i++] = elem.second.get_value<float>();
                }
            };

            if (auto trans = data.get_child_optional("trans")) {
                float dst[3]{translation.x, translation.y, translation.z};
                readVec(*trans, dst, 3);
                translation = Vec3{dst[0], dst[1], dst[2]};
            } else {
                translation.x = data.get<float>("trans.x", translation.x);
                translation.y = data.get<float>("trans.y", translation.y);
                translation.z = data.get<float>("trans.z", translation.z);
            }

            if (auto rot = data.get_child_optional("rot")) {
                float dst[3]{rotation.x, rotation.y, rotation.z};
                readVec(*rot, dst, 3);
                rotation = Vec3{dst[0], dst[1], dst[2]};
            } else {
                rotation.x = data.get<float>("rot.x", rotation.x);
                rotation.y = data.get<float>("rot.y", rotation.y);
                rotation.z = data.get<float>("rot.z", rotation.z);
            }

            if (auto scl = data.get_child_optional("scale")) {
                float dst[2]{scale.x, scale.y};
                readVec(*scl, dst, 2);
                scale = Vec2{dst[0], dst[1]};
            } else {
                scale.x = data.get<float>("scale.x", scale.x);
                scale.y = data.get<float>("scale.y", scale.y);
            }
            update();
        } catch (const std::exception& e) {
            return std::string(e.what());
        }
        return std::nullopt;
    }

    Transform operator*(const Transform& other) const {
        const auto actualOther = other;
        Transform tnew;
        const auto currentMatrix = trs;
        Mat4 strs = Mat4::multiply(actualOther.trs, currentMatrix);
        std::array<float, 4> origin{};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                origin[r] = detail::inmathAccumulate(origin[r],
                    detail::inmathProduct(strs.a.a[r][c], c == 3 ? 1.0f : 0.0f));
            }
        }
        tnew.translation = Vec3{origin[0], origin[1], origin[2]};
        tnew.rotation = Vec3{rotation.x + actualOther.rotation.x,
                             rotation.y + actualOther.rotation.y,
                             rotation.z + actualOther.rotation.z};
        tnew.scale = Vec2{scale.x * actualOther.scale.x,
                          scale.y * actualOther.scale.y};
        tnew.trs = strs;
        return tnew;
    }
};

struct Transform2D {
    Vec2 translation{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    Vec2 scale{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    float rotation{std::numeric_limits<float>::quiet_NaN()};
    Mat3x3 trs{{
        {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()},
        {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()},
        {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()},
    }};

    Mat3x3 matrix() const { return trs; }

    void update() {
        Mat3x3 translationM{};
        translationM.m[0][2] = translation.x;
        translationM.m[1][2] = translation.y;
        translationM.m[2][2] = 0.0f;

        const long double angle = rotation;
        float c = static_cast<float>(std::cos(angle));
        float s = static_cast<float>(std::sin(angle));
        Mat3x3 rotationM{};
        rotationM.m[0][0] = c;
        rotationM.m[0][1] = -s;
        rotationM.m[0][2] = 0.0f;
        rotationM.m[1][0] = s;
        rotationM.m[1][1] = c;
        rotationM.m[1][2] = 0.0f;
        rotationM.m[2][0] = 0.0f;
        rotationM.m[2][1] = 0.0f;
        rotationM.m[2][2] = 1.0f;

        Mat3x3 scaleM{};
        scaleM.m[0][0] = scale.x;
        scaleM.m[1][1] = scale.y;
        scaleM.m[2][2] = 1.0f;

        trs = multiply(multiply(translationM, rotationM), scaleM);
    }
};

} // namespace nicxlive::core::math
