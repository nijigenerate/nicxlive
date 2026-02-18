# unity_native (D `integration/unity.d` ↔ C++ `core/unity_native.*`) 互換性チェック

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `NjgRenderCommandKind` | 定義あり | 定義あり | ◯ |
| `NjgQueuedCommand` | 定義あり | 定義あり | ◯ |
| `NjgPartDrawPacket` | 定義あり | 定義あり | ◯ |
| `NjgMaskDrawPacket` | 定義あり | 定義あり | ◯ |
| `NjgMaskApplyPacket` | 定義あり | 定義あり | ◯ |
| `NjgDynamicCompositePass` | 定義あり | 定義あり | ◯ |
| `CommandQueueView` | 定義あり | 定義あり | ◯ |
| `TextureStats` | 定義あり | 定義あり | ◯ |
| `SharedBufferSnapshot` | 定義あり | 定義あり | ◯ |
| `NjgRenderTargets` | 定義あり | C++ 側に未定義 | ✗（未実装） |
| `UnityResourceCallbacks.createTexture` | 定義あり | 定義あり | ◯ |
| `UnityResourceCallbacks.updateTexture` | 定義あり | 定義あり | ◯ |
| `UnityResourceCallbacks.releaseTexture` | 定義あり | 定義あり | ◯ |
| `njgRuntimeInit` | export 関数あり | なし | ✗（未実装） |
| `njgRuntimeTerm` | export 関数あり | なし | ✗（未実装） |
| `njgCreateRenderer` | export 関数あり | あり | ◯ |
| `njgDestroyRenderer` | export 関数あり | あり | ◯ |
| `njgLoadPuppet` | export 関数あり | あり | ◯ |
| `njgUnloadPuppet` | export 関数あり | あり | ◯ |
| `njgBeginFrame` | export 関数あり | あり | ◯ |
| `njgTickPuppet` | export 関数あり | あり | ◯ |
| `njgEmitCommands` | export 関数あり | あり | ◯ |
| `njgFlushCommandBuffer` | export 関数あり | あり | △（挙動差分あり） |
| `njgGetTextureStats` | export 関数あり | あり | ◯ |
| `njgGetRenderTargets` | export 関数あり | なし | ✗（未実装） |
| `njgGetSharedBuffers` | export 関数あり | なし（`njgEmitCommands` に統合） | ✗（未実装） |
| `njgSetLogCallback` | export 関数あり | なし | ✗（未実装） |
| `njgGetGcHeapSize` | export 関数あり | あり | △（実装方針差分） |
| `njgGetParameters` | export 関数あり | あり | ◯ |
| `njgUpdateParameters` | export 関数あり | あり | ◯ |
| `njgGetPuppetExtData` | export 関数あり | なし | ✗（未実装） |
| `njgPlayAnimation` | export 関数あり | なし | ✗（未実装） |
| `njgPauseAnimation` | export 関数あり | なし | ✗（未実装） |
| `njgStopAnimation` | export 関数あり | なし | ✗（未実装） |
| `njgSeekAnimation` | export 関数あり | なし | ✗（未実装） |
| `njgSetPuppetScale` | export 関数あり | なし | ✗（未実装） |
| `njgSetPuppetTranslation` | export 関数あり | なし | ✗（未実装） |
| `njgLoadPuppetFromMemory` | D 側に export なし | C++ 側のみ存在 | ✗（削除候補） |
| `njgWritePuppetToMemory` | D 側に export なし | C++ 側のみ存在 | ✗（削除候補） |
| `njgFreeBuffer` | D 側に export なし | C++ 側のみ存在 | ✗（削除候補） |

**現状**: D export API を基準に、未実装と削除候補を本表へ反映済み。優先実装対象は `njgRuntimeInit/Term`、`njgGetRenderTargets`、`njgGetSharedBuffers`、`njgSetLogCallback`、`njgGetPuppetExtData`、各 animation/transform API。 
