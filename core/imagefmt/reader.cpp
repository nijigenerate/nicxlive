// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of imagefmt 2.1.2. See LICENSE.imagefmt.
#include "reader.hpp"
#include "nt_string.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <bit>
#include <array>
#include <compat/array_range_error.hpp>
#include <compat/scope_exit.hpp>
#include <compat/assert_error.hpp>

namespace nicxlive::core::imagefmt {
thread_local std::uint64_t maximumImageSize = 0x7fffffff;
thread_local int verticalOrientationRead = 1;
namespace detail {
namespace {
int sourceAdd(int left, int right) {
    return std::bit_cast<int>(static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}
int sourceSubtract(int left, int right) {
    return std::bit_cast<int>(static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
}
const std::uint8_t* sourcePointer(const std::uint8_t* base, std::size_t offset) {
    return reinterpret_cast<const std::uint8_t*>(reinterpret_cast<std::uintptr_t>(base) + offset);
}
std::span<const std::uint8_t> pointerSlice(const std::uint8_t* base,
        std::size_t first, std::size_t last, std::size_t line) {
    if (first > last)
        throw d_runtime::ArraySliceError(first, last, 0,
            d_runtime::RetainedString::fromStaticLiteral("imagefmt/package.d"), line);
    return {sourcePointer(base, first), last - first};
}
std::span<std::uint8_t> targetSlice(std::span<std::uint8_t> target,
        std::size_t first, std::size_t last, std::size_t line) {
    if (first > last || last > target.size())
        throw d_runtime::ArraySliceError(first, last, target.size(),
            d_runtime::RetainedString::fromStaticLiteral("imagefmt/package.d"), line);
    return {const_cast<std::uint8_t*>(sourcePointer(target.data(), first)), last - first};
}
using ReaderCallback = void (*)(Reader&);
using ReaderSkipCallback = void (*)(Reader&, int);
// These are the actual three source module variables. They are TLS shared by
// readers on one OS thread, not independent state or Fiber-local dispatch.
thread_local ReaderCallback fillbuf{};
thread_local ReaderCallback reset2start{};
thread_local ReaderSkipCallback skipReader{};
void fillMemory(Reader& rc) { rc.a = 0; rc.fail = true; }
void resetMemory(Reader& rc) { rc.a = 0; rc.fail = false; }
void skipMemory(Reader& rc, int count) {
    if (count <= sourceSubtract(rc.b, rc.a)) { rc.a = sourceAdd(rc.a, count); return; }
    rc.fail = true;
}
void fillIo(Reader& rc) {
    rc.a = 0;
    if (rc.fail) return;
    const int count = rc.io.read(rc.io.stream, const_cast<std::uint8_t*>(rc.buf), rc.cap);
    rc.bufpos = rc.iopos;
    rc.iopos = sourceAdd(rc.iopos, count);
    rc.b = count;
    rc.fail = count == 0;
}
void resetIo(Reader& rc) {
    rc.fail = false; rc.a = 0;
    const int offset = sourceAdd(sourceSubtract(0, rc.iopos), rc.bufpos == 0 ? rc.cap : 0);
    if (rc.io.seek(rc.io.stream, offset) == 0) rc.iopos = sourceAdd(rc.iopos, offset);
    else rc.fail = true;
    if (rc.bufpos != 0) rc.fill();
}
void skipIo(Reader& rc, int count) {
    if (count <= sourceSubtract(rc.b, rc.a)) { rc.a = sourceAdd(rc.a, count); return; }
    if (rc.a < rc.b) { count = sourceSubtract(count, sourceSubtract(rc.b, rc.a)); rc.a = rc.b; }
    if (rc.io.seek(rc.io.stream, count) == 0) rc.iopos = sourceAdd(rc.iopos, count);
    else rc.fail = true;
}
}
Error Reader::initialize(std::span<const std::uint8_t> input, Read source) {
    if (input.size() < 16) {
        if (source.stream) return Error::arg;
        if (input.empty()) return Error::nodata;
    }
    io = source; buf = input.data();
    cap = b = std::bit_cast<int>(static_cast<std::uint32_t>(input.size()));
    a = bufpos = iopos = 0; fail = false;
    if (io.stream) {
        fillbuf = &fillIo; reset2start = &resetIo; skipReader = &skipIo;
        fill();
        if (iopos == 0) return Error::nodata;
    } else {
        fillbuf = &fillMemory; reset2start = &resetMemory; skipReader = &skipMemory;
    }
    return Error::none;
}
void Reader::fill() { fillbuf(*this); }
void Reader::reset() { reset2start(*this); }
void Reader::skip(int count) { skipReader(*this, count); }
void Reader::skipTo(int position) {
    if (position >= sourceAdd(bufpos, b)) {
        skip(sourceAdd(sourceSubtract(position, bufpos), a)); return;
    }
    if (position >= sourceAdd(bufpos, a)) { a = sourceSubtract(position, bufpos); return; }
    fail = true;
}
std::uint8_t Reader::byte() {
    if (a < b) {
        const int index = a; a = sourceAdd(a, 1);
        return *sourcePointer(buf, static_cast<std::size_t>(index));
    }
    if (b == cap) {
        fill();
        // Source read_u8 returns this byte even after fill marks EOF. Do not
        // replace its stale-byte/pointer behavior with a synthetic zero.
        const int index = a; a = sourceAdd(a, 1);
        return *sourcePointer(buf, static_cast<std::size_t>(index));
    }
    fail = true; return 0;
}
std::uint16_t Reader::little16() {
    const auto first = byte(); const auto second = byte();
    return static_cast<std::uint16_t>((second << 8) + first);
}
std::uint16_t Reader::big16() {
    const auto first = byte(); const auto second = byte();
    return static_cast<std::uint16_t>((first << 8) + second);
}
std::uint32_t Reader::little32() {
    const auto first = little16(); const auto second = little16();
    return (static_cast<std::uint32_t>(second) << 16) + first;
}
std::uint32_t Reader::big32() {
    const auto first = big16(); const auto second = big16();
    return (static_cast<std::uint32_t>(first) << 16) + second;
}
void Reader::block(std::span<std::uint8_t> target) {
    int position = 0;
    while (true) {
        const auto need = target.size() - static_cast<std::size_t>(position);
        const auto end = static_cast<std::size_t>(a) + need;
        if (end <= static_cast<std::size_t>(b)) {
            const auto source = pointerSlice(buf, static_cast<std::size_t>(a), end, 877);
            const auto destination = targetSlice(target, static_cast<std::size_t>(position), target.size(), 877);
            if (need) std::memmove(destination.data(), source.data(), need);
            a = std::bit_cast<int>(static_cast<std::uint32_t>(static_cast<std::size_t>(a) + need));
            return;
        }
        if (a < b) {
            const int count = sourceSubtract(b, a);
            const auto source = pointerSlice(buf, static_cast<std::size_t>(a), static_cast<std::size_t>(b), 883);
            const auto destination = targetSlice(target, static_cast<std::size_t>(position),
                static_cast<std::size_t>(sourceAdd(position, count)), 883);
            if (destination.size()) std::memmove(destination.data(), source.data(), destination.size());
            a = sourceAdd(a, count); position = sourceAdd(position, count);
        }
        // Source has no EOF/zero-progress escape here. In particular exhausted
        // stream input can keep this loop running; preserve that authored rule.
        fill();
    }
}
std::span<const std::uint8_t> Reader::slice(int maximum) {
    do {
        if (a < b) {
            const int begin = a;
            // D int arithmetic wraps at its declared width. Pointer-slice
            // bounds are then converted to size_t, with no private lower guard.
            const int available = std::bit_cast<int>(
                static_cast<std::uint32_t>(b) - static_cast<std::uint32_t>(a));
            const int count = maximum < available ? maximum : available;
            a = std::bit_cast<int>(static_cast<std::uint32_t>(a) +
                static_cast<std::uint32_t>(count));
            const auto lower = static_cast<std::size_t>(begin);
            const auto upper = static_cast<std::size_t>(a);
            // package.d:900 updates rc.a before the raw pointer slice at 901.
            // Its reverse-bound error has no known backing-array length.
            if (lower > upper)
                throw d_runtime::ArraySliceError(lower, upper, 0,
                    d_runtime::RetainedString::fromStaticLiteral("imagefmt/package.d"), 901);
            const auto address = reinterpret_cast<std::uintptr_t>(buf) + lower;
            return std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(address), upper - lower);
        }
        fill();
    } while (!fail);
    return {};
}
bool convertBits(Buffer& buffer, int source, int target) {
    if (source == target) return true;
    if (source == 8 && target == 16) {
        const auto n = buffer.size();
        if (!buffer.sourceReallocate(n * 2)) {
            buffer.sourceFree(); return false;
        }
        auto* words = reinterpret_cast<std::uint16_t*>(buffer.data());
        for (auto i = n; i != 0; --i) words[i - 1] = buffer.data()[i - 1] * 257;
        return true;
    }
    if (source == 16 && target == 8) {
        const auto n = buffer.size() / 2;
        const auto* words = reinterpret_cast<const std::uint16_t*>(buffer.data());
        for (std::size_t i = 0; i < n; ++i) buffer.data()[i] = static_cast<std::uint8_t>(words[i] >> 8);
        if (!buffer.sourceReallocate(n)) { buffer.sourceFree(); return false; }
        return true;
    }
    return false;
}
template<class T> void convert(std::span<const T> source, std::span<T> target, int sc, int tc) {
    constexpr bool wide = sizeof(T) == 2;
    const auto file = compat::RetainedString::fromStaticLiteral("imagefmt/package.d");
    const auto read = [&](int index, std::size_t line) -> T {
        const auto i = static_cast<std::size_t>(index);
        if (i >= source.size()) throw compat::ArrayIndexError(i, source.size(), file, line);
        return source[i];
    };
    const auto write = [&](int index, std::size_t line) -> T& {
        const auto i = static_cast<std::size_t>(index);
        if (i >= target.size()) throw compat::ArrayIndexError(i, target.size(), file, line);
        return target[i];
    };
    const auto fromSlice = [&](int lower, int upper, std::size_t line) {
        const auto lo = static_cast<std::size_t>(lower), hi = static_cast<std::size_t>(upper);
        if (lo > hi || hi > source.size()) throw compat::ArraySliceError(lo, hi, source.size(), file, line);
        return source.subspan(lo, hi - lo);
    };
    const auto toSlice = [&](int lower, int upper, std::size_t line) {
        const auto lo = static_cast<std::size_t>(lower), hi = static_cast<std::size_t>(upper);
        if (lo > hi || hi > target.size()) throw compat::ArraySliceError(lo, hi, target.size(), file, line);
        return target.subspan(lo, hi - lo);
    };
    if (sc == tc) {
        if (source.size() != target.size()) throw compat::RangeError(file, wide ? 577 : 415);
        if (!source.empty()) std::memmove(target.data(), source.data(), source.size_bytes());
        return;
    }
    switch (sourceAdd(std::bit_cast<int>(std::uint32_t(sc) * 16U), tc)) {
    case 18:
    {
        // Source conv_y2ya/conv16_y2ya; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 1), t = sourceAdd(t, 2)) {
            const auto component1_0 = read(k, (wide ? 583 : 421));
            write(t, (wide ? 583 : 421)) = component1_0;
            write(sourceAdd(t, 1), (wide ? 584 : 422)) = std::numeric_limits<T>::max();
        }
        return;
    }
    case 19:
    case 21:
    {
        // Source conv_y2rgb/conv16_y2rgb; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 1), t = sourceAdd(t, 3)) {
            const auto component2_0 = read(k, (wide ? 591 : 429));
            const auto value = component2_0;
            auto output = toSlice(t, sourceAdd(t, 3), (wide ? 591 : 429));
            std::fill(output.begin(), output.end(), value);
        }
        return;
    }
    case 20:
    case 22:
    {
        // Source conv_y2rgba/conv16_y2rgba; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 1), t = sourceAdd(t, 4)) {
            const auto component3_0 = read(k, (wide ? 597 : 435));
            const auto value = component3_0;
            auto output = toSlice(t, sourceAdd(t, 3), (wide ? 597 : 435));
            std::fill(output.begin(), output.end(), value);
            write(sourceAdd(t, 3), (wide ? 598 : 436)) = std::numeric_limits<T>::max();
        }
        return;
    }
    case 33:
    {
        // Source conv_ya2y/conv16_ya2y; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 2), t = sourceAdd(t, 1)) {
            const auto component4_0 = read(k, (wide ? 605 : 443));
            write(t, (wide ? 605 : 443)) = component4_0;
        }
        return;
    }
    case 35:
    case 37:
    {
        // Source conv_ya2rgb/conv16_ya2rgb; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 2), t = sourceAdd(t, 3)) {
            const auto component5_0 = read(k, (wide ? 611 : 449));
            const auto value = component5_0;
            auto output = toSlice(t, sourceAdd(t, 3), (wide ? 611 : 449));
            std::fill(output.begin(), output.end(), value);
        }
        return;
    }
    case 36:
    case 38:
    {
        // Source conv_ya2rgba/conv16_ya2rgba; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 2), t = sourceAdd(t, 4)) {
            const auto component6_0 = read(k, (wide ? 617 : 455));
            const auto value = component6_0;
            auto output = toSlice(t, sourceAdd(t, 3), (wide ? 617 : 455));
            std::fill(output.begin(), output.end(), value);
            const auto component7_0 = read(sourceAdd(k, 1), (wide ? 618 : 456));
            write(sourceAdd(t, 3), (wide ? 618 : 456)) = component7_0;
        }
        return;
    }
    case 49:
    {
        // Source conv_rgb2y/conv16_rgb2y; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 3), t = sourceAdd(t, 1)) {
            const auto component8_0 = read(k, (wide ? 625 : 463));
            const auto component8_1 = read(sourceAdd(k, 1), (wide ? 625 : 463));
            const auto component8_2 = read(sourceAdd(k, 2), (wide ? 625 : 463));
            write(t, (wide ? 625 : 463)) = static_cast<T>(0.21 * component8_0 + 0.64 *  component8_1 + 0.15 *  component8_2);
        }
        return;
    }
    case 50:
    {
        // Source conv_rgb2ya/conv16_rgb2ya; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 3), t = sourceAdd(t, 2)) {
            const auto component9_0 = read(k, (wide ? 631 : 469));
            const auto component9_1 = read(sourceAdd(k, 1), (wide ? 631 : 469));
            const auto component9_2 = read(sourceAdd(k, 2), (wide ? 631 : 469));
            write(t, (wide ? 631 : 469)) = static_cast<T>(0.21 * component9_0 + 0.64 *  component9_1 + 0.15 *  component9_2);
            write(sourceAdd(t, 1), (wide ? 632 : 470)) = std::numeric_limits<T>::max();
        }
        return;
    }
    case 52:
    {
        // Source conv_rgb2rgba/conv16_rgb2rgba; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 3), t = sourceAdd(t, 4)) {
            const auto input = fromSlice(k, sourceAdd(k, 3), (wide ? 639 : 477));
            auto output = toSlice(t, sourceAdd(t, 3), (wide ? 639 : 477));
            std::memmove(output.data(), input.data(), input.size_bytes());
            write(sourceAdd(t, 3), (wide ? 640 : 478)) = std::numeric_limits<T>::max();
        }
        return;
    }
    case 53:
    case 83:
    {
        // Source conv_rgb2bgr/conv16_rgb2bgr; retain each statement and its failure prefix.
        for (int k = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 3)) {
            const auto component10_0 = read(sourceAdd(k, 2), (wide ? 647 : 485));
            write(k, (wide ? 647 : 485)) = component10_0;
            const auto component11_0 = read(sourceAdd(k, 1), (wide ? 648 : 486));
            write(sourceAdd(k, 1), (wide ? 648 : 486)) = component11_0;
            const auto component12_0 = read(k, (wide ? 649 : 487));
            write(sourceAdd(k, 2), (wide ? 649 : 487)) = component12_0;
        }
        return;
    }
    case 54:
    case 84:
    {
        // Source conv_rgb2bgra/conv16_rgb2bgra; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 3), t = sourceAdd(t, 4)) {
            const auto component13_0 = read(sourceAdd(k, 2), (wide ? 656 : 494));
            write(t, (wide ? 656 : 494)) = component13_0;
            const auto component14_0 = read(sourceAdd(k, 1), (wide ? 657 : 495));
            write(sourceAdd(t, 1), (wide ? 657 : 495)) = component14_0;
            const auto component15_0 = read(k, (wide ? 658 : 496));
            write(sourceAdd(t, 2), (wide ? 658 : 496)) = component15_0;
            write(sourceAdd(t, 3), (wide ? 659 : 497)) = std::numeric_limits<T>::max();
        }
        return;
    }
    case 65:
    {
        // Source conv_rgba2y/conv16_rgba2y; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 4), t = sourceAdd(t, 1)) {
            const auto component16_0 = read(k, (wide ? 666 : 504));
            const auto component16_1 = read(sourceAdd(k, 1), (wide ? 666 : 504));
            const auto component16_2 = read(sourceAdd(k, 2), (wide ? 666 : 504));
            write(t, (wide ? 666 : 504)) = static_cast<T>(0.21 * component16_0 + 0.64 *  component16_1 + 0.15 *  component16_2);
        }
        return;
    }
    case 66:
    {
        // Source conv_rgba2ya/conv16_rgba2ya; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 4), t = sourceAdd(t, 2)) {
            const auto component17_0 = read(k, (wide ? 672 : 510));
            const auto component17_1 = read(sourceAdd(k, 1), (wide ? 672 : 510));
            const auto component17_2 = read(sourceAdd(k, 2), (wide ? 672 : 510));
            write(t, (wide ? 672 : 510)) = static_cast<T>(0.21 * component17_0 + 0.64 *  component17_1 + 0.15 *  component17_2);
            const auto component18_0 = read(sourceAdd(k, 3), (wide ? 673 : 511));
            write(sourceAdd(t, 1), (wide ? 673 : 511)) = component18_0;
        }
        return;
    }
    case 67:
    {
        // Source conv_rgba2rgb/conv16_rgba2rgb; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 4), t = sourceAdd(t, 3)) {
            const auto input = fromSlice(k, sourceAdd(k, 3), (wide ? 680 : 518));
            auto output = toSlice(t, sourceAdd(t, 3), (wide ? 680 : 518));
            std::memmove(output.data(), input.data(), input.size_bytes());
        }
        return;
    }
    case 69:
    case 99:
    {
        // Source conv_rgba2bgr/conv16_rgba2bgr; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 4), t = sourceAdd(t, 3)) {
            const auto component19_0 = read(sourceAdd(k, 2), (wide ? 686 : 524));
            write(t, (wide ? 686 : 524)) = component19_0;
            const auto component20_0 = read(sourceAdd(k, 1), (wide ? 687 : 525));
            write(sourceAdd(t, 1), (wide ? 687 : 525)) = component20_0;
            const auto component21_0 = read(k, (wide ? 688 : 526));
            write(sourceAdd(t, 2), (wide ? 688 : 526)) = component21_0;
        }
        return;
    }
    case 70:
    case 100:
    {
        // Source conv_rgba2bgra/conv16_rgba2bgra; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 4), t = sourceAdd(t, 4)) {
            const auto component22_0 = read(sourceAdd(k, 2), (wide ? 695 : 533));
            write(t, (wide ? 695 : 533)) = component22_0;
            const auto component23_0 = read(sourceAdd(k, 1), (wide ? 696 : 534));
            write(sourceAdd(t, 1), (wide ? 696 : 534)) = component23_0;
            const auto component24_0 = read(k, (wide ? 697 : 535));
            write(sourceAdd(t, 2), (wide ? 697 : 535)) = component24_0;
            const auto component25_0 = read(sourceAdd(k, 3), (wide ? 698 : 536));
            write(sourceAdd(t, 3), (wide ? 698 : 536)) = component25_0;
        }
        return;
    }
    case 81:
    {
        // Source conv_bgr2y/conv16_bgr2y; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 3), t = sourceAdd(t, 1)) {
            const auto component26_0 = read(sourceAdd(k, 2), (wide ? 705 : 543));
            const auto component26_1 = read(sourceAdd(k, 1), (wide ? 705 : 543));
            const auto component26_2 = read(sourceAdd(k, 1), (wide ? 705 : 543));
            write(t, (wide ? 705 : 543)) = static_cast<T>(0.21 * component26_0 + 0.64 *  component26_1 + 0.15 *  component26_2);
        }
        return;
    }
    case 82:
    {
        // Source conv_bgr2ya/conv16_bgr2ya; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 3), t = sourceAdd(t, 2)) {
            const auto component27_0 = read(sourceAdd(k, 2), (wide ? 711 : 549));
            const auto component27_1 = read(sourceAdd(k, 1), (wide ? 711 : 549));
            const auto component27_2 = read(sourceAdd(k, 1), (wide ? 711 : 549));
            write(t, (wide ? 711 : 549)) = static_cast<T>(0.21 * component27_0 + 0.64 *  component27_1 + 0.15 *  component27_2);
            write(sourceAdd(t, 1), (wide ? 712 : 550)) = std::numeric_limits<T>::max();
        }
        return;
    }
    case 97:
    {
        // Source conv_bgra2y/conv16_bgra2y; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 4), t = sourceAdd(t, 1)) {
            const auto component28_0 = read(sourceAdd(k, 2), (wide ? 719 : 557));
            const auto component28_1 = read(sourceAdd(k, 1), (wide ? 719 : 557));
            const auto component28_2 = read(k, (wide ? 719 : 557));
            write(t, (wide ? 719 : 557)) = static_cast<T>(0.21 * component28_0 + 0.64 *  component28_1 + 0.15 *  component28_2);
        }
        return;
    }
    case 98:
    {
        // Source conv_bgra2ya/conv16_bgra2ya; retain each statement and its failure prefix.
        for (int k = 0, t = 0; static_cast<std::size_t>(k) < source.size();
                k = sourceAdd(k, 4), t = sourceAdd(t, 2)) {
            const auto component29_0 = read(sourceAdd(k, 2), (wide ? 725 : 563));
            const auto component29_1 = read(sourceAdd(k, 1), (wide ? 725 : 563));
            const auto component29_2 = read(k, (wide ? 725 : 563));
            write(t, (wide ? 725 : 563)) = static_cast<T>(0.21 * component29_0 + 0.64 *  component29_1 + 0.15 *  component29_2);
            write(sourceAdd(t, 1), (wide ? 726 : 564)) = std::numeric_limits<T>::max();
        }
        return;
    }
    default:
        compat::ngAssertUnreachable(nullptr, "imagefmt/package.d", 404);
    }
}
void convertPixels(std::span<const std::uint8_t> a, std::span<std::uint8_t> b, int sc, int tc) {
    convert(a, b, sc, tc);
}
void convertPixels(std::span<const std::uint16_t> a, std::span<std::uint16_t> b, int sc, int tc) {
    convert(a, b, sc, tc);
}
static void decode(Reader& reader, Image& image, int channels, int bits) {
    if (detectPng(reader)) image.e = readPng(reader, image, channels, bits);
    else if (detectBmp(reader)) image.e = readBmp(reader, image, channels, bits);
    else if (detectJpeg(reader)) image.e = readJpeg(reader, image, channels, bits);
    else if (detectTga(reader)) image.e = readTga(reader, image, channels, bits);
    else image.e = Error::oddfmt;
}
} // namespace detail
namespace {
int readFile(void* stream,std::uint8_t* target,int count) {
    return std::bit_cast<int>(static_cast<std::uint32_t>(
        std::fread(target,1,static_cast<std::size_t>(count),static_cast<std::FILE*>(stream))));
}
int seekFile(void* stream,int offset) { return std::fseek(static_cast<std::FILE*>(stream),offset,SEEK_CUR); }
}
Image readImage(std::FILE* file,int channels,int bits) {
    return readImage(Read{file,&readFile,&seekFile},channels,bits);
}
Image readImage(std::string_view file,int channels,int bits) {
    Image image;
    detail::NTString terminated(file);
    if(!terminated.data()) { image.e=Error::oom; return image; }
    auto* input=std::fopen(terminated.data(),"rb");
    terminated.drop();
    if(input) {
        image=readImage(input,channels,bits);
        std::fclose(input);
    } else image.e=Error::fopen;
    return image;
}
ImageInfo readInfo(std::FILE* file) { return readInfo(Read{file,&readFile,&seekFile}); }
ImageInfo readInfo(std::string_view file) {
    ImageInfo info;
    detail::NTString terminated(file);
    if(!terminated.data()) { info.e=Error::oom; return info; }
    auto* input=std::fopen(terminated.data(),"rb");
    terminated.drop();
    if(!input) { info.e=Error::fopen; return info; }
    info=readInfo(input);
    std::fclose(input);
    return info;
}
Image readImage(std::span<const std::uint8_t> data, int channels, int bits) {
    Image image; detail::Reader reader; image.e = reader.initialize(data);
    if (image.e == Error::none) detail::decode(reader, image, channels, bits);
    return image;
}
Image readImage(Read io, int channels, int bits) {
    Image image;
    if (!io.stream || !io.read || !io.seek) { image.e = Error::arg; return image; }
    Buffer buffer;
    // Source returns the default image without assigning the local allocation error.
    if (!buffer.sourceAllocate(4096)) return image;
    d_runtime::ngRunWithScopeExit([&] {
        detail::Reader reader; image.e = reader.initialize({buffer.data(), buffer.size()}, io);
        if (image.e == Error::none) detail::decode(reader, image, channels, bits);
    }, [&] { buffer.sourceFree(); });
    return image;
}
ImageInfo readInfo(std::span<const std::uint8_t> data) {
    detail::Reader reader; ImageInfo info; info.e = reader.initialize(data);
    if (info.e != Error::none) return info;
    if (detail::detectPng(reader)) return detail::readPngInfo(reader);
    if (detail::detectBmp(reader)) return detail::readBmpInfo(reader);
    if (detail::detectJpeg(reader)) return detail::readJpegInfo(reader);
    if (detail::detectTga(reader)) return detail::readTgaInfo(reader);
    info.e = Error::oddfmt; return info;
}
ImageInfo readInfo(Read io) {
    std::array<std::uint8_t, 256> buffer{};
    ImageInfo info; detail::Reader reader;
    info.e = reader.initialize(buffer, io);
    if (info.e != Error::none) return info;
    if (detail::detectPng(reader)) return detail::readPngInfo(reader);
    if (detail::detectBmp(reader)) return detail::readBmpInfo(reader);
    if (detail::detectJpeg(reader)) return detail::readJpegInfo(reader);
    if (detail::detectTga(reader)) return detail::readTgaInfo(reader);
    info.e = Error::oddfmt; return info;
}
} // namespace nicxlive::core::imagefmt
