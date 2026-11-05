// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of the imagefmt 2.1.2 writer closure. See LICENSE.imagefmt.
#include "writer.hpp"
#include "reader.hpp"
#include "nt_string.hpp"
#include <compat/scope_exit.hpp>
#include <compat/array_range_error.hpp>
#include <compat/assert_error.hpp>
#include <array>
#include <bit>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace nicxlive::core::imagefmt {
thread_local Allocator allocator;
thread_local int verticalOrientationWrite = 1;

Buffer::Buffer(Buffer&& other) noexcept
    : sourceLifetime_(other.sourceLifetime_), owner_(other.owner_), data_(other.data_), size_(other.size_) {
    if (!sourceLifetime_) { other.data_ = nullptr; other.size_ = 0; }
}
Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (!sourceLifetime_) reset();
        sourceLifetime_ = other.sourceLifetime_;
        owner_ = other.owner_;
        data_ = other.data_;
        size_ = other.size_;
        if (!sourceLifetime_) { other.data_ = nullptr; other.size_ = 0; }
    }
    return *this;
}
namespace {
bool sourceCustomAllocator() noexcept {
    return allocator.allocate || allocator.resize || allocator.release;
}
}
bool Buffer::sourceAllocate(std::size_t size) {
    sourceLifetime_ = true;
    auto* result = sourceCustomAllocator() ? allocator.allocate(allocator.context, size) : std::malloc(size);
    data_ = static_cast<std::uint8_t*>(result);
    size_ = result ? size : 0;
    return result != nullptr;
}
bool Buffer::sourceReallocate(std::size_t size) {
    sourceLifetime_ = true;
    auto* result = sourceCustomAllocator() ? allocator.resize(allocator.context, data_, size) :
        std::realloc(data_, size);
    if (!result) return false;
    data_ = static_cast<std::uint8_t*>(result);
    size_ = size;
    return true;
}
void Buffer::sourceFree() {
    if (sourceCustomAllocator()) allocator.release(allocator.context, data_);
    else std::free(data_);
    data_ = nullptr;
    size_ = 0;
    sourceLifetime_ = true;
}
bool Buffer::resize(std::size_t size) noexcept {
    if (size == 0) { reset(); return true; }
    void* p = nullptr;
    if (owner_.allocate || owner_.resize || owner_.release) {
        if (!owner_.allocate || !owner_.resize || !owner_.release) return false;
        p = data_ ? owner_.resize(owner_.context, data_, size) : owner_.allocate(owner_.context, size);
    } else {
        p = data_ ? std::realloc(data_, size) : std::malloc(size);
    }
    if (!p) return false;
    data_ = static_cast<std::uint8_t*>(p);
    size_ = size;
    return true;
}
void Buffer::reset() noexcept {
    if (data_) {
        if (owner_.release) owner_.release(owner_.context, data_);
        else std::free(data_);
    }
    data_ = nullptr;
    size_ = 0;
}

namespace detail {
namespace {
int writerInt(std::size_t value) { return std::bit_cast<int>(static_cast<std::uint32_t>(value)); }
int writerAdd(int left, int right) { return std::bit_cast<int>(std::uint32_t(left) + std::uint32_t(right)); }
int writerSubtract(int left, int right) { return std::bit_cast<int>(std::uint32_t(left) - std::uint32_t(right)); }
std::span<std::uint8_t> writerSlice(Buffer& buffer, int lower, int upper, std::size_t line) {
    const auto lo = static_cast<std::size_t>(lower), hi = static_cast<std::size_t>(upper);
    if (lo > hi) throw compat::ArraySliceError(lo, hi, 0,
        compat::RetainedString::fromStaticLiteral("imagefmt/package.d"), line);
    const auto address = reinterpret_cast<std::uintptr_t>(buffer.data()) + lo;
    return {reinterpret_cast<std::uint8_t*>(address), hi - lo};
}
std::span<const std::uint8_t> inputSlice(std::span<const std::uint8_t> input,
        int lower, int upper, std::size_t line) {
    const auto lo = static_cast<std::size_t>(lower), hi = static_cast<std::size_t>(upper);
    if (lo > hi || hi > input.size()) throw compat::ArraySliceError(lo, hi, input.size(),
        compat::RetainedString::fromStaticLiteral("imagefmt/package.d"), line);
    return input.subspan(lo, hi - lo);
}
}
bool Writer::initialize() { return buffer.sourceAllocate(32 * 1024); }
bool Writer::initialize(std::FILE* file) { stream=file; return buffer.sourceAllocate(4096); }
void Writer::weakFlush() {
#ifndef NDEBUG
    if (!stream) throw compat::AssertError(nullptr, "imagefmt/package.d", 982);
#endif
    int offset=0;
    while(offset<used) {
        const auto bytes=writerSlice(buffer,offset,used,985);
        const int written=writerInt(std::fwrite(bytes.data(),1,bytes.size(),stream));
        offset=writerAdd(offset,written);
        if(written==0) {failed=true; return;}
    }
    used=0;
}
Error Writer::fullFlush() {
    if(!stream) return Error::none;
    weakFlush(); failed|=std::fflush(stream)!=0;
    return failed ? Error::stream : Error::none;
}
void Writer::moreSpace() {
    if (stream) weakFlush();
    else if (used == writerInt(buffer.size())) {
        const int newCapacity=writerAdd(writerInt(buffer.size()),writerInt(buffer.size()));
        if (!buffer.sourceReallocate(static_cast<std::size_t>(newCapacity))) failed=true;
    }
}
void Writer::block(std::span<const std::uint8_t> bytes) {
    int available=writerSubtract(writerInt(buffer.size()),used);
    int todo=writerInt(bytes.size());
    const auto put=[&](int first,int count,std::size_t line) {
        const auto source=inputSlice(bytes,first,writerAdd(first,count),line);
        auto target=writerSlice(buffer,used,writerAdd(used,count),line);
        if (!source.empty()) std::memmove(target.data(),source.data(),source.size());
    };
    if (todo<=available) { put(0,todo,1039); used=writerAdd(used,todo); return; }
    int amount=available,first=0;
    do {
        put(first,amount,1046);
        used=writerAdd(used,amount); todo=writerSubtract(todo,amount);
        if (!todo) return;
        first=writerAdd(first,amount); moreSpace();
        available=writerSubtract(writerInt(buffer.size()),used);
        amount=available<todo ? available : todo;
    } while (!failed);
}
void Writer::byte(std::uint8_t value) {
    if (used<writerInt(buffer.size())) {
        const auto offset=used; used=writerAdd(used,1);
        const auto address=reinterpret_cast<std::uintptr_t>(buffer.data()) +
            static_cast<std::uintptr_t>(static_cast<std::intptr_t>(offset));
        *reinterpret_cast<std::uint8_t*>(address)=value;
        return;
    }
    moreSpace();
    if (used<writerInt(buffer.size())) {
        const auto offset=used; used=writerAdd(used,1);
        const auto address=reinterpret_cast<std::uintptr_t>(buffer.data()) +
            static_cast<std::uintptr_t>(static_cast<std::intptr_t>(offset));
        *reinterpret_cast<std::uint8_t*>(address)=value;
    }
}
void Writer::little16(std::uint16_t value) { byte(value & 255); byte(value >> 8); }
void Writer::little32(std::uint32_t value) { little16(value & 65535); little16(value >> 16); }
void convertLine(std::span<const std::uint8_t> source, std::span<std::uint8_t> target,
        int sourceChannels, int targetFormat) {
    convertPixels(source,target,sourceChannels,targetFormat);
}
} // namespace detail

