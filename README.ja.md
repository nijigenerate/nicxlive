# nicxlive

`nicxlive` は D 言語実装の `nijilive` を C++ にストレート移植するライブラリです。ノードグラフ・レンダリング周りの挙動を極力 1:1 で再現しつつ、既存の D 実装との互換性をドキュメント化しています。

## 依存ライブラリ
- CMake 3.16 以降
- C++17 対応コンパイラ
- Boost（`property_tree` を使用）
- stb_image（`core/third_party/stb_image.h` 同梱）

## ビルド方法
```bash
cd nicxlive
cmake -S . -B build
cmake --build build
```

## 開発状況（D 実装と C++ 実装の行数対応）
| nijilive (D) | 行数 | nicxlive (C++) | 行数 | 互換メモ |
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
| nijilive/core/nodes/drivers/simplephysics.d | 1003 | core/nodes/phys.cpp | 52 | [compat-deformer](doc/compat-deformable.md) |
| nijilive/core/nodes/package.d | 1466 | core/nodes/node.cpp | 1156 | [compat-node](doc/compat-node.md) |
| nijilive/core/nodes/utils.d | 22 | core/common/utils.hpp | (header) | [compat-common](doc/compat-common.md) |
| nijilive/core/nodes/part/apart.d | 22 | core/nodes/part.cpp | 730 | [compat-part](doc/compat-part.md) |
| nijilive/core/render/shared buffers 他 | — | core/render/common.cpp | 17 | [compat-render-common](doc/compat-render-common.md) |
| nijilive/（総計） | 10,692 | nicxlive（総計） | 6,784 |  |

詳細な互換性や差分は各 `doc/compat-*.md` を参照してください。
