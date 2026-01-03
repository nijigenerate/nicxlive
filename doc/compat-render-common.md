# render/common.hpp 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| struct `RenderGpuState` | framebuffer/drawBuffers/colorMask/blendEnabled | 同等フィールドを保持 | ◯ |
| class `RenderBackend` | バックエンド実体（OpenGL/Queue等） | インターフェースを同等化（Mask/Composite/DrawPart含む） | ◯ |
| class `RenderCommandEmitter.beginFrame` | backend/gpustateでフレーム開始 | state初期化＋キュークリア | ◯ |
| `RenderCommandEmitter.playback` | キューを実行 | QueueEmitterは記録済みキューを保持（Queue用途） | ◯ |
| `RenderCommandEmitter.endFrame` | フレーム終了 | No-Op（Queue用途） | ◯ |
| `beginMask/applyMask/beginMaskContent/endMask` | マスク設定を発行 | 同等の記録を行う | ◯ |
| `drawPart` | Part描画命令を発行 | PartDrawPacket発行で同等 | ◯ |
| struct `DynamicCompositePass` | textures/stencil/scale/rotation/origBuffer/viewport/autoScaled | 同等フィールドを保持 | ◯ |
| `getCurrentRenderBackend/setCurrentRenderBackend` | グローバル現行バックエンドの取得/設定 | 同等 | ◯ |
