# nicxlive 移植タスク進捗

Status: `[ ]` todo, `[>]` in progress, `[x]` done, `[?]` blocked.

## レンダリング基盤（Projectable互換の前提）
- [x] R1: RenderGraph 相当の API を実装（dynamicComposite push/pop、applyMask、drawPart などのコマンドキュー）
- [x] R2: RenderBackend に MaskApply/DynamicComposite begin/end/playback を追加し、RenderCommandEmitter から呼び出せるようにする
- [x] R3: Offscreen テクスチャ/ステンシルの生成・破棄・再利用（Texture 管理と reuseCachedTextureThisFrame の裏付け）
- [x] R4: RenderContext への renderGraph/renderBackend 配線と frame 管理（currentDynamicCompositeFrame 等）
- [x] R5: Projectable/Composite の serialize/deserialize で textureOffset/オフスクリーン状態/maxBoundsStartFrame など固有フィールドを保存/復元する（D版写経。auto_resized だけでなく offscreenSurface/textureOffset/useMaxChildrenBounds/deferredChanged 等も含む）
- [x] R6: Render queue フレームワークの移植（`render/commands.d`, `command_emitter.d`, `graph_builder.d`, `scheduler.d`, `backends/queue/package.d` を C++ に写経）
  - [x] Q1: render/commands の各コマンド構造体と enum を C++ に写経（DrawPart/ApplyMask/Begin/EndDynamicComposite など）
  - [x] Q2: RenderCommandEmitter を写経（コマンド構築・キュー投入・frame begin/end 管理）
  - [x] Q3: RenderGraphBuilder/RenderScheduler を写経し、queue backend にフック（frame 内の command list 構築）
  - [x] Q4: backend_queue を写経（queue 再生、render backend 呼び出し、resource handle 管理）
  - [x] Q5: immediate/render/profiler 等、queue依存の補助モジュールを必要に応じて写経し統合
    - [x] render/profiler: D 同等のスコープ計測＋1s毎のレポート
    - [x] render/immediate: inDrawTexture* 即時描画ラッパーを追加（Backend API 拡張）
    - [x] render/passes: RenderPassKind/RenderScopeHint を render_pass.hpp に写経済み（GraphBuilder で使用）
    - [x] render/shared_deform_buffer: SharedVecAtlas ベースの共有バッファ API を写経追加し Drawable/Part/Mask で利用開始
    - [x] Backend: differenceAggregation 系 API（DifferenceEvaluationRegion/Result）を RenderBackend/QueueBackend に追加（評価は未実装）
  - [x] File layout (D準拠で作成/分割): `core/render/commands.hpp/.cpp`, `command_emitter.hpp/.cpp`, `graph_builder.hpp/.cpp`, `scheduler.hpp/.cpp`, `backend_queue.hpp/.cpp`, `immediate.hpp/.cpp`, `profiler.hpp/.cpp`

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
- [x] 7: `composite/projectable.d` → `core/nodes/projectable.hpp`
- [x] 8: `composite/package.d` → `core/nodes/composite.hpp`
- [x] 9: `composite/dcomposite.d` → `core/nodes/dynamic_composite.hpp`
- [x] 10: `meshgroup/package.d` → `core/nodes/mesh_group.hpp`（パラメータ連動の deform 反映まで実装済）
- [x] 11: `deformer/base.d` → `core/nodes/deformer_base.hpp`
- [x] 12: `deformer/grid.d` → `core/nodes/grid_deformer.hpp`
- [>] 13: `deformer/path.d` → `core/nodes/path_deformer.hpp`
- [x] 14: `drivers/package.d` → `core/nodes/driver.hpp`
- [x] 15: `drivers/simplephysics.d` → `core/nodes/simple_physics_driver.hpp/.cpp`

