#pragma once

#include "common.hpp"

#include <cstddef>
#include <string>

namespace nicxlive::core::nodes {

class PathDeformer;

// Guard helpers translated from D phys.d
bool guardFinite(PathDeformer* deformer, const std::string& context, float value, std::size_t index = static_cast<std::size_t>(-1));
bool guardFinite(PathDeformer* deformer, const std::string& context, const Vec2& value, std::size_t index = static_cast<std::size_t>(-1));
bool isFiniteMatrix(const Mat4& m);

// Require a finite matrix; if invalid, mark and return identity
Mat4 requireFiniteMatrix(PathDeformer* deformer, const Mat4& m, const std::string& context);

// Summary string for diagnostics
std::string matrixSummary(const Mat4& m);

} // namespace nicxlive::core::nodes
