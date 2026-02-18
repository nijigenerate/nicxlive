# commands 実装互換性チェック (D `nijilive.core.render.commands` ↔ C++ `core/render/commands.*`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `RenderCommandKind` | DrawPart/DrawMask/BeginDynamicComposite/EndDynamicComposite/BeginMask/ApplyMask/BeginMaskContent/EndMask/BeginComposite/DrawCompositeQuad/EndComposite | 同等 (全列挙あり) | ◯ |
| `MaskDrawableKind` | Part/Mask | C++ は RenderBackend::MaskDrawableKind に同等定義 | ◯ |
| `PartDrawPacket` | isMask/renderable/matrices/tints/opacities/Texture[]/origin/atlas offsets/indexCount など詳細フィールド | 同等 (textures[3]/indexBuffer/atlas stride等を保持。追加で頂点/uv/deform配列・textureUUIDを持つ) | ◯ |
| `MaskDrawPacket` | modelMatrix/mvp/origin/offsets/index info | 専用構造を保持。model/mvp/origin/offset/stride/index を設定（mvp は camera\*puppet\*model） | ◯ |
| `MaskApplyPacket` | kind/isDodge/partPacket/maskPacket | C++ 同等 | ◯ |
| `DynamicCompositeSurface` | textures[3], stencil, framebuffer, textureCount | 同等 (core/render/common.hpp に textures/stencil/framebuffer/textureCount を保持) | ◯ |
| `DynamicCompositePass` | surface/scale/rotationZ/origBuffer/origViewport/autoScaled | C++ は DynamicCompositePass に同等フィールドあり | ◯ |
| `makePartDrawPacket(Part, bool isMask)` | Part.fillDrawPacket で構築 | C++ は Part::fillDrawPacket(*part, packet, isMask) で同等 | ◯ |
| `makeMaskDrawPacket(Mask)` | Mask.fillMaskDrawPacket で構築 | C++ は PartDrawPacket 流用で mask 用パケット作成 | ◯ |
| `tryMakeMaskApplyPacket(Drawable, bool isDodge, out MaskApplyPacket)` | Mask/Part を判別、index 範囲チェック、indexBuffer/Count を検査、必要に応じ debug log | index 範囲・indexBuffer/indexCount 検査を実装。ログは最小限(std::cerr) | ◯ |
| `makeCompositeDrawPacket(Composite)` | opacity/tint/screenTint/blendMode を計算 | C++ 同等（NaN チェックは std::isnan） | ◯ |

**現状**: 各コマンド種・パケット構造は D と整合。MaskDrawPacket 専用化と mvp 計算も移植済み。`tryMakeMaskApplyPacket` のログは簡略だが機能上は同等。***

