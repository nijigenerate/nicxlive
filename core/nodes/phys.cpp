#include "phys.hpp"
#include "path_deformer.hpp"

#include <cmath>
#include <sstream>

namespace nicxlive::core::nodes {

bool isFiniteMatrix(const Mat4& m) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (!std::isfinite(m.a.a[r][c])) return false;
        }
    }
    return true;
}

bool guardFinite(PathDeformer* deformer, const std::string& context, float value, std::size_t index) {
    if (std::isfinite(value)) return true;
    if (deformer) deformer->reportInvalid(context, index == static_cast<std::size_t>(-1) ? 0 : index, Vec2{0, 0});
    return false;
}

bool guardFinite(PathDeformer* deformer, const std::string& context, const Vec2& value, std::size_t index) {
    if (std::isfinite(value.x) && std::isfinite(value.y)) return true;
    if (deformer) deformer->reportInvalid(context, index == static_cast<std::size_t>(-1) ? 0 : index, Vec2{0, 0});
    return false;
}

Mat4 requireFiniteMatrix(PathDeformer* deformer, const Mat4& m, const std::string& context) {
    if (isFiniteMatrix(m)) return m;
    if (deformer) deformer->reportInvalid(context, 0, Vec2{});
    return Mat4::identity();
}

std::string matrixSummary(const Mat4& m) {
    std::stringstream ss;
    ss << "[";
    for (int r = 0; r < 4; ++r) {
        ss << "[";
        for (int c = 0; c < 4; ++c) {
            ss << m.a.a[r][c];
            if (c < 3) ss << ",";
        }
        ss << "]";
        if (r < 3) ss << ",";
    }
    ss << "]";
    return ss.str();
}

} // namespace nicxlive::core::nodes
