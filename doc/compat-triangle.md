# triangle 実装互換性チェック (D `nijilive.math.triangle` ↔ C++ `core/math/triangle.cpp/hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `isPointInTriangle(vec2, Vec2Array)` | 3頂点で点が三角形内か判定 | 未実装 | ✗（未実装） |
| `findSurroundingTriangle(vec2, MeshData&)` | インデックス配列を走査し包含三角形を返す | 未実装 | ✗（未実装） |
| `calcOffsetInTriangleCoords(vec2, MeshData&, int[] triangle)` | 三角形座標系でのオフセットを算出 | 未実装 | ✗（未実装） |
| `nlCalculateTransformInTriangle(Vec2Array, int[] triangle, Vec2Array deform, vec2 target, out vec2 target_prime, out float rotVert, out float rotHorz)` | 変形後の射影と回転角を計算 | 未実装 | ✗（未実装） |
| `private: calculateAffineTransform(Vec2Array, int[] triangle, Vec2Array deform)` | アフィン行列を組み立て | 未実装 | ✗（未実装） |
| `private: applyAffineTransform(mat3, vec2)` | アフィン変換を適用 | 未実装 | ✗（未実装） |
| `private: calculateAngle(vec2, vec2)` | 2点から角度を算出 | 未実装 | ✗（未実装） |
| `barycentric(p, v0, v1, v2)` | バリセントリック座標を計算 | 実装あり | ◯ |
| `pointInTriangle(p, v0, v1, v2)` | バリセントリックで包含判定 | 実装あり（barycentric依存） | ◯（Vec2Array 版なし） |

**現状**: C++ 版はバリセントリック計算と単純な三角形判定のみ。D 版が提供する Vec2Array ベースの包含判定、周辺三角形探索、アフィン変換・回転角算出などの関数が欠落しており、大半が未移植。***

