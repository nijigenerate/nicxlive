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
| `filterChildren` | ビットマスクで三角射影し offset を返す | path/grid 判定含め同等 | ◯ |
| `runPreProcessTask` | deform 適用後に三角行列再計算・forward/inverse 更新 | deform適用と行列計算あり（同等寄り） | ◯ |
| `runRenderTask` | GPUなし（子が描画） | 同等 | ◯ |
| `draw` | super | 同等 | ◯ |
| `precalculate` | bounds計算→offsetMatrices→bitMask塗り潰し | pointInTriangle近似だが概ね同等 | ◯ |
| `renderMask` | マスク描画委譲 | 子のrenderMaskを呼ぶのみ | ◯ |
| `rebuffer` | dynamic時 precalculated クリア | mesh再設定＋precalc無効化 | ◯ |
| `serializeSelfImpl` | dynamic/translateChildren を保存 | 同等 | ◯ |
| `deserializeFromFghj` | dynamic/translateChildren を復元 | 同等 | ◯ |
| `setupChildNoRecurse` | translateChildren/dynamic で filter をpre/postに登録 | stage指定で自身のフィルタのみ付替え | ◯ |
| `setupChild` | super＋子孫へ filter 登録 | 同等 | ◯ |
| `releaseChildNoRecurse` | pre/post から filter を除去 | 自身のフィルタのみ除去 | ◯ |
| `releaseChild` | 子孫から filter を解除 | 子孫処理あり | ◯ |
| `captureTarget` | children_ref に追加し setupChildNoRecurse | add+フィルタ設定 | ◯ |
| `releaseTarget` | フィルタ解除し children_ref から除去 | フィルタ解除＋削除 | ◯ |
| `applyDeformToChildren` | translateChildren/dynamic に応じ deform 伝達→メッシュ破棄 | パラメータの deform binding 反映後 filterChildren→meshクリア | ◯ |
| `switchMode` | dynamic 切替で precalc クリア | 同等 | ◯ |
| `getTranslateChildren` | getter | 同等 | ◯ |
| `setTranslateChildren` | setter＋子再setup | 同等 | ◯ |
| `clearCache` | precalc/bitMask/triangles クリア | 同等 | ◯ |
| `centralize` | 子 bounds から中心移動＋頂点オフセット | D同等（子centralize→頂点移動→子ローカル再配置） | ◯ |
| `copyFrom` | MeshGroup/DynamicComposite/GridDeformer からのコピー | MeshGroup同士で主フィールドコピー | ◯ |
| `build` | precalc→子setup→super | precalc＋子setup＋Drawable build | ◯ |
| `coverOthers` | true | 同等 | ◯ |
| `mustPropagate` | false | 同等 | ◯ |

