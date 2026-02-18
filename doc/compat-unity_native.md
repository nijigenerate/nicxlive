# unity_native (D `integration/unity.d` ↔ C++ `core/unity_native.*`) 互換性チェック

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `NjgRenderCommandKind` | `DrawPart, DrawMask, BeginDynamicComposite...` の順 | 同順に修正済み | ◯ |
| `NjgQueuedCommand` | 定義あり | 定義あり | ◯ |
| `NjgPartDrawPacket` | `modelMatrix, renderMatrix, renderRotation, ...` | `renderMatrix/renderRotation` を含む同一レイアウトに修正済み | ◯ |
| `NjgMaskDrawPacket` | 定義あり | 定義あり | ◯ |
| `NjgMaskApplyPacket` | 定義あり | 定義あり | ◯ |
| `MaskDrawableKind` | `Part, Mask` | `Part, Mask` の2値へ修正済み（内部 `Drawable` は export しない） | ◯ |
| `NjgDynamicCompositePass` | `autoScaled/drawBufferCount/hasStencil` を含む | 同一レイアウトへ修正済み | ◯ |
| `CommandQueueView` | `const NjgQueuedCommand* commands; size_t count;` | 同一レイアウトに修正済み | ◯ |
| `TextureStats` | 定義あり | 定義あり | ◯ |
| `SharedBufferSnapshot` | 定義あり | 定義あり | ◯ |
| `NjgRenderTargets` | 定義あり | 定義あり | ◯ |
| `UnityResourceCallbacks.createTexture` | 定義あり | 定義あり | ◯ |
| `UnityResourceCallbacks.updateTexture` | 定義あり | 定義あり | ◯ |
| `UnityResourceCallbacks.releaseTexture` | 定義あり | 定義あり | ◯ |
| `njgRuntimeInit` | export 関数あり | あり | ◯ |
| `njgRuntimeTerm` | export 関数あり | あり | ◯ |
| `njgCreateRenderer` | export 関数あり | あり | ◯ |
| `njgDestroyRenderer` | export 関数あり | あり | ◯ |
| `njgLoadPuppet` | export 関数あり | あり | ◯ |
| `njgUnloadPuppet` | export 関数あり | あり | ◯ |
| `njgBeginFrame` | export 関数あり | あり | ◯ |
| `njgTickPuppet` | `NjgResult function(void* puppet, double deltaSeconds)` | `NjgResult njgTickPuppet(void* puppet, double deltaSeconds)` | ◯ |
| `njgEmitCommands` | `NjgResult function(void* renderer, CommandQueueView* outView)` | `NjgResult njgEmitCommands(void* renderer, CommandQueueView* outView)` | ◯ |
| `njgFlushCommandBuffer` | export 関数あり | あり | ◯ |
| `njgGetTextureStats` | export 関数あり | あり | ◯ |
| `njgGetRenderTargets` | export 関数あり | あり | ◯ |
| `njgGetSharedBuffers` | export 関数あり | あり | ◯ |
| `njgSetLogCallback` | export 関数あり | あり | ◯ |
| `njgGetGcHeapSize` | export 関数あり | あり（GC未採用のため常に0を返却） | ◯ |
| `njgGetParameters` | export 関数あり | あり | ◯ |
| `njgUpdateParameters` | export 関数あり | あり | ◯ |
| `njgGetPuppetExtData` | export 関数あり | あり | ◯ |
| `njgPlayAnimation` | export 関数あり | あり（renderer単位状態管理、`play` の再開/初回リセットを実装） | △ |
| `njgPauseAnimation` | export 関数あり | あり（renderer単位状態管理、`createOrGet` 相当で状態生成） | △ |
| `njgStopAnimation` | export 関数あり | あり（renderer単位状態管理、`immediate` 分岐を実装） | △ |
| `njgSeekAnimation` | export 関数あり | あり（renderer単位状態管理、`createOrGet` 相当で状態生成） | △ |
| `njgSetPuppetScale` | export 関数あり | あり | ◯ |
| `njgSetPuppetTranslation` | export 関数あり | あり | ◯ |
| `njgLoadPuppetFromMemory` | D 側に export なし | C++ から削除済み | ◯ |
| `njgWritePuppetToMemory` | D 側に export なし | C++ から削除済み | ◯ |
| `njgFreeBuffer` | D 側に export なし | C++ から削除済み | ◯ |

**現状**: D export API の欠落項目は埋め込み済み。`njgTickPuppet`/`njgEmitCommands` に加えて `NjgRenderCommandKind`/`CommandQueueView`/`NjgPartDrawPacket`/`NjgDynamicCompositePass` の ABI 不一致も修正済み。`nijiv --test` 失敗は `njgLoadPuppet` 例外未捕捉が直接因であり、C API 境界で捕捉して `Failure` 返却へ修正済み。残タスクは animation API の D 同等再生挙動（`AnimationPlayer` 相当）。 
