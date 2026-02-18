# Vec2Array 実装互換性チェック (D `nijilive.math.veca` ↔ C++ `core/math/veca.hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| フィールド `lanes` (x,y SoA) | SIMD対応の SoA バッファ（Vector!(T,2) を格納、laneStride 等を管理） | x/y + logicalLength/laneStride/laneBase/viewCapacity/ownsStorage を保持 | △ |
| フィールド `backing` | 16byte アラインGC.mallocを保持 | std::vector ベース（GCなし） | △ |
| フィールド `alignment` | 16byte アライン | 明示管理なし | △ |
| `length` | 論理長を返す/設定 | size()+setLength | ◯ |
| `opDollar` | 論理長取得 | size() 相当 | ◯ |
| `opIndex` mutable | vecv ビューを返す | Vec2View で返却 | ◯ |
| `opIndex` const | vecvConst ビューを返す | Vec2ViewConst で返却 | ◯ |
| `toArray` | AoS 配列化 | 実装あり | ◯ |
| `toArrayInto` | AoS 配列化 | 実装あり | ◯ |
| `rawStorage` | 外部バッファ参照取得 | 非オーナービュー対応 | ◯ |
| `bindExternalStorage` | 外部バッファビューを構築 | 長さチェックあり | ◯ |
| `front` mutable | vecv を返す | Vec2View | ◯ |
| `front` const | vecvConst を返す | Vec2ViewConst | ◯ |
| `back` mutable | vecv を返す | Vec2View | ◯ |
| `back` const | vecvConst を返す | Vec2ViewConst | ◯ |
| テンプレート別名 `Vec2Array` | 提供 | 提供 | ◯ |
| テンプレート別名 `Vec3Array` | 提供 | 提供 | ◯ |
| テンプレート別名 `Vec4Array` | 提供 | 提供 | ◯ |
| SIMD対応 (`applySIMD` 等) | パック幅で SIMD 加速 | 未実装 | ✗（未実装） |
| 単体テスト | ユニットテスト多数 | なし | ✗（未実装） |