EncodedImage writeImageMemory(int format, int width, int height,
    std::span<const std::uint8_t> data, int requestedChannels) {
    EncodedImage result;
    detail::Writer writer;
    const auto fail=[&]() { writer.buffer.sourceFree(); return std::move(result); };
    if (!writer.initialize()) { result.error=Error::oom; return fail(); }
    switch (format) {
    case bmp: result.error=detail::writeBmp(writer,width,height,data,requestedChannels); break;
    case tga: result.error=detail::writeTga(writer,width,height,data,requestedChannels); break;
    case png: result.error=detail::writePng(writer,width,height,data,requestedChannels); break;
    default: result.error=Error::unsupp; break;
    }
    if (result.error!=Error::none) return fail();
    const int capacity=detail::writerInt(writer.buffer.size());
    const int waste=detail::writerSubtract(capacity,writer.used);
    const int product=std::bit_cast<int>(std::uint32_t(waste)*100U);
    if (product/capacity>20 && !writer.buffer.sourceReallocate(static_cast<std::size_t>(writer.used)))
        return fail();
    result.length=static_cast<std::size_t>(writer.used);
    result.buffer=Buffer::sourceDescriptor(writer.buffer.data(),result.length);
    return result;
}
Error writeImageFile(std::string_view file,int width,int height,
    std::span<const std::uint8_t> data,int requestedChannels) {
    // Source fname2fmt works with signed int lengths and a narrow fopen path.
    const auto dot=file.find_last_of('.');
    if(dot==std::string_view::npos) return Error::unsupp;
    const auto length=file.size()-dot-1;
    if(length<3 || length>4) return Error::unsupp;
    char extension[4]{};
    for(std::size_t i=0;i<length;++i) {
        const auto c=file[dot+1+i]; extension[i]=c>='A' && c<='Z' ? c+'a'-'A' : c;
    }
    const std::string_view ext(extension,length);
    const int format=ext=="bmp" ? bmp : ext=="tga" ? tga : ext=="png" ? png :
        ext=="jpg" || ext=="jpeg" ? jpg : -1;
    if(format<0) return Error::unsupp;
    detail::NTString terminated(file);
    if(!terminated.data()) return Error::oom;
    auto* output=std::fopen(terminated.data(),"wb");
    terminated.drop();
    if(!output) return Error::fopen;
    Error result=Error::none;
    detail::Writer writer;
    if(!writer.initialize(output)) result=Error::oom;
    else d_runtime::ngRunWithScopeExit([&] {
        switch(format) {
        case bmp: result=detail::writeBmp(writer,width,height,data,requestedChannels);break;
        case tga: result=detail::writeTga(writer,width,height,data,requestedChannels);break;
        case png: result=detail::writePng(writer,width,height,data,requestedChannels);break;
        default: result=Error::unsupp;break;
        }
        if(result==Error::none) result=writer.fullFlush();
    },[&] { writer.buffer.sourceFree(); });
    // The source file overload closes only after a normal inner return.
    std::fclose(output);
    return result;
}
} // namespace nicxlive::core::imagefmt
