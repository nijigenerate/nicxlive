# Transform 実装互換性チェック (D `nijilive.math.transform` ↔ C++ `core/math/transform.hpp`)

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| フィールド `translation` | vec3(0,0,0) | Vec3(0,0,0) | ◯ |
| フィールド `rotation` | vec3(0,0,0) | Vec3(0,0,0) | ◯ |
| フィールド `scale` | vec2(1,1) | Vec3(1,1,1) | △（Z=1で運用） |
| フィールド `pixelSnap` | bool pixelSnap | 実装あり | ◯ |
| 非公開フィールド `trs` | mat4 キャッシュ | Mat4 trs あり | ◯ |
| ctor `this(vec3, vec3, vec2)` | translation/rotation/scale 初期化 | 同等引数の ctor あり（scale.z=1 に変換） | ◯ |
| `calcOffset(Transform other)` | translation/rotation は加算、scale は乗算、update() 呼び出し | 同等（pixelSnap 合成）、scale.z も乗算 | △（Z=1 想定） |
| 演算子 `opBinary "*" (Transform)` | trs 合成 + translation/rotation/scale 再計算 | 実装あり | ◯ |
| `matrix()` | trs を返す | trs を返す | ◯ |
| `update()` | translation/rotation/scale から trs を再計算 | 実装あり（pixelSnap 丸め） | ◯ |
| `clear()` | translation/rotation/scale をリセット | リセット | ◯ |
| `toString()` | 文字列表現 | 未実装 | ✗ |
| `serialize()/deserializeFromFghj()` | trans/rot/scale をシリアライズ/デシリアライズ | 未実装 | ✗ |
| `Transform2D.translation` | vec2 | 実装あり | ◯ |
| `Transform2D.scale` | vec2 | 実装あり | ◯ |
| `Transform2D.rotation` | float | 実装あり | ◯ |
| `Transform2D.matrix()` | trs を返す | 実装あり | ◯ |
| `Transform2D.update()` | mat3 translation/rotation/scale を合成 | 実装あり | ◯ |

**現状**: Transform/Transform2D の主要な挙動は写経ベースで実装済み。残課題は scale 次元の扱い（Z は 1 固定想定）、toString/シリアライズ系の未実装。***
