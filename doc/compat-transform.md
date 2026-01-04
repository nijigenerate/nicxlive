# Transform 実装互換性チェック (D `nijilive.math.transform` ↔ C++ `core/math/transform.hpp`)

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| フィールド `translation` | vec3(0,0,0) | Vec3(0,0,0) | ◯ |
| フィールド `rotation` | vec3(0,0,0) | Vec3(0,0,0) | ◯ |
| フィールド `scale` | vec2(1,1) | Vec3(1,1,1) | ✗（Z含む形で不一致） |
| フィールド `pixelSnap` | bool pixelSnap | 未実装 | ✗ |
| 非公開フィールド `trs` | mat4 キャッシュ | 未保持 | ✗ |
| ctor `this(vec3, vec3, vec2)` | translation/rotation/scale 初期化 | デフォルトのみ | ✗ |
| `calcOffset(Transform other)` | translation/rotation は加算、scale は乗算、update() 呼び出し | translation/rotation 加算、scale 乗算（Vec3） | △（scale 次元差、update 空実装） |
| 演算子 `opBinary "*" (Transform)` | trs 合成 + translation/rotation/scale 再計算 | 未実装 | ✗ |
| `matrix()` | trs を返す | toMat4() で都度計算（trs なし） | △ |
| `update()` | translation/rotation/scale から trs を再計算 | 空実装 | ✗ |
| `clear()` | translation/rotation/scale をリセット | リセット | ◯（scale 次元差あり） |
| `toString()` | 文字列表現 | 未実装 | ✗ |
| `serialize()/deserializeFromFghj()` | trans/rot/scale をシリアライズ/デシリアライズ | 未実装 | ✗ |
| `Transform2D.translation` | vec2 | 未実装 | ✗ |
| `Transform2D.scale` | vec2 | 未実装 | ✗ |
| `Transform2D.rotation` | float | 未実装 | ✗ |
| `Transform2D.matrix()` | trs を返す | 未実装 | ✗ |
| `Transform2D.update()` | mat3 translation/rotation/scale を合成 | 未実装 | ✗ |

**現状**: C++ 版は Transform の最低限のフィールドと Mat4 生成（都度計算）だけを持ち、D 版の行列キャッシュ、pixelSnap、シリアライズ、Transform2D 系が未実装。scale は D が vec2 なのに対し C++ は Vec3 で非互換。***
