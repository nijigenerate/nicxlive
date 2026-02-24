# Mask 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| クラス/メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| typeId | "Mask" | 同等 | ◯ |
| fillDrawPacket | renderable=false,textures削除 | renderable=false＋textures消去 | ◯ |
| serializeSelfImpl | Drawable同様 | Drawable同様 | ◯ |
| deserializeFromFghj | Drawable同様 | Drawable同様 | ◯ |
| renderMask | MaskApplyPacket(kind=Mask, isDodge, maskPacket)をbackendへ | kind=Mask / `isDodge=dodge` で applyMask | ◯ |
| fillMaskDrawPacket | model/mvp/origin/offset/stride/indexを設定。offscreen有効時は `offscreenRender * model`、それ以外は `currentRenderSpace.matrix * model` | 同等（offscreen分岐と currentRenderSpace を写経反映） | ◯ |
| runRenderTask | 何もしない | 同等 | ◯ |
| draw | 子をdraw、selfは描画しない | 同等 | ◯ |

※ 現状カメラ行列も配線済み。
