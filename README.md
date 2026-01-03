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
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| 1 | nodes/package.d | 1466 | core/nodes/node.cpp | 1180 | [compat-node](doc/compat-node.md) |
| 2 | nodes/filter.d | 146 | core/nodes/filter.cpp | 65 | [compat-filter](doc/compat-filter.md) |
| 3 | nodes/deformable.d | 209 | core/nodes/deformable.cpp | 162 | [compat-deformable](doc/compat-deformable.md) |
| 4 | nodes/drawable.d | 740 | core/nodes/drawable.cpp | 829 | [compat-drawable](doc/compat-drawable.md) |
| 5 | nodes/part/package.d | 826 | core/nodes/part.cpp | 715 | [compat-part](doc/compat-part.md) |
| 6 | nodes/mask/package.d | 140 | core/nodes/mask.cpp | 49 | [compat-mask](doc/compat-mask.md) |
| 7 | nodes/composite/projectable.d | 1229 | core/nodes/projectable.cpp | 779 | [compat-projectable](doc/compat-projectable.md) |
| 8 | nodes/composite/package.d | 334 | core/nodes/composite.cpp | 229 | [compat-composite](doc/compat-composite.md) |
| 9 | nodes/composite/dcomposite.d | 41 | core/nodes/dynamic_composite.cpp | 15 | [compat-dcomposite](doc/compat-dcomposite.md) |
| 10 | nodes/meshgroup/package.d | 611 | core/nodes/mesh_group.cpp | 562 | [compat-meshgroup](doc/compat-meshgroup.md) |
| 11 | nodes/deformer/base.d | 16 | core/nodes/deformer_base.hpp | 33 | [compat-deformable](doc/compat-deformable.md) |
| 12 | nodes/deformer/grid.d | 774 | core/nodes/grid_deformer.cpp | 620 | [compat-grid_deformer](doc/compat-grid_deformer.md) |
| 13 | nodes/deformer/path.d | 1334 | core/nodes/path_deformer.cpp | 1279 | [compat-path_deformer](doc/compat-path_deformer.md) |
| 14 | nodes/drivers/package.d | 58 | core/nodes/driver.hpp | 21 | (WIP) |
| 15 | nodes/drivers/simplephysics.d | 1003 | core/nodes/simple_physics_driver.cpp | 104 | (WIP) |
| nijilive (total) | 10,692 | nicxlive (total) | — |  |

See `doc/compat-*.md` for detailed compatibility notes and remaining gaps.
