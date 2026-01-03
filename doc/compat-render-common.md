# render/common.hpp 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| struct `RenderGpuState` | GPU状態保持。init はバックエンド依存 | 空構造体＋init()のみ | △ |
| class `RenderBackend` | バックエンド実体（OpenGL/Queue等） | 抽象ベースのみ（Queue派生あり） | △ |
| class `RenderCommandEmitter.beginFrame` | backend/gpustateでフレーム開始 | QueueEmitterはキュー初期化のみ | △ |
| `RenderCommandEmitter.playback` | キューを実行 | QueueEmitterはQueue再生のみ（描画なし） | △ |
| `RenderCommandEmitter.endFrame` | フレーム終了 | QueueEmitterはNo-Op | △ |
| `beginMask/applyMask/beginMaskContent/endMask` | マスク設定を発行 | QueueEmitterはキュー記録のみ | △ |
| `drawPart` | Part描画命令を発行 | QueueEmitterはキュー記録のみ | △ |
| struct `DynamicCompositePass` | textures/stencil/scale/rotation/autoScaled を保持 | 同等フィールド・未利用 | △ |
| `getCurrentRenderBackend/setCurrentRenderBackend` | グローバル現行バックエンドの取得/設定 | 追加済み | ◯ |
