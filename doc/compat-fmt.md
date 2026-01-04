# compat-fmt (D `nijilive.fmt` ↔ C++ `fmt/fmt.hpp`)

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `inIsINPMode` | INPローディング状態を保持 | 書き出し時にフラグを管理（読込側は未接続） | △ |
| `inLoadPuppet(string file)` | 拡張子を判定し INP/INX/JSON 読込、テクスチャセクション復元 | JSON読込にフォールバック。INP未実装 | ✗ |
| `inLoadPuppetFromMemory(ubyte[])` | バイナリから Deserialize | INP/INX マジック検出→JSON＋TEX＋EXT を読み込み。テクスチャは ShallowTexture(stbi) デコード | ◯ |
| `inLoadJSONPuppet(string data)` | JSON 文字列からロード | 同等 | ◯ |
| `inWriteINPPuppetMemory(Puppet)` | INPバイナリを書き出し（テクスチャ含む） | MAGIC/JSON/テクスチャスロット/EXTを出力。テクスチャは type=1(TGA) でエンコードして書き込み（PNG/BC7 未対応） | △ |
| `inWriteINPPuppet(Puppet, string file)` | INPファイル書き出し | 同上のデータを書き込み | △ |

備考: INP バイナリ入出力が未移植。テクスチャスロットの復元・EXTセクション書き込み等を写経する必要あり。***
