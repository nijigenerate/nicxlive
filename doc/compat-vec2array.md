# Vec2Array 実装互換性チェック (D `nijilive.math.veca` ↔ C++ `core/math/veca.hpp`)

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| フィールド `lanes` (x,y SoA) | SIMD対応の SoA バッファ（Vector!(T,2) を格納、laneStride 等を管理） | x/y + logicalLength/laneStride/laneBase/viewCapacity/ownsStorage を保持 | △（アライン/GCなし、ビューは内部ポインタ管理） |
| フィールド `backing` 等 (alignment/GC確保) | 16byte アラインで GC.malloc を確保、view と owned を切替 | std::vector ベース（アライン・GCなし） | △ |
| ctor `this(size_t)` | ensureLength でバッファ確保 | ensureLength で確保 | ◯ |
| ctor `this(Element[] values)` | AoS配列を SoA にコピー | `assign` で対応 | ◯ |
| ctor `this(Element value)` | 1要素の SoA 初期化 | 1要素コンストラクタあり | ◯ |
| `length / opDollar` | 論理長を返す/設定できる | size()+setLength | ◯ |
| `ensureLength` | owned/view で挙動分岐し rebindLanes | owned＋非オーナービュー対応で論理長更新・ポインタ維持 | ◯ |
| `append(Element)` | ensureLength→末尾書き込み | 末尾に追加 | ◯ |
| `opIndex` (mutable/const) | vecv/vecvConst ビューを返す | Vec2View(Vec2ViewConst) で返却 | ◯ |
| `assign(Element[])` | AoS から SoA にコピー | 実装あり | ◯ |
| `opAssign(veca)` | SoA 同士の memmove コピー | copyFrom/dup あり（SIMDなし） | △ |
| `opOpAssign (veca, Vector, scalar)` | 要素単位 SIMD 対応の +,-,*,/ | +,-,* 対応（SIMDなし） | △ |
| `toArray / toArrayInto` | AoS 配列化 | 実装あり | ◯ |
| `dup` | SoA 複製 | 実装あり | ◯ |
| `clear` | logicalLength 0＋backing扱い分岐 | 実装あり | ◯ |
| `opOpAssign "~"` (append) | SoA/AoS/vecv を追加 | Vec2/Vec2Array/ベクタ追加あり | ◯ |
| `opSliceAssign` 系 | スライス代入各種 | start/len で Vec2Array/Vec2 を代入可能 | ◯ |
| `lane(component)` | コンポーネントの inout スライス取得 | owned時に std::vector を返す | ◯ |
| `rawStorage/bindExternalStorage` | 外部バッファビューを構築 | 非オーナーのビューを構築（長さチェックあり） | ◯ |
| `front/back` (mutable/const) | vecv / vecvConst を返す | Vec2View / Vec2ViewConst | ◯ |
| `empty` | length==0 | 実装あり | ◯ |
| `opApply` 系 | foreach 互換の delegate 反復 | forEach で同等 | ◯ |
| テンプレート別名/alias (Vec2Array 等) | Vec2/3/4 すべて提供 | Vec2/Vec3/Vec4 を提供 | ◯ |
| SIMD対応 (`applySIMD` 等) | パック幅で SIMD 加速 | 未実装 | ✗ |
| 単体テスト | ユニットテスト多数 | なし | ✗ |

**現状**: Vec2Array は SoA 構造・ビューアクセス・append/assign/dup・スライス代入・非オーナービューを写経ベースで反映済み。Vec3Array/Vec4Array も同様の SoA 実装を追加。未対応は SIMD 最適化とユニットテスト群。***
