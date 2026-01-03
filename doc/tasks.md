# nicxlive 移植タスク進捗

Status: `[ ]` todo, `[>]` in progress, `[x]` done, `[?]` blocked.

## レンダリング基盤（Projectable互換の前提）
- [ ] R1: RenderGraph 相当の API を実装（dynamicComposite push/pop、applyMask、drawPart などのコマンドキュー）
- [ ] R2: RenderBackend に MaskApply/DynamicComposite begin/end/playback を追加し、RenderCommandEmitter から呼び出せるようにする
- [ ] R3: Offscreen テクスチャ/ステンシルの生成・破棄・再利用（Texture 管理と reuseCachedTextureThisFrame の裏付け）
- [ ] R4: RenderContext への renderGraph/renderBackend 配線と frame 管理（currentDynamicCompositeFrame 等）
- [ ] R5: Projectable/Composite の serialize/deserialize で textureOffset/offsreen 状態・maxBoundsStartFrame など固有フィールドを保存/復元する

## パラメータ / フィルタ基盤（優先）
- [x] P1: Vec2Array/InterpolateMode/gather・scatter など ParameterBinding が依存する utils を core/common に実装。
- [x] P2: ParameterBinding 詳細（Value/Deformation/ParameterParameterBinding の apply/scale/copy/swap/reverseAxis/reInterpolate 等）を D に合わせて移植。
- [x] P3: NodeFilter の `applyDeformToChildren` をフル実装（パラメータバインディングを辿って deform/transform を転送）。

## ノード階層（基底 → 派生）
- [x] 1: `package.d` → `core/nodes/node.hpp/.cpp`（Node/TmpNode, TaskFlag/SerializeFlag/UUID/transform）
- [x] 2: `filter.d` → `core/nodes/filter.hpp`（NodeFilter/Mixin）
- [x] 3: `deformable.d` → `core/nodes/deformable.hpp`
- [>] 4: `drawable.d` → `core/nodes/drawable.hpp/.cpp`（shared buffer / welding / serialization）
- [>] 5: `part/package.d` → `core/nodes/part.hpp`
- [>] 6: `mask/package.d` → `core/nodes/mask.hpp`
- [>] 7: `composite/projectable.d` → `core/nodes/projectable.hpp`
- [>] 8: `composite/package.d` → `core/nodes/composite.hpp`
- [>] 9: `composite/dcomposite.d` → `core/nodes/dynamic_composite.hpp`
- [>] 10: `meshgroup/package.d` → `core/nodes/mesh_group.hpp`（パラメータ連動の deform 反映まで実装済）
- [ ] 11: `deformer/base.d` → `core/nodes/deformer_base.hpp`
- [>] 12: `deformer/grid.d` → `core/nodes/grid_deformer.hpp`
- [>] 13: `deformer/path.d` → `core/nodes/path_deformer.hpp`
- [ ] 14: `drivers/package.d` → `core/nodes/driver.hpp`
- [ ] 15: `drivers/simplephysics.d` → `core/nodes/simple_physics_driver.hpp`

### ノード階層 1-13 行数トラッカー（◯/△/✗ 評価メモ）
| # | D (nijilive) | Lines | C++ (nicxlive) | Lines | 評価 |
| - | --- | ---:| --- | ---:| --- |
| 1 | nodes/package.d | 1466 | core/nodes/node.cpp | 1180 | ◯ |
| 2 | nodes/filter.d | 146 | core/nodes/filter.cpp | 65 | ◯ |
| 3 | nodes/deformable.d | 209 | core/nodes/deformable.cpp | 162 | ◯ |
| 4 | nodes/drawable.d | 740 | core/nodes/drawable.cpp | 551 | △ |
| 5 | nodes/part/package.d | 826 | core/nodes/part.cpp | 730 | △ |
| 6 | nodes/mask/package.d | 140 | core/nodes/mask.cpp | 49 | △ |
| 7 | nodes/composite/projectable.d | 1229 | core/nodes/projectable.cpp | 390 | △ |
| 8 | nodes/composite/package.d | 334 | core/nodes/composite.cpp | 195 | △ |
| 9 | nodes/composite/dcomposite.d | 41 | core/nodes/dynamic_composite.cpp | 15 | △ |
| 10 | nodes/meshgroup/package.d | 611 | core/nodes/mesh_group.cpp | 545 | △ |
| 11 | nodes/deformer/base.d | 16 | core/nodes/deformer_base.hpp | header | △ |
| 12 | nodes/deformer/grid.d | 774 | core/nodes/grid_deformer.cpp | 620 | △ |
| 13 | nodes/deformer/path.d | 1334 | core/nodes/path_deformer.cpp | 1257 | △ |

※ 写経レベルの突合前の暫定値。メソッド/フィールドごとの差分列挙と検証後、◯になるまで更新する。
