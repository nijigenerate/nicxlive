# compat-fmt (D `nijilive.fmt` ↔ C++ `fmt/fmt.hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `inIsINPMode` | INPローディング状態を保持 | INP読込/書込でフラグ遷移、JSON読込で false へ復帰 | ◯ |
| `inLoadPuppet(string file)` | 拡張子を判定し INP/INX/JSON 読込、テクスチャセクション復元 | `.inp/.inx` を判定してバイナリ読込へ接続（非対応拡張子はエラー） | ◯ |
| `inLoadPuppetFromMemory(ubyte[])` | バイナリから Deserialize | INP/INX マジック検出→JSON＋TEX＋EXT を読み込み。テクスチャは ShallowTexture(stbi) デコード | ◯ |
| `inLoadJSONPuppet(string data)` | JSON 文字列からロード | 同等 | ◯ |
| `inWriteINPPuppetMemory(Puppet)` | INPバイナリを書き出し（テクスチャ含む） | MAGIC/JSON/テクスチャスロット/EXTを出力。テクスチャは type=1(TGA) でエンコードして書き込み（PNG/BC7 未対応） | ◯ |
| `inWriteINPPuppet(Puppet, string file)` | INPファイル書き出し | 同上のデータを書き込み | ◯ |

備考: PNG/BC7 テクスチャ種別は未対応（TGA のみ）。***


