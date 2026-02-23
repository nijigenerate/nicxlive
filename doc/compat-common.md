# core/common utils 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| クラス/メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| `Vec2Array` 形式 | `veca!(float,2)` (SoA) | SoA 構造体（x/y 別配列） | ◯ |
| Vec2Array::push_back | 配列操作 | 同等 | ◯ |
| Vec2Array::resize | 配列操作 | 同等 | ◯ |
| Vec2Array::size | 配列操作 | 同等 | ◯ |
| Vec2Array::operator[] | 要素取得 | 同等 | ◯ |
| Vec2Array::at | 要素取得 | 同等 | ◯ |
| `InterpolateMode` | Nearest/Linear/Cubic/Step | 同等 | ◯ |
| `gatherVec2` | indices で抽出 | 同等 | ◯ |
| `scatterAddVec2` | indices で加算散布 | 同等 | ◯ |

