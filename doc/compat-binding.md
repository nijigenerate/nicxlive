# Binding 実装互換性チェック (D `nijilive.core.param.binding` ↔ C++ `core/param/binding*.hpp`)

| フィールド `BindTarget.target` | `Resource` 参照 | `weak_ptr<Node>` | △（Resource/UUID 不保持） |
| フィールド `BindTarget.name` | 名前保持 | 同等 | ◯ |
| `reconstruct` | puppet 経由で node 再解決 | no-op | ✗ |
| `finalize` | puppet 経由で node 再解決 | no-op | ✗ |
| `apply` | 左キーポイント＋オフセットで適用 | 実装あり | ◯ |
| `clear` | キーポイント初期化 | 実装あり | ◯ |
| `setCurrent` | 現値をセット | 実装あり | ◯ |
| `unset` | キーポイント解除 | 実装あり | ◯ |
| `reset` | デフォルトで再設定 | 実装あり | ◯ |
| `isSet` | キーポイント状態取得 | 実装あり | ◯ |
| `getIsSet` | isSet 配列参照 | 実装あり | ◯ |
| `getSetCount` | 設定済み数を返す | 実装あり | ◯ |
| `scaleValueAt` | 軸指定でスケール | 実装あり | ◯ |
| `extrapolateValueAt` | 軸外挿 | 実装あり | ◯ |
| `copyKeypointToBinding` | 他バインドへコピー | 実装あり | ◯ |
| `swapKeypointWithBinding` | 他バインドとスワップ | 実装あり | ◯ |
| `reverseAxis` | 軸反転 | 実装あり | ◯ |
| `moveKeypoints` | 軸上で移動 | 実装あり | ◯ |
| `insertKeypoints` | 軸上に挿入 | 実装あり | ◯ |
| `deleteKeypoints` | 軸上から削除 | 実装あり | ◯ |
| `reInterpolate` | 2D 補間/外挿 | 実装あり（同等ロジック） | ◯ |
| `getNodeUUID` | nodeRef から UUID 返却 | weak_ptr 解決時のみ UUID 取得 | △ |
| `interpolateMode` getter | 取得 | 実装あり | ◯ |
| `interpolateMode` setter | 設定 | 実装あり | ◯ |
| `serializeSelf` | target/values/isSet/interpolate_mode をシリアライズ | 未実装 | ✗ |
| `deserializeFromFghj` | Fghj から復元＋整合性検証 | 未実装 | ✗ |
| `clearValue (Value)` | 型に応じ既定値に戻す | 実装あり | ◯ |
| `clearValue (Deformation)` | vertexOffsets をクリア | 実装あり | ◯ |
| `clearValue (ParameterParameter)` | 参照パラメータのデフォルトに戻す | 実装あり | ◯ |
| `isCompatibleWithNode` | node 型検証 | Deformation/Value は常に true、ParameterParameter は false 固定 | △ |

**現状**: キーポイント操作・補間・軸操作は概ね写経済み。未対応はシリアライズ/デシリアライズ、puppet 再解決（reconstruct/finalize）、Resource/UUID ベースのターゲット保持。互換性チェックや UUID 参照も D 版ほど厳密ではない。***
