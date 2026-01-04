#pragma once

#include "../nodes/common.hpp"

#include <cstddef>

namespace nicxlive::core::render {

// D: nijilive.core.render.shared_deform_buffer
void sharedDeformRegister(::nicxlive::core::common::Vec2Array& target, std::size_t* offsetSink);
void sharedDeformUnregister(::nicxlive::core::common::Vec2Array& target);
void sharedDeformResize(::nicxlive::core::common::Vec2Array& target, std::size_t newLength);
std::size_t sharedDeformAtlasStride();
::nicxlive::core::common::Vec2Array& sharedDeformBufferData();
bool sharedDeformBufferDirty();
void sharedDeformMarkDirty();
void sharedDeformMarkUploaded();

void sharedVertexRegister(::nicxlive::core::common::Vec2Array& target, std::size_t* offsetSink);
void sharedVertexUnregister(::nicxlive::core::common::Vec2Array& target);
void sharedVertexResize(::nicxlive::core::common::Vec2Array& target, std::size_t newLength);
std::size_t sharedVertexAtlasStride();
::nicxlive::core::common::Vec2Array& sharedVertexBufferData();
bool sharedVertexBufferDirty();
void sharedVertexMarkDirty();
void sharedVertexMarkUploaded();

void sharedUvRegister(::nicxlive::core::common::Vec2Array& target, std::size_t* offsetSink);
void sharedUvUnregister(::nicxlive::core::common::Vec2Array& target);
void sharedUvResize(::nicxlive::core::common::Vec2Array& target, std::size_t newLength);
std::size_t sharedUvAtlasStride();
::nicxlive::core::common::Vec2Array& sharedUvBufferData();
bool sharedUvBufferDirty();
void sharedUvMarkDirty();
void sharedUvMarkUploaded();

} // namespace nicxlive::core::render

