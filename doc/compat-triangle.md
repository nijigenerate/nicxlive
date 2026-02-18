# triangle 実装互換性チェック (D `nijilive.math.triangle` ↔ C++ `core/math/triangle.cpp/hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `isPointInTriangle(vec2, Vec2Array)` | 3頂点で点が三角形内か判定 | 実装済み | ◯ |
| `findSurroundingTriangle(vec2, MeshData&)` | インデックス配列を走査し包含三角形を返す | 実装済み | ◯ |
| `calcOffsetInTriangleCoords(vec2, MeshData&, int[] triangle)` | 三角形座標系でのオフセットを算出 | 実装済み | ◯ |
| `nlCalculateTransformInTriangle(Vec2Array, int[] triangle, Vec2Array deform, vec2 target, out vec2 target_prime, out float rotVert, out float rotHorz)` | 変形後の射影と回転角を計算 | 実装済み | ◯ |
| `private: calculateAffineTransform(Vec2Array, int[] triangle, Vec2Array deform)` | アフィン行列を組み立て | 実装済み（内部ヘルパ） | ◯ |
| `private: applyAffineTransform(mat3, vec2)` | アフィン変換を適用 | 実装済み（内部ヘルパ） | ◯ |
| `private: calculateAngle(vec2, vec2)` | 2点から角度を算出 | 実装済み（内部ヘルパ） | ◯ |
| `barycentric(p, v0, v1, v2)` | バリセントリック座標を計算 | 実装あり | ◯ |
| `pointInTriangle(p, v0, v1, v2)` | バリセントリックで包含判定 | 実装あり（barycentric依存） | ◯（Vec2Array 版なし） |

**現状**: D 版の triangle 系 API は主要機能を移植済み。***

