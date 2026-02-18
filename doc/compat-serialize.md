# compat-serialize (D `nijilive.fmt.serialize` ↔ C++ `fmt/serialize.hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| ISerializable | serialize を要求 | `ISerializable` を追加（`serialize(InochiSerializer&)`） | ◯ |
| IDeserializable | deserialize を要求 | `IDeserializable<T>::deserialize(Fghj)` を追加 | ◯ |
| Ignore | 属性エイリアス（serdeIgnore 等） | `Ignore` マーカー型を追加 | ◯ |
| Optional | 属性エイリアス（serdeOptional 等） | `Optional` マーカー型を追加 | ◯ |
| Name | 属性エイリアス（serdeName 等） | `Name{jsonKey, displayName}` マーカー型を追加 | ◯ |
| `inLoadJsonData` | ファイルを読んで `deserialize!T(parseJson(...))` | Boost ptree で JSON 読み込み → `deserializeFromFghj` | ◯ |
| `inLoadJsonDataFromMemory` | メモリ文字列からデシリアライズ | 同上 (ptree + deserializeFromFghj) | ◯ |
| `inToJson` | JsonSerializer でシリアライズ | ptree に serialize() を流し write_json | ◯ |
| `inToJsonPretty` | pretty 出力 | 同上 | ◯ |

備考: C++側はコンパイル時属性反映機構は持たないため、マーカーは互換表現（メタ情報）として実装。***

