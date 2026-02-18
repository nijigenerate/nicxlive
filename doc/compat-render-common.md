# render/common.hpp 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| struct `RenderGpuState` | framebuffer/drawBuffers/colorMask/blendEnabled | 同等フィールドを保持 | ◯ |
| class `RenderBackend` | バックエンド実体（OpenGL/Queue等） | インターフェースを同等化（Mask/Composite/DrawPart含む） | ◯ |
| class `RenderCommandEmitter.beginFrame` | backend/gpustateでフレーム開始 | state初期化＋キュークリア | ◯ |
| class `RenderCommandEmitter.playback` | D には存在しない | 削除済み | ◯ |
| class `RenderCommandEmitter.endFrame` | フレーム終了 | No-Op（Queue用途） | ◯ |
| `beginMask` | マスク設定を発行 | 同等の記録を行う | ◯ |
| `applyMask` | マスク設定を発行 | 同等の記録を行う | ◯ |
| `beginMaskContent` | マスク設定を発行 | 同等の記録を行う | ◯ |
| `endMask` | マスク設定を発行 | 同等の記録を行う | ◯ |
| `drawPart` | Part描画命令を発行 | PartDrawPacket発行で同等 | ◯ |
| struct `DynamicCompositePass` | textures/stencil/scale/rotation/origBuffer/viewport/autoScaled | 同等フィールドを保持 | ◯ |
| `getCurrentRenderBackend` | グローバル現行バックエンドの取得 | 同等 | ◯ |
| `setCurrentRenderBackend` | グローバル現行バックエンドの設定 | 同等 | ◯ |
