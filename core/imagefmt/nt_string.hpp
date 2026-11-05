// Copyright 2019 Tero Hänninen. BSD-2-Clause; see LICENSE.imagefmt.
#pragma once
#include "writer.hpp"
#include <array>
#include <cstring>

namespace nicxlive::core::imagefmt::detail {
// One lowering of package.d NTString for the actual narrow stdio entry points.
// drop() is explicit and is intentionally not a scope destructor.
class NTString {
public:
    explicit NTString(std::string_view input) {
        if(input.empty()) pointer_=temporary_.data();
        else if(input.back()==0) pointer_=input.data();
        else if(input.size()<temporary_.size()) {
            std::memcpy(temporary_.data(),input.data(),input.size());
            pointer_=temporary_.data();
        } else {
            if(!allocation_.sourceAllocate(input.size()+1)) return;
            heap_=true;
            std::memcpy(allocation_.data(),input.data(),input.size());
            allocation_.data()[input.size()]=0;
            pointer_=reinterpret_cast<const char*>(allocation_.data());
        }
    }
    const char* data() const noexcept { return pointer_; }
    void drop() {
        if(heap_) {
            auto descriptor=Buffer::sourceDescriptor(
                reinterpret_cast<std::uint8_t*>(const_cast<char*>(pointer_)),allocation_.size());
            descriptor.sourceFree();
        }
    }
private:
    std::array<char,255> temporary_{};
    Buffer allocation_=Buffer::sourceDescriptor(nullptr,0);
    const char* pointer_{};
    bool heap_{};
};
} // namespace nicxlive::core::imagefmt::detail
