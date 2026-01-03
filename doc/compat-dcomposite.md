# DynamicComposite 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| ctor `DynamicComposite(parent)`/`(data,uuid,parent)` | Projectableベース | data/uuid対応のみ | ◯ |
| method `typeId` | "DynamicComposite" | 同等 | ◯ |
| field `needsRedraw` | 有 | 同等 | ◯ |
| field `cached` | 有 | 同等 | ◯ |
| field `offscreen` | 有 | 同等 | ◯ |
| その他 Projectable/Composite処理 | DynamicComposite固有の追加処理なし（描画委譲） | 同等 | ◯ |
