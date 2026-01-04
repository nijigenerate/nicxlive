# Mask 実装互換性チェック (D ↔ C++)

| クラス/メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| typeId | "Mask" | 同等 | ◯ |
| fillDrawPacket | renderable=false,textures削除 | renderable=false＋textures消去 | ◯ |
| serializeSelfImpl | Drawable同様 | Drawable同様 | ◯ |
| deserializeFromFghj | Drawable同様 | Drawable同様 | ◯ |
| renderMask | MaskApplyPacket(kind=Mask, maskPacket)をbackendへ | kind=MaskでapplyMask | ◯ |
| fillMaskDrawPacket | model/mvp/origin/offset/stride/indexを設定。mvpは`camera * puppet * model` | camera * puppet * model を計算し、origin/stride/indexを設定 | ◯ |
| runRenderTask | 何もしない | 同等 | ◯ |
| draw | 子をdraw、selfは描画しない | 同等 | ◯ |

※ 現状カメラ行列も配線済み。
