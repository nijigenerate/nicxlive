// Phobos std.internal.cstring, Copyright Denis Shelomovskij 2013-2014.
// BSL-1.0; see licenses/phobos-cstring-source.txt.
#pragma once
#include "static_error.hpp"
#include <common/unicode_lowercase.hpp>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>

namespace nicxlive::core::d_runtime::phobos {
// The actual const(char)[] input branches shared by the Phobos consumers.
template<class Char> class TempCStringBuffer {
    static_assert(std::is_same_v<Char,char> || sizeof(Char)==2);
public:
    explicit TempCStringBuffer(std::string_view source) : null_(source.data()==nullptr) {
        if(null_) return;
        if constexpr(std::is_same_v<Char,char>) {
            if(source.size()>=capacity_) allocate(source.size()+1);
            if(!source.empty()) std::memcpy(data(),source.data(),source.size());
            length_=source.size();
        } else {
            nicxlive::core::common::unicode::ReplacementUtf8Cursor cursor(source);
            while(!cursor.empty()) {
                auto scalar=cursor.take();
                if(scalar<=0xffff) append(static_cast<Char>(scalar),source.size());
                else {
                    scalar-=0x10000;
                    append(static_cast<Char>(0xd800+(scalar>>10)),source.size());
                    append(static_cast<Char>(0xdc00+(scalar&0x3ff)),source.size());
                }
            }
        }
        data()[length_]=Char{};
    }
    TempCStringBuffer(const TempCStringBuffer&)=delete;
    TempCStringBuffer& operator=(const TempCStringBuffer&)=delete;
    ~TempCStringBuffer() {
        // A null source also takes the original non-stack pureFree branch.
        if(null_) std::free(nullptr);
    }
    const Char* c_str() const noexcept {return null_?nullptr:heap_?heap_.get():inline_.data();}
private:
    struct Free {void operator()(Char* value) const noexcept {std::free(value);}};
    Char* data() noexcept {return heap_?heap_.get():inline_.data();}
    template<std::size_t N> [[noreturn]] static void allocationFailure(
            const char (&file)[N],std::size_t line) {
        throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral(file),line);
    }
    void allocate(std::size_t capacity) {
        auto* replacement=static_cast<Char*>(std::malloc(capacity*sizeof(Char)));
        if(!replacement) allocationFailure("std/internal/memory.d",32);
        heap_.reset(replacement);
        capacity_=capacity;
    }
    void append(Char unit,std::size_t inputLength) {
        if(length_+1==capacity_) {
            auto capacity=capacity_*3/2;
            if(!heap_) {
                if(capacity<=inputLength) capacity=inputLength+1;
                const auto previousCapacity=capacity_;
                allocate(capacity);
                std::memcpy(heap_.get(),inline_.data(),previousCapacity*sizeof(Char));
            } else {
                if(capacity_>=std::numeric_limits<std::size_t>::max()/(2*sizeof(Char)))
                    allocationFailure("std/internal/cstring.d",287);
                auto* replacement=static_cast<Char*>(std::realloc(heap_.get(),capacity*sizeof(Char)));
                if(!replacement) allocationFailure("std/internal/memory.d",48);
                (void)heap_.release();
                heap_.reset(replacement);
                capacity_=capacity;
            }
        }
        data()[length_++]=unit;
    }
    std::array<Char,256/sizeof(Char)> inline_;
    std::unique_ptr<Char,Free> heap_;
    std::size_t capacity_=inline_.size();
    std::size_t length_=0;
    bool null_;
};
} // namespace nicxlive::core::d_runtime::phobos
