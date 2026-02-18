# Binding 実装互換性チェック (D `nijilive.core.param.binding` ↔ C++ `core/param/binding*.hpp`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| フィールド `BindTarget.target` | `Resource` 参照 | `target` フィールドで参照を保持（Node 実体は weak_ptr） | ◯ |
| フィールド `BindTarget.name` | 名前保持 | 同等 | ◯ |
| `reconstruct` | no-op | no-op | ◯ |
| `finalize` | puppet 経由で node 再解決 | `resolvePuppetNodeById` で uuid 解決 | ◯ |
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
| `getNodeUUID` | nodeRef から UUID 返却 | `nodeUuid_` を返却 | ◯ |
| `interpolateMode` getter | 取得 | 実装あり | ◯ |
| `interpolateMode` setter | 設定 | 実装あり | ◯ |
| `serializeSelf` | target/values/isSet/interpolate_mode をシリアライズ | 実装済み | ◯ |
| `deserializeFromFghj` | Fghj から復元＋整合性検証 | 実装済み（軸数整合性チェックあり） | ◯ |
| `clearValue (Value)` | 型に応じ既定値に戻す | 実装あり | ◯ |
| `clearValue (Deformation)` | vertexOffsets をクリア | 実装あり | ◯ |
| `clearValue (ParameterParameter)` | 参照パラメータのデフォルトに戻す | 実装あり | ◯ |
| `isCompatibleWithNode` | node 型検証 | Value/Deformation/ParameterParameter を D 基準で判定 | ◯ |

**現状**: binding API は D 基準で実装済み。***

