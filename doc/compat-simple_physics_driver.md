# SimplePhysicsDriver 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| ctor `(uuid,parent)` | super(uuid,parent)＋reset | uuid/parent ctor＋reset | ◯ |
| field `paramRef`/`param_` | 参照パラメータ UUID と実体 | paramRef のみ（Param 解決なし） | △ |
| field `active` | 有効フラグ | 実装 | ◯ |
| field `modelType` | Pendulum/SpringPendulum | enum あり | ◯ |
| field `mapMode` | AngleLength/XY/LengthAngle/YX | enum あり | ◯ |
| field `localOnly` | ローカル座標のみ参照 | 実装 | ◯ |
| field `gravity/length/frequency/angleDamping/lengthDamping/outputScale` | 物理パラメータ | 実装 | ◯ |
| runtime offsets（offsetGravity 等） | オフセット適用 | 実装 | ◯ |
| runtime `anchor/output/prev*` | アンカー・出力追跡 | anchor/output/prevAnchor 追跡 | ◯ |
| method `typeId` | "SimplePhysics" | 実装 | ◯ |
| method `runBeginTask` | オフセット初期化 | 同等 | ◯ |
| method `preProcess`/`postProcess` | アンカー更新 | pre/post で anchor 入出力更新 | ◯ |
| method `updateDriver` | 物理ステップ実行（deltaTime/PhysicsSystem tick） | 簡易サイン波で出力更新（近似） | △ |
| method `reset` | オフセット/状態初期化 | 同等（簡略） | ◯ |
| method `updateAnchors`/`updateInputs`/`updateOutputs` | Anchor/出力反映と検証 | 入力更新・出力を Parameter に書き込み | △ |
| method `logPhysicsState` | デバッグログ | stderr 出力 | ◯ |
| method `getAffectedParameters`/`affectsParameter` | 影響パラメータ返却 | Driver 側に実装依存（未リゾルブ） | △ |
| serializeSelfImpl | param/model/map/physics params/output_scale/local_only を保存 | 同等フィールド保存 | ◯ |
| deserializeFromFghj | 上記復元＋初期化 | 同等フィールド復元 | ◯ |
| drawDebug | デバッグ描画 | 未実装 | ✗ |