### PathDeformer 追加タスク（写経・基盤整備）
- [x] PD1: driver シリアライズ/デシリアライズを D 同等に実装（physics ノードに type＋state を保存/復元）
- [x] PD2: diagnostics を D 同等に実装（begin/endDiagnosticFrame、invalid スナップショット／連続 invalid ログ）
- [x] PD3: deformChildren の最近点算出・tangent/normal 正規化・safeInverse のフォールバックを D と一致させる
- [x] PD4: applyDeformToChildren の診断フローを D に揃える（begin/endDiagnosticFrame、rebuffer 呼び出し位置など）

### ノード階層 1-13 行数トラッカー（◯/△/✗ 評価メモ）
| # | D (nijilive) | Lines | C++ (nicxlive) | Lines | 評価 |
| - | --- | ---:| --- | ---:| --- |
| 1 | nodes/package.d | 1466 | core/nodes/node.cpp | 1180 | ◯ |
| 2 | nodes/filter.d | 146 | core/nodes/filter.cpp | 65 | ◯ |
| 3 | nodes/deformable.d | 209 | core/nodes/deformable.cpp | 162 | ◯ |
| 4 | nodes/drawable.d | 740 | core/nodes/drawable.cpp | 829 | ◯ |
| 5 | nodes/part/package.d | 826 | core/nodes/part.cpp | 715 | ◯ |
| 6 | nodes/mask/package.d | 140 | core/nodes/mask.cpp | 49 | ◯ |
| 7 | nodes/composite/projectable.d | 1229 | core/nodes/projectable.cpp | 779 | ◯ |
| 8 | nodes/composite/package.d | 334 | core/nodes/composite.cpp | 229 | ◯ |
| 9 | nodes/composite/dcomposite.d | 41 | core/nodes/dynamic_composite.cpp | 15 | ◯ |
| 10 | nodes/meshgroup/package.d | 611 | core/nodes/mesh_group.cpp | 562 | ◯ |
| 11 | nodes/deformer/base.d | 16 | core/nodes/deformer_base.hpp | 33 | ◯ |
| 12 | nodes/deformer/grid.d | 774 | core/nodes/grid_deformer.cpp | 620 | ◯ |
| 13 | nodes/deformer/path.d | 1334 | core/nodes/path_deformer.cpp | 1279 | △ |
| 14 | nodes/drivers/package.d | 58 | core/nodes/driver.hpp | 39 | ◯ |
| 15 | nodes/drivers/simplephysics.d | 1003 | core/nodes/simple_physics_driver.cpp | 781 | ◯ |

※ 写経レベルの突合前の暫定値。メソッド/フィールドごとの差分列挙と検証後、◯になるまで更新する。

### レンダリング層 行数トラッカー（◯/△/✗ 評価メモ）
| # | D (nijilive) | Lines | C++ (nicxlive) | Lines | 評価 |
| - | --- | ---:| --- | ---:| --- |
| RQ1 | render/commands.d | 210 | core/render/commands.cpp | 92 | ◯ |
| RQ2 | render/command_emitter.d | 25 | core/render/command_emitter.cpp | 126 | ◯ |
| RQ3 | render/graph_builder.d | 262 | core/render/graph_builder.cpp | 213 | ◯ |
| RQ4 | render/scheduler.d | 98 | core/render/scheduler.cpp | 41 | ◯ |
| RQ5 | render/shared_deform_buffer.d | 234 | core/render/shared_deform_buffer.cpp | 134 | ◯ |
| RQ6 | render/immediate.d | 42 | core/render/immediate.cpp | 28 | ◯ |
| RQ7 | render/passes.d | 39 | core/render/render_pass.hpp | 20 | ◯ |
| RQ8 | render/profiler.d | 111 | core/render/profiler.cpp | 108 | ◯ |
| RQ9 | render/backends/queue/package.d | 393 | core/render/backend_queue.cpp | 148 | △ |
| RQ10 | render/backends/opengl/* | 4150 | （未移植） | 0 | ✗ |
| RQ11 | render/backends/directx12/* | 2915 | （未移植） | 0 | ✗ |

※ バックエンドは queue のみ移植済み。OpenGL/DX12 系は未着手のため ✗。人数の差は写経範囲の差異を示す。
