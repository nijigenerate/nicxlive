# Parameter 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| クラス/メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| `Parameter` フィールド | uuid/name/active/value/min/max/defaults/axisPoints 等 | 主要フィールド保持（mergeMode/indexableName 等含む） | ◯ |
| `Parameter::getBinding` | bindings から取得 | 同等 | ◯ |
| `Parameter::getOrAddBinding` | key に応じて Value/Deformation を生成 | 同等（軸サイズ初期化） | ◯ |
| `Parameter::removeBinding` | bindings から削除 | 同等 | ◯ |
| `Parameter::normalizedValue` | min/max ベースで正規化取得 | 同等 | ◯ |
| `Parameter::setNormalizedValue` | min/max ベースで正規化設定 | 同等 | ◯ |
| `Parameter::makeIndexable` | インデックス名生成 | 簡易 | ◯ |
| `Parameter::update` | merge/add/mul/csum 適用＋ bindings apply | mergeMode 分岐＋ binding apply | ◯ |
| `ParameterBinding::apply` | あり | 実装（補間対応） | ◯ |
| `ParameterBinding::reInterpolate` | あり | 2D 補間/外挿を実装 | ◯ |
| `ParameterBinding::scaleValueAt` | あり | 実装 | ◯ |
| `ParameterBinding::extrapolateValueAt` | あり | 実装 | ◯ |
| `ParameterBinding::copyKeypointToBinding` | あり | 同等 | ◯ |
| `ParameterBinding::swapKeypointWithBinding` | あり | 同等 | ◯ |
| `ParameterBinding::reverseAxis` | あり | 同等 | ◯ |
| `ParameterBinding::getIsSet` | あり | 同等 | ◯ |
| `ParameterBinding::getSetCount` | あり | 同等 | ◯ |
| `ParameterBinding::moveKeypoints` | あり | 実装 | ◯ |
| `ParameterBinding::insertKeypoints` | あり | 実装 | ◯ |
| `ParameterBinding::deleteKeypoints` | あり | 実装 | ◯ |
| `ParameterBinding::getNodeUUID` | あり | target.node 参照 | ◯ |
| ParameterBinding::interpolateMode getter | あり | 実装 | ◯ |
| ParameterBinding::interpolateMode setter | あり | 実装（Nearest/Step/Linear/Cubic） | ◯ |
| `ValueParameterBinding` フィールド | values/isSet 保持 | 同等 | ◯ |
| `ValueParameterBinding` メソッド | apply/scale/reverseAxis など | 実装（scaleValue/clearValue 対応） | ◯ |
| `DeformationParameterBinding` フィールド | targetNode, values/isSet 保持 | DeformSlot で vertexOffsets (SoA) | ◯ |
| `DeformationParameterBinding` メソッド | apply/copy/scale | 軸別スケール対応・apply | ◯ |
| `ParameterParameterBinding` | 別パラメータバインド | 実装（値転送のみ） | ◯ |
| `Resource` | プレースホルダ | プレースホルダ | ◯ |
| 依存 Vec2Array(SoA) | あり | 同等 | ◯ |
| 依存 interpolate | あり | 同等 | ◯ |
| 依存 gather | あり | 同等 | ◯ |

