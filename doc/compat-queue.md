# Queue Backend 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| メソッド / フィールド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| initializeDrawableResources | noop | noop | ◯ |
| bindDrawableVao | noop | noop | ◯ |
| createDrawableBuffers | IBOハンドル生成 | IBOハンドル生成 | ◯ |
| uploadDrawableIndices | GC確保しindexBuffersに保持 | std::vectorにコピー保持 | ◯ |
| uploadSharedVertexBuffer | 共有頂点を保持 | Vec2Arrayを保持 | ◯ |
| uploadSharedUvBuffer | 共有UVを保持 | Vec2Arrayを保持 | ◯ |
| uploadSharedDeformBuffer | 共有Deformを保持 | Vec2Arrayを保持 | ◯ |
| drawDrawableElements | 呼び出し記録のみ | 呼び出し記録のみ(lastDraw) | ◯ |
| setDebugPointSize | 記録のみ | 値を保持 | ◯ |
| setDebugLineWidth | 記録のみ | 値を保持 | ◯ |
| uploadDebugBuffer | バッファ保持 | positions/indices保持 | ◯ |
| drawDebugPoints | 記録のみ | noop | ◯ |
| drawDebugLines | 記録のみ | noop | ◯ |
| createTextureHandle | 実装（QueueTextureHandle） | 実装（TextureHandle map） | ◯ |
| Texture管理 | upload/update/params 等を保持 | 同等 | ◯ |
| submit | CommandQueueEmitterがキュー記録 | QueueCommandEmitterがキュー記録 | ◯ |
| draw commands | CommandQueueEmitterがキュー記録 | QueueCommandEmitterがキュー記録 | ◯ |
| queuedCommands 取得 | 記録を返す | recorded/queueで返す | ◯ |
| `recorded` 取得 | 記録を返す | recorded/queueで返す | ◯ |
| `QueueRenderBackend::playback(RenderBackend*)` | D `RenderingBackend` には非定義 | C++ から削除済み | ◯ |
| `QueueRenderBackend::recorded()` | D `RenderingBackend` には非定義 | C++ から削除済み | ◯ |
| `QueueRenderBackend::backendQueue()` | D `RenderingBackend` には非定義 | C++ から削除済み（`queue` 直接参照） | ◯ |
| `QueueRenderBackend::backendResourceQueue()` | D `RenderingBackend` には非定義 | C++ から削除済み（`resourceQueue` 直接参照） | ◯ |
| `QueueRenderBackend::clearResourceQueue()` | D `RenderingBackend` には非定義 | C++ から削除済み（`resourceQueue.clear()`） | ◯ |
| `QueueRenderBackend::renderTarget()` | D `RenderingBackend` には非定義 | C++ から削除済み | ◯ |
| `QueueRenderBackend::compositeTarget()` | D `RenderingBackend` には非定義 | C++ から削除済み | ◯ |
| `QueueRenderBackend::sharedVerticesRaw()` | D `RenderingBackend` には非定義 | C++ から削除済み（shared buffer APIへ統一） | ◯ |
| `QueueRenderBackend::sharedUvRaw()` | D `RenderingBackend` には非定義 | C++ から削除済み（shared buffer APIへ統一） | ◯ |
| `QueueRenderBackend::sharedDeformRaw()` | D `RenderingBackend` には非定義 | C++ から削除済み（shared buffer APIへ統一） | ◯ |
| `QueueRenderBackend::sharedVertexCount()` | D `RenderingBackend` には非定義 | C++ から削除済み（shared buffer APIへ統一） | ◯ |
| `QueueRenderBackend::sharedUvCount()` | D `RenderingBackend` には非定義 | C++ から削除済み（shared buffer APIへ統一） | ◯ |
| `QueueRenderBackend::sharedDeformCount()` | D `RenderingBackend` には非定義 | C++ から削除済み（shared buffer APIへ統一） | ◯ |
| `QueueRenderBackend::sharedVerticesData()` | D `RenderingBackend` には非定義 | C++ から削除済み（shared buffer APIへ統一） | ◯ |
| `QueueRenderBackend::sharedUvData()` | D `RenderingBackend` には非定義 | C++ から削除済み（shared buffer APIへ統一） | ◯ |
| `QueueRenderBackend::sharedDeformData()` | D `RenderingBackend` には非定義 | C++ から削除済み（shared buffer APIへ統一） | ◯ |
| drawTextureAtPart | 空実装（差分評価用に保持のみ） | 空実装 | ◯ |
| drawTextureAtPosition | 空実装（差分評価用に保持のみ） | 空実装 | ◯ |
| drawTextureAtRect | 空実装（差分評価用に保持のみ） | 空実装 | ◯ |
| differenceAggregation API | enable/region/result を保持、evaluateは未実装 | enable/region/result を保持、evaluateは未実装 | ◯ |

※ 余剰 API は Unity 側の利用実態を確認しつつ、D正準に寄せて削除/縮退判断する。


