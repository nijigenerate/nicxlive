# Transform 実装互換性チェック (D `nijilive.math.transform` ↔ C++ `core/math/transform.hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| フィールド `translation` | vec3(0,0,0) | Vec3(0,0,0) | ◯ |
| フィールド `rotation` | vec3(0,0,0) | Vec3(0,0,0) | ◯ |
| フィールド `scale` | vec2(1,1) | Vec2(1,1) | ◯ |
| フィールド `pixelSnap` | bool pixelSnap | 実装あり | ◯ |
| 非公開フィールド `trs` | mat4 キャッシュ | Mat4 trs あり | ◯ |
| ctor `this(vec3, vec3, vec2)` | translation/rotation/scale 初期化 | 同等引数の ctor あり | ◯ |
| `calcOffset(Transform other)` | translation/rotation は加算、scale は乗算、update() 呼び出し | 同等（pixelSnap 合成なし） | ◯ |
| 演算子 `opBinary "*" (Transform)` | trs 合成 + translation/rotation/scale 再計算 | 実装あり | ◯ |
| `matrix()` | trs を返す | trs を返す | ◯ |
| `update()` | translation/rotation/scale から trs を再計算 | 実装あり（pixelSnap 丸め） | ◯ |
| `clear()` | translation/rotation/scale をリセット | リセット | ◯ |
| `toString()` | 文字列表現 | 実装済み（行列+TRSの文字列化） | ◯ |
| serialize() | trans/rot/scale をシリアライズ | 実装済み（trans/rot/scale キー） | ◯ |
| deserializeFromFghj() | trans/rot/scale をデシリアライズ | 実装済み（trans/rot/scale キー読込） | ◯ |
| `Transform2D.translation` | vec2 | 実装あり | ◯ |
| `Transform2D.scale` | vec2 | 実装あり | ◯ |
| `Transform2D.rotation` | float | 実装あり | ◯ |
| `Transform2D.matrix()` | trs を返す | 実装あり | ◯ |
| `Transform2D.update()` | mat3 translation/rotation/scale を合成 | 実装あり | ◯ |

**現状**: Transform/Transform2D の主要 API は実装済み。`scale` は D 実装と同じ `vec2` で一致。***

