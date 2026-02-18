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
- [>] R7: 2026-02 upstream 構成差分の追従（ファイル名レベル）
  - [ ] Q6: `core/render/backends/package.d` 相当の backend 選択/初期化層を `core/render/common.*`・`core/runtime_state.*`・`core/unity_native.*` に整理して明文化する
  - [ ] Q7: `core/render/tests/render_queue.d` 相当の回帰テスト（queue command 並び順、mask/dynamic composite）を nicxlive 側で追加する
  - [ ] U5: `integration/package.d` 相当の公開 API 集約レイヤを `core/unity_native.*` 周辺で定義する

## パラメータ / フィルタ基盤（優先）
- [x] P1: Vec2Array/InterpolateMode/gather・scatter など ParameterBinding が依存する utils を core/common に実装。
- [x] P2: ParameterBinding 詳細（Value/Deformation/ParameterParameterBinding の apply/scale/copy/swap/reverseAxis/reInterpolate 等）を D に合わせて移植。
- [x] P3: NodeFilter の `applyDeformToChildren` をフル実装（パラメータバインディングを辿って deform/transform を転送）。

## シリアライズ/フォーマット基盤（Unity DLL 前提）
- [>] F1: `fmt/package.d`, `fmt/io.d`, `fmt/serialize.d`, `fmt/binfmt.d` を写経し、C++の fmt モジュールを追加（InochiSerializer/Fghj 等を提供）【ヘッダ追加済み、INP テクスチャの TGA/PNG エンコードは未実装】
- [>] F2: fmt 基盤を使って Node/Puppet/Texture 等のシリアライズ経路を D 版と一致させる（ファイル読み書きは Unity 側バッファ渡しを前提。INP テクスチャセクション/EXT セクションは実装済、TGAのみ対応で PNG/BC7 未対応）

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
| RQ1 | render/commands.d | 162 | core/render/commands.cpp | 99 | ◯ |
| RQ2 | render/command_emitter.d | 23 | core/render/command_emitter.cpp | 197 | ◯ |
| RQ3 | render/graph_builder.d | 230 | core/render/graph_builder.cpp | 185 | ◯ |
| RQ4 | render/scheduler.d | 84 | core/render/scheduler.cpp | 35 | ◯ |
| RQ5 | render/shared_deform_buffer.d | 200 | core/render/shared_deform_buffer.cpp | 120 | ◯ |
| RQ6 | render/immediate.d | 38 | core/render/immediate.cpp | 22 | ◯ |
| RQ7 | render/passes.d | 34 | core/render/render_pass.hpp | 14 | ◯ |
| RQ8 | render/profiler.d | 99 | core/render/profiler.cpp | 93 | ◯ |
| RQ9 | render/backends/queue/package.d | 422 | core/render/backend_queue.cpp | 214 | △ |
| RQ9b | render/backends/package.d | 182 | （未移植） | 0 | ✗ |
| RQ9c | render/tests/render_queue.d | 345 | （未移植） | 0 | ✗ |
| RQ10 | render/backends/opengl/* | 4150 | （未移植） | 0 | ✗ |
| RQ11 | render/backends/directx12/* | 2915 | （未移植） | 0 | ✗ |

### fmt 層 行数トラッカー（◯/△/✗ 評価メモ）※上記 F1/F2 タスク対応
| Task ID | D モジュール | Lines | C++ モジュール | Lines | 評価 |
| - | --- | ---:| --- | ---:| --- |
| F1 (fmt/package) | fmt/package.d | 37 | fmt/fmt.hpp | 73 | △ |
| F1 (fmt/io) | fmt/io.d | 118 | fmt/io.hpp | 44 | ◯ |
| F1 (fmt/serialize) | fmt/serialize.d | 185 | fmt/serialize.hpp | 48 | ◯ |
| F1 (fmt/binfmt) | fmt/binfmt.d | 81 | fmt/binfmt.hpp | 37 | ◯ |

※ バックエンドは queue のみ移植済み。OpenGL/DX12 系は未着手のため ✗。人数の差は写経範囲の差異を示す。

### Unity Native DLL exports（DllImport 参照）
- [x] U1: `njgCreateRenderer`/`njgDestroyRenderer`/`njgFlushCommandBuffer`/`njgGetGcHeapSize`/`njgGetTextureStats` の C API を `extern "C"` で実装し、構造体 packing 含め D 版と一致させる（`unity-managed/Interop/NijiliveNative.cs` に準拠）※ GC サイズは malloc 統計の概算
- [x] U2: `njgLoadPuppet`/`njgUnloadPuppet`/`njgBeginFrame`/`njgTickPuppet` を写経ベースで C API 化（`njgLoadPuppetFromMemory`/`njgWritePuppetToMemory`/`njgFreeBuffer` は D に存在しないため削除）
- [x] U3: `njgEmitCommands`/`njgGetSharedBuffers` で queue/backend のコマンドと共有バッファスナップショットを返す C API を実装（SharedBufferSnapshot に頂点/UV/deform のポインタ＋長さを詰める）
- [x] U4: `njgGetParameters`/`njgUpdateParameters` のパラメータ列挙・更新 API を写経し、構造体サイズ/packing を D 版に揃える（P/Invoke バッファ経由）
- [ ] U5: `integration/package.d` のエントリ集約方針を C++ 側に反映（Unity Native API の公開境界を `core/unity_native.*` で明示化）

## compat 差分由来の実装タスク（全件）

判定基準: D実装を正とし、`✗（未実装）`は実装タスク、`✗（削除候補）`は削除またはD側準拠に寄せるタスク、`△`は挙動差分解消タスクとして扱う。

Status: `[ ]` todo, `[>]` in progress, `[x]` done, `[?]` blocked.

| Status | Task ID | 種別 | 参照 | 対象項目 | 実装方針 |
| --- | --- | --- | --- | --- | --- |
| [x] | CP001 | missing_impl | `compat-binding.md` | `deserializeFromFghj` | D実装をC++へ移植（未実装の解消） |
| [x] | CP002 | missing_impl | `compat-binding.md` | `finalize` | D実装をC++へ移植（未実装の解消） |
| [x] | CP003 | behavior_diff | `compat-binding.md` | `getNodeUUID` | D実装の挙動に合わせて差分を解消 |
| [x] | CP004 | behavior_diff | `compat-binding.md` | `isCompatibleWithNode` | D実装の挙動に合わせて差分を解消 |
| [x] | CP005 | missing_impl | `compat-binding.md` | `reconstruct` | D実装をC++へ移植（未実装の解消） |
| [x] | CP006 | missing_impl | `compat-binding.md` | `serializeSelf` | D実装をC++へ移植（未実装の解消） |
| [x] | CP007 | behavior_diff | `compat-binding.md` | フィールド `BindTarget.target` | D実装の挙動に合わせて差分を解消 |
| [x] | CP008 | behavior_diff | `compat-command_emitter.md` | `beginMask(bool useStencil)` | D実装の挙動に合わせて差分を解消 |
| [x] | CP009 | behavior_diff | `compat-command_emitter.md` | `drawPart(Part, bool)` | D実装の挙動に合わせて差分を解消 |
| [x] | CP010 | behavior_diff | `compat-command_emitter.md` | `endMask()` | D実装の挙動に合わせて差分を解消 |
| [x] | CP011 | remove_candidate | `compat-command_emitter.md` | `QueueCommandEmitter::backendQueue()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP012 | remove_candidate | `compat-command_emitter.md` | `QueueCommandEmitter::playback(RenderBackend*)` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP013 | remove_candidate | `compat-command_emitter.md` | `QueueCommandEmitter::queue() const` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP014 | remove_candidate | `compat-command_emitter.md` | `QueueCommandEmitter::record(...)` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP015 | remove_candidate | `compat-command_emitter.md` | `QueueCommandEmitter::recorded() const` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP016 | remove_candidate | `compat-command_emitter.md` | `RenderQueue::ready()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP017 | behavior_diff | `compat-command_emitter.md` | フィールド `QueueCommandEmitter.activeBackend_` | D実装の挙動に合わせて差分を解消 |
| [x] | CP018 | behavior_diff | `compat-command_emitter.md` | フィールド `RenderQueue.activeBackend_` | D実装の挙動に合わせて差分を解消 |
| [x] | CP019 | behavior_diff | `compat-command_emitter.md` | フィールド `RenderQueue.frameState_` | D実装の挙動に合わせて差分を解消 |
| [x] | CP020 | behavior_diff | `compat-fmt.md` | `inIsINPMode` | D実装の挙動に合わせて差分を解消 |
| [x] | CP021 | missing_impl | `compat-fmt.md` | `inLoadPuppet(string file)` | D実装をC++へ移植（未実装の解消） |
| [x] | CP022 | behavior_diff | `compat-fmt.md` | `inWriteINPPuppet(Puppet, string file)` | D実装の挙動に合わせて差分を解消 |
| [x] | CP023 | behavior_diff | `compat-fmt.md` | `inWriteINPPuppetMemory(Puppet)` | D実装の挙動に合わせて差分を解消 |
| [x] | CP024 | behavior_diff | `compat-path_deformer.md` | フィールド `diagnostics` | D実装の挙動に合わせて差分を解消 |
| [x] | CP025 | remove_candidate | `compat-render-common.md` | class `RenderCommandEmitter.playback` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP026 | missing_impl | `compat-runtime_state.md` | currentRenderBackend | D実装をC++へ移植（未実装の解消） |
| [x] | CP027 | missing_impl | `compat-runtime_state.md` | difference aggregation API | D実装をC++へ移植（未実装の解消） |
| [x] | CP028 | missing_impl | `compat-runtime_state.md` | inDumpViewport | D実装をC++へ移植（未実装の解消） |
| [x] | CP029 | missing_impl | `compat-runtime_state.md` | inEnsureCameraStackForTests | D実装をC++へ移植（未実装の解消） |
| [x] | CP030 | missing_impl | `compat-runtime_state.md` | inEnsureViewportForTests | D実装をC++へ移植（未実装の解消） |
| [x] | CP031 | missing_impl | `compat-runtime_state.md` | inGetClearColor | D実装をC++へ移植（未実装の解消） |
| [x] | CP032 | behavior_diff | `compat-runtime_state.md` | inGetViewport | D実装の挙動に合わせて差分を解消 |
| [x] | CP033 | missing_impl | `compat-runtime_state.md` | initRenderer | D実装をC++へ移植（未実装の解消） |
| [x] | CP034 | missing_impl | `compat-runtime_state.md` | initRendererCommon | D実装をC++へ移植（未実装の解消） |
| [x] | CP035 | missing_impl | `compat-runtime_state.md` | inSetClearColor | D実装をC++へ移植（未実装の解消） |
| [x] | CP036 | behavior_diff | `compat-runtime_state.md` | inSetViewport | D実装の挙動に合わせて差分を解消 |
| [x] | CP037 | missing_impl | `compat-runtime_state.md` | inViewportDataLength | D実装をC++へ移植（未実装の解消） |
| [x] | CP038 | missing_impl | `compat-runtime_state.md` | render target handles (inGetRenderImage 等) | D実装をC++へ移植（未実装の解消） |
| [x] | CP039 | missing_impl | `compat-runtime_state.md` | requireRenderBackend | D実装をC++へ移植（未実装の解消） |
| [x] | CP040 | missing_impl | `compat-runtime_state.md` | tryRenderBackend | D実装をC++へ移植（未実装の解消） |
| [x] | CP041 | remove_candidate | `compat-scheduler.md` | `RenderContext.frameId` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP042 | behavior_diff | `compat-scheduler.md` | `RenderContext.renderBackend` | D実装の挙動に合わせて差分を解消 |
| [x] | CP043 | behavior_diff | `compat-serialize.md` | IDeserializable | D実装の挙動に合わせて差分を解消 |
| [x] | CP044 | behavior_diff | `compat-serialize.md` | Ignore | D実装の挙動に合わせて差分を解消 |
| [x] | CP045 | behavior_diff | `compat-serialize.md` | ISerializable | D実装の挙動に合わせて差分を解消 |
| [x] | CP046 | behavior_diff | `compat-serialize.md` | Name | D実装の挙動に合わせて差分を解消 |
| [x] | CP047 | behavior_diff | `compat-serialize.md` | Optional | D実装の挙動に合わせて差分を解消 |
| [x] | CP048 | missing_impl | `compat-texture.md` | lock | D実装をC++へ移植（未実装の解消） |
| [x] | CP049 | missing_impl | `compat-texture.md` | unlock | D実装をC++へ移植（未実装の解消） |
| [x] | CP050 | behavior_diff | `compat-timing.md` | `inInit(timeFunc)` | D実装の挙動に合わせて差分を解消 |
| [x] | CP051 | behavior_diff | `compat-transform.md` | `calcOffset(Transform other)` | D実装の挙動に合わせて差分を解消 |
| [x] | CP052 | missing_impl | `compat-transform.md` | `toString()` | D実装をC++へ移植（未実装の解消） |
| [x] | CP053 | missing_impl | `compat-transform.md` | deserializeFromFghj() | D実装をC++へ移植（未実装の解消） |
| [x] | CP054 | missing_impl | `compat-transform.md` | serialize() | D実装をC++へ移植（未実装の解消） |
| [x] | CP055 | behavior_diff | `compat-transform.md` | フィールド `scale` | D実装の挙動に合わせて差分を解消 |
| [x] | CP056 | missing_impl | `compat-triangle.md` | `calcOffsetInTriangleCoords(vec2, MeshData&, int[] triangle)` | D実装をC++へ移植（未実装の解消） |
| [x] | CP057 | missing_impl | `compat-triangle.md` | `findSurroundingTriangle(vec2, MeshData&)` | D実装をC++へ移植（未実装の解消） |
| [x] | CP058 | missing_impl | `compat-triangle.md` | `isPointInTriangle(vec2, Vec2Array)` | D実装をC++へ移植（未実装の解消） |
| [x] | CP059 | missing_impl | `compat-triangle.md` | `nlCalculateTransformInTriangle(Vec2Array, int[] triangle, Vec2Array deform, vec2 target, out vec2 target_prime, out float rotVert, out float rotHorz)` | D実装をC++へ移植（未実装の解消） |
| [x] | CP060 | missing_impl | `compat-triangle.md` | `private: applyAffineTransform(mat3, vec2)` | D実装をC++へ移植（未実装の解消） |
| [x] | CP061 | missing_impl | `compat-triangle.md` | `private: calculateAffineTransform(Vec2Array, int[] triangle, Vec2Array deform)` | D実装をC++へ移植（未実装の解消） |
| [x] | CP062 | missing_impl | `compat-triangle.md` | `private: calculateAngle(vec2, vec2)` | D実装をC++へ移植（未実装の解消） |
| [x] | CP063 | behavior_diff | `compat-unity_native.md` | `njgFlushCommandBuffer` | D実装の挙動に合わせて差分を解消 |
| [x] | CP064 | remove_candidate | `compat-unity_native.md` | `njgFreeBuffer` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP065 | behavior_diff | `compat-unity_native.md` | `njgGetGcHeapSize` | D実装の挙動に合わせて差分を解消 |
| [x] | CP066 | missing_impl | `compat-unity_native.md` | `njgGetPuppetExtData` | D実装をC++へ移植（未実装の解消） |
| [x] | CP067 | missing_impl | `compat-unity_native.md` | `njgGetRenderTargets` | D実装をC++へ移植（未実装の解消） |
| [x] | CP068 | missing_impl | `compat-unity_native.md` | `njgGetSharedBuffers` | D実装をC++へ移植（未実装の解消） |
| [x] | CP069 | remove_candidate | `compat-unity_native.md` | `njgLoadPuppetFromMemory` | C++独自実装を削除、またはD側に寄せて一本化 |
| [>] | CP070 | missing_impl | `compat-unity_native.md` | `njgPauseAnimation` | D実装をC++へ移植（未実装の解消） |
| [>] | CP071 | missing_impl | `compat-unity_native.md` | `njgPlayAnimation` | D実装をC++へ移植（未実装の解消） |
| [x] | CP072 | missing_impl | `compat-unity_native.md` | `NjgRenderTargets` | D実装をC++へ移植（未実装の解消） |
| [x] | CP073 | missing_impl | `compat-unity_native.md` | `njgRuntimeInit` | D実装をC++へ移植（未実装の解消） |
| [x] | CP074 | missing_impl | `compat-unity_native.md` | `njgRuntimeTerm` | D実装をC++へ移植（未実装の解消） |
| [>] | CP075 | missing_impl | `compat-unity_native.md` | `njgSeekAnimation` | D実装をC++へ移植（未実装の解消） |
| [x] | CP076 | missing_impl | `compat-unity_native.md` | `njgSetLogCallback` | D実装をC++へ移植（未実装の解消） |
| [x] | CP077 | missing_impl | `compat-unity_native.md` | `njgSetPuppetScale` | D実装をC++へ移植（未実装の解消） |
| [x] | CP078 | missing_impl | `compat-unity_native.md` | `njgSetPuppetTranslation` | D実装をC++へ移植（未実装の解消） |
| [>] | CP079 | missing_impl | `compat-unity_native.md` | `njgStopAnimation` | D実装をC++へ移植（未実装の解消） |
| [x] | CP080 | remove_candidate | `compat-unity_native.md` | `njgWritePuppetToMemory` | C++独自実装を削除、またはD側に寄せて一本化 |
| [>] | CP081 | missing_impl | `compat-vec2array.md` | SIMD対応 (`applySIMD` 等) | D実装をC++へ移植（未実装の解消） |
| [x] | CP082 | behavior_diff | `compat-vec2array.md` | フィールド `alignment` | D実装の挙動に合わせて差分を解消 |
| [x] | CP083 | behavior_diff | `compat-vec2array.md` | フィールド `backing` | D実装の挙動に合わせて差分を解消 |
| [x] | CP084 | behavior_diff | `compat-vec2array.md` | フィールド `lanes` (x,y SoA) | D実装の挙動に合わせて差分を解消 |
| [x] | CP085 | missing_impl | `compat-vec2array.md` | 単体テスト | D実装をC++へ移植（未実装の解消） |
| [x] | CP086 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::playback(RenderBackend*)` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP087 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::recorded()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP088 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::backendQueue()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP089 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::backendResourceQueue()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP090 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::clearResourceQueue()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP091 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::renderTarget()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP092 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::compositeTarget()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP093 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::sharedVerticesRaw()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP094 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::sharedUvRaw()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP095 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::sharedDeformRaw()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP096 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::sharedVertexCount()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP097 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::sharedUvCount()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP098 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::sharedDeformCount()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP099 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::sharedVerticesData()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP100 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::sharedUvData()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP101 | remove_candidate | `compat-queue.md` | `QueueRenderBackend::sharedDeformData()` | C++独自実装を削除、またはD側に寄せて一本化 |
| [x] | CP102 | behavior_diff | `compat-unity_native.md` | `njgTickPuppet` ABI (`float` vs `double`) | D準拠で `double deltaSeconds` に修正 |
| [x] | CP103 | behavior_diff | `compat-unity_native.md` | `njgEmitCommands` ABI (`SharedBufferSnapshot* + const CommandQueueView**` vs `CommandQueueView*`) | D準拠で `CommandQueueView* outView` に修正し、shared は `njgGetSharedBuffers` へ分離 |
| [x] | CP104 | behavior_diff | `compat-unity_native.md` | `NjgRenderCommandKind` 列挙値順（`DrawMask` 欠落） | `DrawMask/BeginComposite/DrawCompositeQuad/EndComposite` を含む D 準拠順へ修正 |
| [x] | CP105 | behavior_diff | `compat-unity_native.md` | `NjgPartDrawPacket` レイアウト（`renderMatrix`/`renderRotation` 欠落） | D 準拠レイアウトへ修正、queue 詰め替えを更新 |
| [x] | CP106 | behavior_diff | `compat-unity_native.md` | `NjgDynamicCompositePass` レイアウト（`autoScaled`/`drawBufferCount`/`hasStencil` 欠落） | D 準拠レイアウトへ修正、queue 詰め替えを更新 |
| [x] | CP107 | behavior_diff | `compat-unity_native.md` | `CommandQueueView.commands` 型不一致（`const void*`） | D 準拠で `const NjgQueuedCommand*` へ修正 |
| [x] | CP109 | behavior_diff | `compat-unity_native.md` | `MaskDrawableKind` 列挙値（内部 `Part/Drawable/Mask` を export） | D 準拠で export は `Part/Mask` の2値へ修正 |
| [x] | CP108 | behavior_diff | `compat-unity_native.md` | `nijiv-opengl --test` で `0xC0000409` (BEX64) | `njgLoadPuppet` 例外未捕捉を C API 境界で捕捉し `Failure` 返却へ修正、原因切り分け可能化 |
| [x] | CP110 | behavior_diff | `compat-binfmt.md` | `MAGIC_BYTES`/`TEX_SECTION`/`EXT_SECTION` 定数不一致（`INOCHI02` 系） | D準拠 `TRNSRTS\\0` / `TEX_SECT` / `EXT_SECT` へ修正し `.inx` 読み込みを復旧 |

