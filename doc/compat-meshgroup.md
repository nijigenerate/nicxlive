# MeshGroup 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| フィールド `bitMask` | 三角形IDを格納するマスク | 塗り潰し同等 | ◯ |
| フィールド `bounds` | 三角形境界（floor/ceil適用） | 計算同等 | ◯ |
| フィールド `triangles` | offset/transformMatrix を保持 | Mat3 だが同等演算 | ◯ |
| フィールド `transformedVertices` | SoA で頂点変形後を保持 | SoA で保持 | ◯ |
| フィールド `forwardMatrix` | transform 行列 | 同等 | ◯ |
| フィールド `inverseMatrix` | global 逆行列 | 同等 | ◯ |
| フィールド `translateChildren` | 子へ平行移動転送フラグ | 同等 | ◯ |
| フィールド `dynamic` | 動的モード | 同等 | ◯ |
| フィールド `precalculated` | プレ計算済みフラグ | 同等 | ◯ |
| ctor | parent/タスク登録 | parent付きコンストラクタで super 相当＋preProcess登録 | ◯ |
| `typeId` | "MeshGroup" | 同等 | ◯ |
| `preProcess` | super 呼び出し | 同等 | ◯ |
| `postProcess` | super 呼び出し | 同等 | ◯ |
| `filterChildren` | `Tuple!(Vec2Array, mat4*, bool)` を返す | `tuple<Vec2Array, optional<Mat4>, bool>` を返す（matrixは常に `nullopt`） | △ |
| `runPreProcessTask` | deform 適用後に三角行列再計算・forward/inverse 更新 | deform適用と行列計算あり（同等寄り） | ◯ |
| `runRenderTask` | GPUなし（子が描画） | 同等 | ◯ |
| `draw` | super | 同等 | ◯ |
| `precalculate` | bounds計算→offsetMatrices→bitMask塗り潰し | pointInTriangle近似だが概ね同等 | ◯ |
| `renderMask` | マスク描画委譲 | 子のrenderMaskを呼ぶのみ | ◯ |
| `rebuffer` | dynamic時 precalculated クリア | mesh再設定＋precalc無効化 | ◯ |
| `serializeSelfImpl` | dynamic/translateChildren を保存 | 同等 | ◯ |
| `deserializeFromFghj` | dynamic/translateChildren を復元 | 同等 | ◯ |
| `setupChildNoRecurse` | `tuple(stage, &filterChildren)` を upsert/remove | `stage+tag` + lambda bridge で登録/解除 | △ |
| `setupChild` | super＋子孫へ filter 登録 | 同等 | ◯ |
| `releaseChildNoRecurse` | `tuple(stage, &filterChildren)` を除去 | `stage+tag` 条件で除去 | △ |
| `releaseChild` | 子孫から filter を解除 | 子孫処理あり | ◯ |
| filter hook 同一性管理 | `(stage, func)` 単位で登録/解除 | `stage + tag` 単位で登録/解除を一致化 | ◯ |
| `captureTarget` | children_ref に追加し setupChildNoRecurse | add+フィルタ設定 | ◯ |
| `releaseTarget` | フィルタ解除し children_ref から除去 | フィルタ解除＋削除 | ◯ |
| `applyDeformToChildren` | `_applyDeformToChildren(tuple(0,&filterChildren), ...)` を呼ぶ | 専用実装で手動転送（NodeFilterMixin 共通実装を使わない） | ✗ |
| `switchMode` | dynamic 切替で precalc クリア | 同等 | ◯ |
| `getTranslateChildren` | getter | 同等 | ◯ |
| `setTranslateChildren` | setter＋子再setup | 同等 | ◯ |
| `clearCache` | precalc/bitMask/triangles クリア | 同等 | ◯ |
| `centralize` | 子 bounds から中心移動＋頂点オフセット | D同等（子centralize→頂点移動→子ローカル再配置） | ◯ |
| `copyFrom` | MeshGroup/DynamicComposite/GridDeformer からのコピー | MeshGroup同士で主フィールドコピー | ◯ |
| `build` | precalc→子setup→super | precalc＋子setup＋Drawable build | ◯ |
| `coverOthers` | true | 同等 | ◯ |
| `mustPropagate` | false | 同等 | ◯ |

## 行単位差分（フィルタ関連）
1. `filterChildren` 戻り値型
`nijilive/source/nijilive/core/nodes/meshgroup/package.d:94`
`nicxlive/core/nodes/mesh_group.cpp:347-350`
差分:
- D: `Tuple!(Vec2Array, mat4*, bool)`。
- C++: `std::tuple<Vec2Array, std::optional<Mat4>, bool>`。

2. `setupChildNoRecurse` の filter 登録方式
`nijilive/source/nijilive/core/nodes/meshgroup/package.d:354-374`
`nicxlive/core/nodes/mesh_group.cpp:738-830`
差分:
- D: `tuple(0, &filterChildren)` を upsert/remove。
- C++: `FilterHook{stage=0, tag=this}` + lambda bridge を pre/post に insert/erase。

3. `releaseChildNoRecurse` の filter 解除方式
`nijilive/source/nijilive/core/nodes/meshgroup/package.d:400-403`
`nicxlive/core/nodes/mesh_group.cpp:832-840`
差分:
- D: 関数ポインタ同値で remove。
- C++: `stage+tag` 条件で remove。

4. `applyDeformToChildren` 実装構造
`nijilive/source/nijilive/core/nodes/meshgroup/package.d:436-470`
`nicxlive/core/nodes/mesh_group.cpp:77-219`
差分:
- D: `NodeFilterMixin._applyDeformToChildren(...)` を呼ぶ。
- C++: NodeFilterMixin を使わず、TRS適用/transfer/filter適用を関数内で再実装。

5. `applyDeformToChildren` のメッシュ破棄処理
`nijilive/source/nijilive/core/nodes/meshgroup/package.d:466-470`
`nicxlive/core/nodes/mesh_group.cpp:214-219`
差分:
- D: `data.indices/vertices/uvs` を個別に length=0。
- C++: `*mesh = MeshData{}` で全体リセット。
