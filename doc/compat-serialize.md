# compat-serialize (D `nijilive.fmt.serialize` ↔ C++ `fmt/serialize.hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| ISerializable | serialize を要求 | インターフェイスなし | △ |
| IDeserializable | deserialize を要求 | インターフェイスなし | △ |
| Ignore | 属性エイリアス（serdeIgnore 等） | ダミー using (int) のみ | △ |
| Optional | 属性エイリアス（serdeOptional 等） | ダミー using (int) のみ | △ |
| Name | 属性エイリアス（serdeName 等） | ダミー using (int) のみ | △ |
| `inLoadJsonData` | ファイルを読んで `deserialize!T(parseJson(...))` | Boost ptree で JSON 読み込み → `deserializeFromFghj` | ◯ |
| `inLoadJsonDataFromMemory` | メモリ文字列からデシリアライズ | 同上 (ptree + deserializeFromFghj) | ◯ |
| `inToJson` | JsonSerializer でシリアライズ | ptree に serialize() を流し write_json | ◯ |
| `inToJsonPretty` | pretty 出力 | 同上 | ◯ |

備考: fghj を ptree に置き換える前提の簡易実装だが、現行要件では許容。***

