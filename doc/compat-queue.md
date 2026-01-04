# Queue Backend 実装互換性チェック (D ↔ C++)

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
| createTextureHandle/Texture管理 | 実装（QueueTextureHandle, upload/update/params等） | 実装（TextureHandle map） | ◯ |
| submit/draw commands | CommandQueueEmitterがキュー記録 | QueueCommandEmitterがキュー記録 | ◯ |
| queuedCommands/recorded取得 | 記録を返す | recorded/queueで返す | ◯ |
| playback | queuedCommandsを backend に再生 | QueueRenderBackend::playback で再生 | ◯ |
| drawTextureAtPart/Position/Rect | 全て空実装（差分評価用に保持のみ） | 全て空実装 | ◯ |
| differenceAggregation API | enable/region/result を保持、evaluateは未実装 | enable/region/result を保持、evaluateは未実装 | ◯ |

※デバッグ描画系はDでも実描画はしないが、point/line幅設定を保持する実装が未実装のため△。
