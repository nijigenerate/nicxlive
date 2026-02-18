# SimplePhysicsDriver 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| ctor `(uuid,parent)` | super(uuid,parent)＋reset | uuid/parent ctor＋reset | ◯ |
| field `paramRef` | 参照パラメータ UUID | 保存＋復元、親Puppet経由で解決 | ◯ |
| field `param_` | Parameter 実体 | shared_ptr を保持し解決時にキャッシュ | ◯ |
| field `active` | 有効フラグ | active フィールドあり | ◯ |
| field `modelType` | Pendulum/SpringPendulum | enum として保持 | ◯ |
| field `mapMode` | AngleLength/XY/LengthAngle/YX | enum として保持 | ◯ |
| field `localOnly` | ローカル座標のみ参照 | フラグ保持 | ◯ |
| field `gravity` | 重力スケール | gravity フィールド保持 | ◯ |
| field `length` | 自然長 | length フィールド保持 | ◯ |
| field `frequency` | 周波数 | frequency フィールド保持 | ◯ |
| field `angleDamping` | 角度減衰 | angleDamping フィールド保持 | ◯ |
| field `lengthDamping` | 長さ減衰 | lengthDamping フィールド保持 | ◯ |
| field `outputScale` | 出力スケール | outputScale[2] 保持 | ◯ |
| field `offsetGravity` | 重力オフセット | フィールド保持 | ◯ |
| field `offsetLength` | 長さオフセット | フィールド保持 | ◯ |
| field `offsetFrequency` | 周波数オフセット | フィールド保持 | ◯ |
| field `offsetAngleDamping` | 角度減衰オフセット | フィールド保持 | ◯ |
| field `offsetLengthDamping` | 長さ減衰オフセット | フィールド保持 | ◯ |
| field `offsetOutputScale` | 出力スケールオフセット | フィールド保持 | ◯ |
| field `anchor` | アンカー座標 | anchor フィールド保持 | ◯ |
| field `output` | 出力座標 | output フィールド保持 | ◯ |
| field `prevAnchor` | 前回アンカー | prevAnchor フィールド保持 | ◯ |
| field `prevTransMat` | 前回変換行列 | prevTransMat フィールド保持 | ◯ |
| field `prevAnchorSet` | アンカーセット済み | prevAnchorSet フィールド保持 | ◯ |
| field `simPhase` | シミュレーション位相 | simPhase フィールド保持 | ◯ |
| method `typeId` | "SimplePhysics" | 実装 | ◯ |
| method `runBeginTask` | オフセット初期化 | offset群を初期化 | ◯ |
| method `runPreProcessTask` | アンカー更新 | updateInputs でアンカー更新 | ◯ |
| method `runPostTaskImpl` | アンカー/出力更新 | updateOutputs で Parameter へ書き込み | ◯ |
| method `updateDriver` | 物理ステップ実行（deltaTime→0.01刻みでPhysicsSystem tick→updateOutputs） | 同等のtick/ステップ／updateOutputs | ◯ |
| method `reset` | オフセット/状態初期化 | 同等（簡略） | ◯ |
| method `updateInputs` | アンカー取得（ローカル/グローバル） | transform から anchor を取得 | ◯ |
| method `updateOutputs` | 出力を Parameter に反映 | puppet 経由で UUID resolve → mapMode/スケール適用で書き込み | ◯ |
| method `logPhysicsState` | デバッグログ | stderr 出力 | ◯ |
| method `getAffectedParameters` | 影響パラメータ返却 | param_ を返却 | ◯ |
| method `affectsParameter` | UUID 比較で判定 | param_ UUID 比較 | ◯ |
| serializeSelfImpl `param` | param を保存 | 実装 | ◯ |
| serializeSelfImpl `model_type` | モデル種を保存 | 実装 | ◯ |
| serializeSelfImpl `map_mode` | マップモード保存 | 実装 | ◯ |
| serializeSelfImpl `gravity` | 重力保存 | 実装 | ◯ |
| serializeSelfImpl `length` | 長さ保存 | 実装 | ◯ |
| serializeSelfImpl `frequency` | 周波数保存 | 実装 | ◯ |
| serializeSelfImpl `angle_damping` | 角度減衰保存 | 実装 | ◯ |
| serializeSelfImpl `length_damping` | 長さ減衰保存 | 実装 | ◯ |
| serializeSelfImpl `output_scale` | 出力スケール保存 | 実装 | ◯ |
| serializeSelfImpl `local_only` | ローカル参照保存 | 実装 | ◯ |
| deserializeFromFghj `param` | param 復元 | 実装 | ◯ |
| deserializeFromFghj `model_type` | モデル復元 | 実装 | ◯ |
| deserializeFromFghj `map_mode` | マップ復元 | 実装 | ◯ |
| deserializeFromFghj `gravity` | 重力復元 | 実装 | ◯ |
| deserializeFromFghj `length` | 長さ復元 | 実装 | ◯ |
| deserializeFromFghj `frequency` | 周波数復元 | 実装 | ◯ |
| deserializeFromFghj `angle_damping` | 角度減衰復元 | 実装 | ◯ |
| deserializeFromFghj `length_damping` | 長さ減衰復元 | 実装 | ◯ |
| deserializeFromFghj `output_scale` | 出力スケール復元 | 実装 | ◯ |
| deserializeFromFghj `local_only` | ローカル参照復元 | 実装 | ◯ |
| drawDebug | デバッグ描画 | anchor→outputライン＋クロスを描画 | ◯ |

