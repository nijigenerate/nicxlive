# commands 実装互換性チェック (D `nijilive.core.render.commands` ↔ C++ `core/render/commands.*`)

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `RenderCommandKind` | DrawPart/DrawMask/BeginDynamicComposite/EndDynamicComposite/BeginMask/ApplyMask/BeginMaskContent/EndMask/BeginComposite/DrawCompositeQuad/EndComposite | 同等 (全列挙あり) | ◯ |
| `MaskDrawableKind` | Part/Mask | C++ は RenderBackend::MaskDrawableKind に同等定義 | ◯ |
| `PartDrawPacket` | isMask/renderable/matrices/tints/opacities/Texture[]/origin/atlas offsets/indexCount など詳細フィールド | 同等 (textures[3]/indexBuffer/atlas stride等を保持。追加で頂点/uv/deform配列・textureUUIDを持つ) | ◯ |
| `MaskDrawPacket` | modelMatrix/mvp/origin/offsets/index info | C++ は PartDrawPacket をマスクにも流用 (mvp 未保持)。パケット構造が統合されている | △ |
| `MaskApplyPacket` | kind/isDodge/partPacket/maskPacket | C++ 同等 | ◯ |
| `DynamicCompositeSurface` | textures[3], stencil, framebuffer, textureCount | 同等 (core/render/common.hpp に textures/stencil/framebuffer/textureCount を保持) | ◯ |
| `DynamicCompositePass` | surface/scale/rotationZ/origBuffer/origViewport/autoScaled | C++ は DynamicCompositePass に同等フィールドあり | ◯ |
| `makePartDrawPacket(Part, bool isMask)` | Part.fillDrawPacket で構築 | C++ は Part::fillDrawPacket(*part, packet, isMask) で同等 | ◯ |
| `makeMaskDrawPacket(Mask)` | Mask.fillMaskDrawPacket で構築 | C++ は PartDrawPacket 流用で mask 用パケット作成 | ◯ |
| `tryMakeMaskApplyPacket(Drawable, bool isDodge, out MaskApplyPacket)` | Mask/Part を判別、index 範囲チェック、indexBuffer/Count を検査、必要に応じ debug log | index 範囲・indexBuffer/indexCount 検査は移植済み。デバッグログは簡易 std::cerr 出力 | ◯ |
| `makeCompositeDrawPacket(Composite)` | opacity/tint/screenTint/blendMode を計算 | C++ 同等（NaN チェックは std::isnan） | ◯ |

**現状**: RenderCommandKind は揃った。PartDrawPacket は D と同等構造になったが MaskDrawPacket を統合しており mvp 等を保持していない点は残る。DynamicCompositeSurface も同等フィールドを保持。`tryMakeMaskApplyPacket` は indexBuffer 検査とデバッグログが未移植。***
