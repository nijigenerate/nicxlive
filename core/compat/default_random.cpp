// Current Phobos std.random required path. See ../licenses/phobos-default-random.txt.
#include "default_random.hpp"
#include "native_access_fault.hpp"
#include <bit>
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__)
#include <sys/random.h>
#else
#include <cstdlib>
#endif

namespace nicxlive::core::d_runtime {
namespace {
template<std::size_t M, std::size_t F>
[[noreturn]] void sourceAssertUnreachable(const char (&message)[M], const char (&file)[F], unsigned line) {
#ifdef NDEBUG
    ngAssertUnreachable(message, file, line);
#else
    throw nicxlive::core::d_runtime::ngStaticError<AssertError>(RetainedString::fromStaticLiteral(message),
        RetainedString::fromStaticLiteral(file), line);
#endif
}
#ifdef _WIN32
class BcryptModule {
public:
    BcryptModule() = default;
    BcryptModule(const BcryptModule&) = delete;
    BcryptModule& operator=(const BcryptModule&) = delete;
    bool generate(std::uint64_t& result) {
        if (!handle_) {
            handle_ = ::LoadLibraryA("Bcrypt.dll");
            if (!handle_) sourceAssertUnreachable(
                "LoadLibraryA(\"Bcrypt.dll\") failed.", "std/internal/windows/bcrypt.d", 39);
            generate_ = reinterpret_cast<decltype(&::BCryptGenRandom)>(
                ::GetProcAddress(handle_, "BCryptGenRandom"));
            if (!generate_) sourceAssertUnreachable(
                "GetProcAddress(hBcrypt , \"BCryptGenRandom\") failed.",
                "std/internal/windows/bcrypt.d", 43);
        }
        if (!generate_) ngFailNullAccess(false);
        return generate_(nullptr, reinterpret_cast<PUCHAR>(&result), static_cast<ULONG>(sizeof(result)),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
    }
    ~BcryptModule() noexcept(false) {
        if (handle_) {
            if (!::FreeLibrary(handle_)) sourceAssertUnreachable(
                "FreeLibrary(\"Bcrypt.dll\") failed.", "std/internal/windows/bcrypt.d", 55);
            handle_ = nullptr;
            generate_ = nullptr;
        }
    }
private:
    HMODULE handle_ = nullptr;
    decltype(&::BCryptGenRandom) generate_ = nullptr;
};
#endif
std::uint64_t unpredictableSeed() {
    std::uint64_t seed{};
#ifdef _WIN32
    thread_local BcryptModule module;
    if (!module.generate(seed))
        sourceAssertUnreachable("BCryptGenRandom() failed.", "std/random.d", 1988);
#elif defined(__linux__)
    const auto status = ::getrandom(&seed, sizeof(seed), 0);
#ifndef NDEBUG
    if (status != sizeof(seed)) throw nicxlive::core::d_runtime::ngStaticError<AssertError>(
        RetainedString::fromStaticLiteral("Assertion failure"),
        RetainedString::fromStaticLiteral("std/random.d"), 1976);
#else
    (void)status;
#endif
#else
    ::arc4random_buf(&seed, sizeof(seed));
#endif
    return seed;
}
}
DefaultRandom::DefaultRandom(std::uint64_t seed) {
    if (seed == 0) seed = std::numeric_limits<std::uint64_t>::max();
    std::uint32_t s0 = static_cast<std::uint32_t>(seed);
    std::uint32_t s1 = static_cast<std::uint32_t>(seed >> 32);
    for (auto& entry : data_) {
        entry = std::rotl(s0 * 0x9e3779bbU, 5) * 5U;
        const auto mixed = s0 ^ s1;
        s0 = std::rotl(s0, 26) ^ mixed ^ (mixed << 9);
        s1 = std::rotl(mixed, 13);
    }
    popFront();
    popFront();
}
void DefaultRandom::popFront() noexcept {
    int next = index_ - 1;
    if (next < 0) next = 623;
    int conjugate = index_ - 397;
    if (conjugate < 0) conjugate += 624;
    auto z = z_;
    z ^= z >> 11;
    const auto y = (data_[index_] & 0x80000000U) | (data_[next] & 0x7fffffffU);
    z ^= (z << 7) & 0x9d2c5680U;
    auto x = y >> 1;
    z ^= (z << 15) & 0xefc60000U;
    if (y & 1U) x ^= 0x9908b0dfU;
    z_ = data_[index_] = data_[conjugate] ^ x;
    z ^= z >> 18;
    index_ = next;
    front_ = z;
}
std::uint64_t DefaultRandom::uniformUlong() noexcept {
    const auto high = front();
    popFront();
    const auto value = (static_cast<std::uint64_t>(high) << 32) | front();
    popFront();
    return value;
}
DefaultRandom& ngDefaultRandom() {
    thread_local DefaultRandom generator(unpredictableSeed());
    return generator;
}
std::uint64_t ngRandomUlongExcludingMax() {
    auto& generator = ngDefaultRandom();
    for (;;) {
        const auto value = generator.uniformUlong();
        if (value != std::numeric_limits<std::uint64_t>::max()) return value;
    }
}
std::uint32_t ngRandomUintExcludingMax() {
    auto& generator = ngDefaultRandom();
    for (;;) {
        const auto value = generator.front();
        generator.popFront();
        if (value != std::numeric_limits<std::uint32_t>::max()) return value;
    }
}
}
