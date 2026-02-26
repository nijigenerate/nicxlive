# GridDeformer 実装互換性チェック (D ↔ C++)

判定基準: D実装 (`nijilive/source/nijilive/core/nodes/deformer/grid.d`) を正とし、関数一覧単位で C++ (`nicxlive/core/nodes/grid_deformer.cpp/.hpp`) を照合する。

| D側対象 | C++対応 | 判定 | 差分/補足 | 対応Task |
| --- | --- | --- | --- | --- |
| `inInitGridDeformer` | `unity_native` 側で型登録 | △ | Dの package 初期化関数そのものは存在しないが、型登録自体は実施 | CP129 |
| `gridFormation` getter/setter | `gridFormation()/setGridFormation()` | ◯ | 対応済み | - |
| `switchDynamic` | `GridDeformer::switchDynamic` | ◯ | D同様に毎回 `setupChildNoRecurse` を再適用へ修正済み | CP129 |
| `vertices()` override | `Deformable::vertices` フィールド使用 | △ | Dは `vertexBuffer` 明示、C++は基底 `vertices` を直接保持 | CP129 |
| `rebuffer` | `GridDeformer::rebuffer` | ◯ | D同様の fallback (`DefaultAxis`) と `clearCache` | - |
| `typeId` | `typeId() const` | ◯ | 一致 | - |
| `runPreProcessTask` | `GridDeformer::runPreProcessTask` | ◯ | `inverseMatrix = globalTransform.toMat4().inverse()` へ修正済み | - |
| `build` | `GridDeformer::build` | ◯ | D同様 `setupChild -> setupSelf -> Node::build`。余計な `refresh()` を削除済み | CP129 |
| `clearCache` | `GridDeformer::clearCache` | ◯ | D同様 no-op | - |
| `setupChild` | `GridDeformer::setupChild` | ◯ | D同様に propagate 再帰 | - |
| `releaseChild` | `GridDeformer::releaseChild` | ◯ | D同様に propagate 再帰解除 | - |
| `captureTarget` | `GridDeformer::captureTarget` | ◯ | prepend で hook 追加 | - |
| `releaseTarget` | `GridDeformer::releaseTarget` | ◯ | 一致 | - |
| `runRenderTask` | `GridDeformer::runRenderTask` | ◯ | no-op 一致 | - |
| `deformChildren` | `GridDeformer::deformChildren` | ◯ | 戻り値/受け渡し型を D と同じ (`Vec2Array`, `mat4*`, `bool`) に統一済み | CP129, CP147 |
| `applyDeformToChildren` | `GridDeformer::applyDeformToChildren` | ◯ | `applyDeformToChildrenInternal` 経由で D 同等の転送処理へ統合済み | CP129, CP146 |
| `copyFrom` | `GridDeformer::copyFrom` | ◯ | D同様 fallback と `clearCache` 追加済み | CP129 |
| `coverOthers` | `coverOthers() const` | ◯ | 一致 | - |
| `mustPropagate` | `mustPropagate() const` | ◯ | 一致 | - |
| `serializeSelfImpl` | `GridDeformer::serializeSelfImpl` | ◯ | D同様に軸/formation/dynamic を常時書き込みへ修正済み | CP129 |
| `deserializeFromFghj` | `GridDeformer::deserializeFromFghj` | ◯ | `formation` が文字列（`"Bilinear"`）のモデルで C++ が `int` 変換例外を起こしていた差分を修正し、D同様に文字列/数値の両方で復元可能にした | CP148 |
| `cols/rows/hasValidGrid` | `cols()/rows()/hasValidGrid()` | ◯ | 一致 | - |
| `gridIndex` | `gridIndex` | ◯ | 一致 | - |
| `normalizeAxis` | `normalizeAxis` | ◯ | 一致 | - |
| `axisIndexOfValue` | `axisIndexOfValue` | ◯ | 一致 | - |
| `rebuildVertices/rebuildBuffers` | `setGridAxes` 内で統合 | △ | C++は独立関数を持たず同等処理を `setGridAxes` に集約 | CP129 |
| `setGridAxes` | `setGridAxes` | ◯ | 一致 | - |
| `adoptGridFromAxes` | `adoptGridFromAxes` | ◯ | 一致 | - |
| `deriveAxes` | `deriveAxes` | ◯ | `xAt/yAt` ベースで一致 | - |
| `adoptFromVertices` | `adoptFromVertices` | ◯ | 一致 | - |
| `fillDeformationFromPositions` | `fillDeformationFromPositions` | ◯ | 一致 | - |
| `matrixIsFinite` | `matrixIsFinite` | ◯ | 一致 | - |
| `computeCache` | `computeCache` | ◯ | 一致 | - |
| `locateInterval` | `locateInterval` | ◯ | 一致 | - |
| `sampleGridPoints` | `sampleGridPoints` | ◯ | `xAt/yAt` 化済みで D と同等 | - |
| `setupChildNoRecurse` | `setupChildNoRecurse` | △ | Dは関数ポインタ同値で upsert/remove。C++は `stage+tag` と lambda bridge | - |
| `releaseChildNoRecurse` | `releaseChildNoRecurse` | △ | Dは関数ポインタ同値 remove。C++は `stage+tag` remove | - |

## C++側にのみ存在する補助
| C++独自関数 | 内容 | 判定 |
| --- | --- | --- |
| `isFiniteMatrix` (namespace local) | 行列 finite 判定のローカル補助 | △ |
| `locate(float x, float y)` | `computeCache` を `optional` で包む補助 | △ |

## 行単位差分（フィルタ関連）
1. `applyDeformToChildren` の呼び出し構造
`nijilive/source/nijilive/core/nodes/deformer/grid.d:257-272`
`nicxlive/core/nodes/grid_deformer.cpp:370-390`
差分:
- D: `_applyDeformToChildren(tuple(1,&deformChildren), &update, &transfer, ...)`。
- C++: `applyDeformToChildrenInternal(...)` 呼び出しで同等処理。

2. `setupChildNoRecurse` の hook 形式
`nijilive/source/nijilive/core/nodes/deformer/grid.d:751-774`
`nicxlive/core/nodes/grid_deformer.cpp:323-359`
差分:
- D: `tuple(1, &deformChildren)` を pre/post に upsert/remove。
- C++: `FilterHook{stage=1, tag=this}` + lambda (`deformChildren`) を登録。

3. `releaseChildNoRecurse` の hook 解除方式
`nijilive/source/nijilive/core/nodes/deformer/grid.d:778-780`
`nicxlive/core/nodes/grid_deformer.cpp:361-368`
差分:
- D: `removeByValue(tuple(1,&deformChildren))`。
- C++: `erase_if(stage==1 && tag==this)`。

4. `deformChildren` の戻り型・受け渡し型
`nijilive/source/nijilive/core/nodes/deformer/grid.d:167-254`
`nicxlive/core/nodes/grid_deformer.cpp:200-292`
差分: なし（C++ も `DeformResult{Vec2Array, Mat4*, bool}` に移行済み）。

5. `runPreProcessTask` の inverse 行列元
`nijilive/source/nijilive/core/nodes/deformer/grid.d:94-100`
`nicxlive/core/nodes/grid_deformer.cpp:294-301`
差分: なし（C++ も `globalTransform.toMat4().inverse()` に修正済み）。

## 重点残課題
1. hook同一性判定（delegate同値 vs `stage+tag`）の厳密一致要否を判断する。
2. Grid/Path で共通化可能な補助（finite判定・cache補助）の整理方針を決める。
