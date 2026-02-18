# compat-binfmt (D `nijilive.fmt.binfmt` ↔ C++ `fmt/binfmt.hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `MAGIC_BYTES` | `"INOCHI02"` | 同等 (`INOCHI02`) | ◯ |
| `TEX_SECTION` | `"TEXBLOB\0"` | 同等 | ◯ |
| `EXT_SECTION` | `"INXEXT\0\0"` | 同等 | ◯ |
| `inVerifyMagicBytes` | 先頭比較 | 同等 | ◯ |
| `inVerifySection` | セクション比較 | 同等 | ◯ |
| `inInterpretDataFromBuffer` | BE→native 変換 | 同等（BEで集計） | ◯ |

備考: C++版は `std::vector<uint8_t>` を入力に使用。***

