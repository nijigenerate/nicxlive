# Vec2Array 実装互換性チェック (D `nijilive.math.veca` ↔ C++ `core/math/veca.hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| フィールド `lanes` (x,y SoA) | SIMD対応の SoA バッファ（Vector!(T,2) を格納、laneStride 等を管理） | SoA `x/y` レーン保持 + `lanes()` で明示アクセス可 | ◯ |
| フィールド `backing` | 16byte アラインGC.mallocを保持 | `backing_` を保持し `backing()` で参照可（rawStorage用連続SoA） | ◯ |
| フィールド `alignment` | 16byte アライン | `kSimdAlignment=16` / `alignment_` を保持し `alignment()` で参照可 | ◯ |
| `length` | 論理長を返す/設定 | size()+setLength | ◯ |
| `opDollar` | 論理長取得 | size() 相当 | ◯ |
| `opIndex` mutable | vecv ビューを返す | Vec2View で返却 | ◯ |
| `opIndex` const | vecvConst ビューを返す | Vec2ViewConst で返却 | ◯ |
| `toArray` | AoS 配列化 | 実装あり | ◯ |
| `toArrayInto` | AoS 配列化 | 実装あり | ◯ |
| `rawStorage` | 外部バッファ参照取得 | 非オーナービュー対応 | ◯ |
| `bindExternalStorage` | 外部バッファビューを構築 | API は存在するが、`x/y` 直接アクセス経路との整合が未保証 | △ |
| 外部ビュー + lane直接アクセスの一貫性 | 外部ビュー時でも `lane(0/1)` と各種演算/参照が破綻しない | `x/y` 直接参照コードが多く、ビュー時に atlas先頭/別配列参照へ崩れる経路が残る | △ |
| コピー構築/コピー代入 | 値型（レーン実体を複製） | `Vec2Array/Vec3Array/Vec4Array` で copy ctor/copy assign を明示実装し、内部ポインタ再同期込みで値コピー | ◯ |
| `copyFrom` 初期化 | コピー前に内部状態を一貫初期化 | `logicalLength_` を含む状態初期化を実装し、`ensureLength` 早期 return による不正ポインタ書き込みを解消 | ◯ |
| `front` mutable | vecv を返す | Vec2View | ◯ |
| `front` const | vecvConst を返す | Vec2ViewConst | ◯ |
| `back` mutable | vecv を返す | Vec2View | ◯ |
| `back` const | vecvConst を返す | Vec2ViewConst | ◯ |
| テンプレート別名 `Vec2Array` | 提供 | 提供 | ◯ |
| テンプレート別名 `Vec3Array` | 提供 | 提供 | ◯ |
| テンプレート別名 `Vec4Array` | 提供 | 提供 | ◯ |
| SIMD対応 (`applySIMD` 等) | パック幅で SIMD 加速 | `Vec2Array` の `+=`/`-=`/`*=(scalar)` に SIMD パスを実装（SSE, scalar fallback あり） | △ |
| 単体テスト | ユニットテスト多数 | `tests/vec2array_test.cpp` を追加（SoA rawStorage / SIMD演算 / external view） | ◯ |
