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
| `runPreProcessTask` | `GridDeformer::runPreProcessTask` | ◯ | `transform` 更新後に inverse 再計算と `updateDeform` 実行 | - |
| `build` | `GridDeformer::build` | ◯ | D同様 `setupChild -> setupSelf -> Node::build`。余計な `refresh()` を削除済み | CP129 |
| `clearCache` | `GridDeformer::clearCache` | ◯ | D同様 no-op | - |
| `setupChild` | `GridDeformer::setupChild` | ◯ | D同様に propagate 再帰 | - |
| `releaseChild` | `GridDeformer::releaseChild` | ◯ | D同様に propagate 再帰解除 | - |
| `captureTarget` | `GridDeformer::captureTarget` | ◯ | prepend で hook 追加 | - |
| `releaseTarget` | `GridDeformer::releaseTarget` | ◯ | 一致 | - |
| `runRenderTask` | `GridDeformer::runRenderTask` | ◯ | no-op 一致 | - |
| `deformChildren` | `GridDeformer::deformChildren` | △ | `origDeformation.size()<origVertices.size()` 早期return と `cache.valid` invalid 扱いを削除し、非finiteのみ early return へ写経修正済み。残差分は実行結果差の継続調査 | CP129, CP147 |
| `applyDeformToChildren` | `GridDeformer::applyDeformToChildren` | △ | `transferChildren` 内での keypoint 算出・`transferCondition()` 呼び出し・更新順を D `filter.d` 手順へ写経反映済み。残差分は実行結果差（`Midori-gridtest2` の queue count 差） | CP129, CP146 |
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
| `setupChildNoRecurse` | `setupChildNoRecurse` | ◯ | `stage+tag` 管理で Dの upsert/remove と同等契約 | - |
| `releaseChildNoRecurse` | `releaseChildNoRecurse` | ◯ | 一致 | - |

## C++側にのみ存在する補助
| C++独自関数 | 内容 | 判定 |
| --- | --- | --- |
| `toVec2List/toVec2Array` | `Vec2Array` と `std::vector<Vec2>` 変換ヘルパ | △ |
| `isFiniteMatrix` (namespace local) | 行列 finite 判定のローカル補助 | △ |
| `locate(float x, float y)` | `computeCache` を `optional` で包む補助 | △ |

## 重点残課題
1. `Midori-gridtest2-20250426-1.6.2.inx` での `FRAME0/119 count` 差分（`nijilive: 244/234`, `nicxlive: 193/183`）要因を、`deformChildren` 入出力ログで点単位比較する。
2. `Midori-gridtest2` 以外（Aka/Midori本体）でも `deformChildren` 差分観測を拡張し、count差の再現ノード集合を固定する。
