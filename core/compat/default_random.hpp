#pragma once
// Required Phobos default MT path; BSL-1.0, ../licenses/phobos-default-random.txt.
#include <array>
#include <cstdint>
#include "assert_error.hpp"

namespace nicxlive::core::d_runtime {
using RandomSeedError = AssertError;
// Required current Phobos default Mt19937 and uniform!ulong path.
class DefaultRandom {
public:
    explicit DefaultRandom(std::uint64_t unpredictableSeed);
    std::uint32_t front() const noexcept { return front_; }
    void popFront() noexcept;
    std::uint64_t uniformUlong() noexcept;
private:
    std::array<std::uint32_t, 624> data_{};
    std::uint32_t z_ = 0;
    std::uint32_t front_ = 0;
    int index_ = 623;
};
DefaultRandom& ngDefaultRandom();
std::uint64_t ngRandomUlongExcludingMax();
std::uint32_t ngRandomUintExcludingMax();
}
