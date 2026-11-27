// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of imagefmt 2.1.2. See LICENSE.imagefmt.
#pragma once
#include "writer.hpp"

namespace nicxlive::core::imagefmt::detail {
// Reader and writer observe the same sticky, per-thread source error state.
Error& pngErrorState() noexcept;
} // namespace nicxlive::core::imagefmt::detail
