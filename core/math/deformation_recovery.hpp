#pragma once

#include "veca.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace nicxlive::core::math {
// Preserve the original component-wise finite-sample selection and operation order.
inline std::size_t inRecoverDeformationOffsets(const Vec2Array& vertices, Vec2Array& offsets,
                                               std::span<const std::uint8_t> missing) {
    struct Sample { std::size_t index; double distance; };
    std::size_t recovered = 0;
    const auto count = std::min(vertices.size(), std::min(offsets.size(), missing.size()));
    for (std::size_t i = 0; i < count; ++i) {
        for (unsigned component = 0; component < 2; ++component) {
            if (!(missing[i] & (1 << component))) continue;
            std::vector<Sample> samples;
            const auto coordinate = [&](std::size_t index) {
                return component == 0 ? offsets.xAt(index) : offsets.yAt(index);
            };
            for (std::size_t j = 0; j < std::min(vertices.size(), offsets.size()); ++j) {
                if (j < missing.size() && (missing[j] & (1 << component))) continue;
                if (!std::isfinite(coordinate(j))) continue;
                const float dx = vertices.xAt(j) - vertices.xAt(i);
                const float dy = vertices.yAt(j) - vertices.yAt(i);
                const double distance = static_cast<double>(dx) * dx + static_cast<double>(dy) * dy;
                if (std::isfinite(distance)) samples.push_back({j, distance});
            }
            if (samples.empty()) continue;
            std::sort(samples.begin(), samples.end(), [](const Sample& a, const Sample& b) {
                return a.distance < b.distance || (a.distance == b.distance && a.index < b.index);
            });
            double value = coordinate(samples[0].index);
            if (samples[0].distance > 1e-8) {
                const auto sampleCount = std::min<std::size_t>(samples.size(), 16);
                const double radius = std::sqrt(samples[sampleCount - 1].distance);
                double a00 = 0, a01 = 0, a02 = 0, a11 = 0, a12 = 0, a22 = 0;
                double b0 = 0, b1 = 0, b2 = 0;
                for (std::size_t k = 0; k < sampleCount; ++k) {
                    const auto sample = samples[k];
                    const float dx = vertices.xAt(sample.index) - vertices.xAt(i);
                    const float dy = vertices.yAt(sample.index) - vertices.yAt(i);
                    const double x = dx / radius, y = dy / radius;
                    const double weight = 1 / std::max(sample.distance, 1e-8);
                    const double v = coordinate(sample.index);
                    a00 += weight; a01 += weight * x; a02 += weight * y;
                    a11 += weight * x * x; a12 += weight * x * y; a22 += weight * y * y;
                    b0 += weight * v; b1 += weight * x * v; b2 += weight * y * v;
                }
                const double c0 = a11 * a22 - a12 * a12;
                const double c1 = a02 * a12 - a01 * a22;
                const double c2 = a01 * a12 - a02 * a11;
                const double determinant = a00 * c0 + a01 * c1 + a02 * c2;
                value = std::abs(determinant) > 1e-10 * a00 * a11 * a22 ?
                    (c0 * b0 + c1 * b1 + c2 * b2) / determinant : b0 / a00;
            }
            if (std::isfinite(value)) {
                const float narrowed = static_cast<float>(value);
                if (!std::isfinite(narrowed)) continue;
                if (component == 0) offsets.xAt(i) = narrowed;
                else offsets.yAt(i) = narrowed;
                ++recovered;
            }
        }
    }
    return recovered;
}
} // namespace nicxlive::core::math
