# Mask 実装互換性チェック (D ↔ C++)

| クラス/メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| typeId | "Mask" | 同等 | ◯ |
| fillDrawPacket | renderable=false,textures削除 | renderable=false＋textures消去 | ◯ |
| serializeSelfImpl | Drawable同様 | Drawable同様 | ◯ |
| deserializeFromFghj | Drawable同様 | Drawable同様 | ◯ |
| renderMask | MaskApplyPacket(kind=Mask, maskPacket)をbackendへ | kind=MaskでapplyMask | ◯ |
| fillMaskDrawPacket | model/mvp/origin/offset/stride/index等を設定 | model/puppet/origin/offset/stride/index・texture解除 | ◯ |
| runRenderTask | 何もしない | 同等 | ◯ |
| draw | 子をdraw、selfは描画しない | 同等 | ◯ |

※ fillDrawPacket/ fillMaskDrawPacket の詳細（textures削除、MVP設定など）が未反映。
