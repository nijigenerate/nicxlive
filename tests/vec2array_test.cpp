#include "../core/math/veca.hpp"

#include <cassert>
#include <cmath>
#include <vector>

using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec2Array;

namespace {

bool nearlyEqual(float a, float b, float eps = 1e-6f) {
    return std::fabs(a - b) <= eps;
}

void testSoALayoutAndRawStorage() {
    Vec2Array arr;
    arr.append(Vec2{1.0f, 10.0f});
    arr.append(Vec2{2.0f, 20.0f});
    arr.append(Vec2{3.0f, 30.0f});

    assert(arr.size() == 3);
    auto raw = arr.rawStorage();
    assert(raw.length == 6);
    assert(raw.ptr != nullptr);
    assert(nearlyEqual(raw.ptr[0], 1.0f));
    assert(nearlyEqual(raw.ptr[1], 2.0f));
    assert(nearlyEqual(raw.ptr[2], 3.0f));
    assert(nearlyEqual(raw.ptr[3], 10.0f));
    assert(nearlyEqual(raw.ptr[4], 20.0f));
    assert(nearlyEqual(raw.ptr[5], 30.0f));

    auto lanes = arr.lanes();
    assert(lanes[0] != nullptr && lanes[1] != nullptr);
    assert(lanes[0]->size() == 3);
    assert(lanes[1]->size() == 3);
    assert(arr.alignment() == 16);
    assert(arr.backing().size() == 6);
}

void testSIMDPathForPlusMinusMul() {
    Vec2Array lhs;
    Vec2Array rhs;
    for (int i = 0; i < 8; ++i) {
        lhs.append(Vec2{static_cast<float>(i), static_cast<float>(i + 100)});
        rhs.append(Vec2{1.0f, 2.0f});
    }

    lhs += rhs;
    for (int i = 0; i < 8; ++i) {
        auto v = lhs.at(static_cast<std::size_t>(i));
        assert(nearlyEqual(v.x, static_cast<float>(i + 1)));
        assert(nearlyEqual(v.y, static_cast<float>(i + 102)));
    }

    lhs -= rhs;
    for (int i = 0; i < 8; ++i) {
        auto v = lhs.at(static_cast<std::size_t>(i));
        assert(nearlyEqual(v.x, static_cast<float>(i)));
        assert(nearlyEqual(v.y, static_cast<float>(i + 100)));
    }

    lhs *= 2.0f;
    for (int i = 0; i < 8; ++i) {
        auto v = lhs.at(static_cast<std::size_t>(i));
        assert(nearlyEqual(v.x, static_cast<float>(i * 2)));
        assert(nearlyEqual(v.y, static_cast<float>((i + 100) * 2)));
    }
}

void testExternalStorageView() {
    Vec2Array storage;
    storage.append(Vec2{0.0f, 0.0f});
    storage.append(Vec2{1.0f, 10.0f});
    storage.append(Vec2{2.0f, 20.0f});
    storage.append(Vec2{3.0f, 30.0f});

    Vec2Array view;
    view.bindExternalStorage(storage, 1, 2);
    assert(view.size() == 2);
    assert(nearlyEqual(view.at(0).x, 1.0f));
    assert(nearlyEqual(view.at(0).y, 10.0f));
    assert(nearlyEqual(view.at(1).x, 2.0f));
    assert(nearlyEqual(view.at(1).y, 20.0f));

    view.set(1, Vec2{9.0f, 90.0f});
    assert(nearlyEqual(storage.at(2).x, 9.0f));
    assert(nearlyEqual(storage.at(2).y, 90.0f));
}

} // namespace

int main() {
    testSoALayoutAndRawStorage();
    testSIMDPathForPlusMinusMul();
    testExternalStorageView();
    return 0;
}
