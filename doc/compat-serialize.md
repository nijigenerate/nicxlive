# compat-serialize (D `nijilive.fmt.serialize` ↔ C++ `fmt/serialize.hpp`)

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `ISerializable`/`IDeserializable` | インターフェイスで serialize/deserialize を要求 | ptree 前提で `serialize`/`deserializeFromFghj` を呼ぶ（インターフェイスなし） | ◯ |
| `Ignore`/`Optional`/`Name` | 属性エイリアス（serdeIgnore 等） | ダミー `using` (int) のみ | ◯ |
| `inLoadJsonData` | ファイルを読んで `deserialize!T(parseJson(...))` | Boost ptree で JSON 読み込み → `deserializeFromFghj` | ◯ |
| `inLoadJsonDataFromMemory` | メモリ文字列からデシリアライズ | 同上 (ptree + deserializeFromFghj) | ◯ |
| `inToJson` | JsonSerializer でシリアライズ | ptree に serialize() を流し write_json | ◯ |
| `inToJsonPretty` | pretty 出力 | 同上 | ◯ |

備考: fghj を ptree に置き換える前提の簡易実装だが、現行要件では許容。***
