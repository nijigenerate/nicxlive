# compat-serialize (D `nijilive.fmt.serialize` ↔ C++ `fmt/serialize.hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| ISerializable | `serialize(S)(ref S)` | `ISerializable<Implementation>::serialize(S&)` calls the concrete generic body | Source mapping |
| IDeserializable | deserialize を要求 | `IDeserializable<T>::deserialize(Fghj)` を追加 | ◯ |
| Ignore | 属性エイリアス（serdeIgnore 等） | `Ignore` マーカー型を追加 | ◯ |
| Optional | 属性エイリアス（serdeOptional 等） | `Optional` マーカー型を追加 | ◯ |
| Name | 属性エイリアス（serdeName 等） | `Name{jsonKey, displayName}` マーカー型を追加 | ◯ |
| `inLoadJsonData` | ファイルを読んで `deserialize!T(parseJson(...))` | Boost ptree で JSON 読み込み → `deserializeFromFghj` | ◯ |
| `inLoadJsonDataFromMemory` | メモリ文字列からデシリアライズ | 同上 (ptree + deserializeFromFghj) | ◯ |
| `inToJson` | JsonSerializer でシリアライズ | ptree に serialize() を流し write_json | ◯ |
| `inToJsonPretty` | pretty 出力 | 同上 | ◯ |

The canonical interface is published by `core/serialization_interface.hpp` and
re-exported by `fmt/serialize.hpp`. C++ does not support virtual function
templates: the implementation type is an explicit base template argument so the
actual mutable serializer reference and concrete member template remain intact.
Every typed bridge publicly inherits the same abstract `ISerializable<>`, so
different implementing classes can be upcast and retained in heterogeneous
interface-handle collections. Common and typed bases cannot be instantiated or
copied as values. The common interface does not claim a runtime virtual template
method; calling generic serialization retains the concrete static class type.
`ngSerializeCustom` looks up the concrete static class's member, including a
subclass's own generic body. The inherited adapter rejects a missing concrete
implementation. No fixed serializer or const conversion is used.
`TypedJsonWriter` uses this path for genuine custom receivers;
existing non-interface serialization paths remain available.

`core/type_id.hpp` supplies the independent Source `TypeId` attribute value,
including its retained string descriptor. `UserDefinedAttributes<T>` records
actual annotations; `ngGetUDAs<T, Attribute>()` selects matching values and
`ngHasUDA<T, Attribute>` inspects their presence. These APIs do not derive an
annotation from RTTI, class names, or automation factory registration. Other
attribute markers above do not yet imply automatic field serialization.

