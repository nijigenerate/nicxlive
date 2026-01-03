# nicxlive

`nicxlive` is a straight C++ port of the D-based `nijilive` library. It aims to mirror the original node graph and rendering behavior as closely as possible, while documenting compatibility gaps.

## Dependencies
- CMake 3.16+
- C++17 compiler
- Boost (uses `property_tree`)
- stb_image (bundled at `core/third_party/stb_image.h`)

## Build
```bash
cd nicxlive
cmake -S . -B build
cmake --build build
```

## Progress (D → C++ line counts with compat docs)
| nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| --- | ---:| --- | ---:| --- |
| nijilive/core/nodes/common.d | 290 | core/nodes/common.hpp | (header) | [compat-common](doc/compat-common.md) |
| nijilive/core/nodes/filter.d | 146 | core/nodes/filter.cpp | 65 | [compat-filter](doc/compat-filter.md) |
| nijilive/core/nodes/drawable.d | 740 | core/nodes/drawable.cpp | 512 | [compat-drawable](doc/compat-drawable.md) |
| nijilive/core/nodes/deformable.d | 209 | core/nodes/deformable.cpp | 149 | [compat-deformable](doc/compat-deformable.md) |
| nijilive/core/nodes/part/package.d | 826 | core/nodes/part.cpp | 730 | [compat-part](doc/compat-part.md) |
| nijilive/core/nodes/mask/package.d | 140 | core/nodes/mask.cpp | 49 | [compat-mask](doc/compat-mask.md) |
| nijilive/core/nodes/composite/package.d | 334 | core/nodes/composite.cpp | 195 | [compat-composite](doc/compat-composite.md) |
| nijilive/core/nodes/composite/dcomposite.d | 41 | core/nodes/dynamic_composite.cpp | 15 | [compat-dcomposite](doc/compat-dcomposite.md) |
| nijilive/core/nodes/composite/projectable.d | 1229 | core/nodes/projectable.cpp | 390 | [compat-projectable](doc/compat-projectable.md) |
| nijilive/core/nodes/meshgroup/package.d | 611 | core/nodes/mesh_group.cpp | 545 | [compat-meshgroup](doc/compat-meshgroup.md) |
| nijilive/core/nodes/deformer/grid.d | 774 | core/nodes/grid_deformer.cpp | 620 | [compat-grid_deformer](doc/compat-grid_deformer.md) |
| nijilive/core/nodes/deformer/path.d | 1334 | core/nodes/path_deformer.cpp | 1257 | [compat-path_deformer](doc/compat-path_deformer.md) |
| nijilive/core/nodes/drivers/simplephysics.d | 1003 | core/nodes/phys.cpp | 52 | [compat-deformable](doc/compat-deformable.md) |
| nijilive/core/nodes/package.d | 1466 | core/nodes/node.cpp | 1156 | [compat-node](doc/compat-node.md) |
| nijilive/core/nodes/utils.d | 22 | core/common/utils.hpp | (header) | [compat-common](doc/compat-common.md) |
| nijilive/core/nodes/part/apart.d | 22 | core/nodes/part.cpp | 730 | [compat-part](doc/compat-part.md) |
| nijilive/core/render/shared buffers etc. | — | core/render/common.cpp | 17 | [compat-render-common](doc/compat-render-common.md) |
| nijilive (total) | 10,692 | nicxlive (total) | 6,784 |  |

See `doc/compat-*.md` for detailed compatibility notes and remaining gaps.
