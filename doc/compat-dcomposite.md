# DynamicComposite 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| ctor DynamicComposite(parent) | Projectableベース | 対応 | ◯ |
| ctor DynamicComposite(data, uuid, parent) | Projectableベース | data/uuid対応 | ◯ |
| method `typeId` | "DynamicComposite" | 同等 | ◯ |
| field `needsRedraw` | 有 | 同等 | ◯ |
| field `cached` | 有 | 同等 | ◯ |
| field `offscreen` | 有 | 同等 | ◯ |
| Projectable 側処理 | DynamicComposite固有の追加処理なし（描画委譲） | 同等 | ◯ |
| Composite 側処理 | DynamicComposite固有の追加処理なし（描画委譲） | 同等 | ◯ |

