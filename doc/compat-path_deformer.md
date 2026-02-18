# PathDeformer 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| フィールド `inverseMatrix` | transform 逆行列キャッシュ | 実装 | ◯ |
| フィールド driver | 物理ドライバ | Connected* ドライバ | ◯ |
| フィールド physicsEnabled | 物理有効状態 | 同等 | ◯ |
| フィールド physicsOnly | 物理のみ適用フラグ | 同等 | ◯ |
| フィールド dynamicDeformation | 動的変形フラグ | 同等 | ◯ |
| フィールド `controlPoints` | パス制御点 | std::vector<Vec2>＋再バッファ対応 | ◯ |
| フィールド `deformation` | Vec2Array | Deformableから継承 | ◯ |
| フィールド `physicsEnabled` | 物理有効 | 同等 | ◯ |
| フィールド `strength` | 影響度 | 同等 | ◯ |
| フィールド `curveSamples` | 曲線サンプル | rebufferで生成 | ◯ |
| フィールド `curveNormals` | 法線 | rebufferで生成 | ◯ |
| フィールド `curveType` | Bezier/Spline | enum＋曲線生成で利用 | ◯ |
| フィールド `physicsType` | Pendulum/SpringPendulum | enum＋driver生成で利用 | ◯ |
| フィールド `frameCounter` | フレームカウント | 診断フレームで更新 | ◯ |
| フィールド `diagnostics` | invalid カウント/log | 連続カウント・閾値無効化・曲線スケールログあり（詳細ログ簡略） | △ |
| ctor | parent+タスク登録+初期化 | preProcess登録＋曲線/driver初期化 | ◯ |
| `typeId` | "PathDeformer"/alias | 同等 | ◯ |
| `runPreProcessTask` | transform更新＋安全逆行列＋deform更新 | 有限検証→inverse→Deformable→applyPathDeform＋診断ログ | ◯ |
| `runRenderTask` | GPUなし | 同等 | ◯ |
| `deformChildren` | パスへ投影し補間・物理適用・健全性検証 | 最近点→tan/normal射影→逆変換＋sanitizeOffsets＋invalid記録＋driver時notifyChange | ◯ |
| `applyDeformToChildren` | translateChildren/physicsで伝達 | driver存在時 skip・dynamicDeformation skip・診断フレーム連動 | ◯ |
| `switchDynamic` 相当 | 物理/動的切替 | physicsEnabled切替＋driver再生成・診断リセット・physicsOnly/dynamicDeformation反映 | ◯ |
| setupChild | フィルタ登録 | 同等 | ◯ |
| eleaseChild | フィルタ解除 | 同等 | ◯ |
| captureTarget | children_ref 操作＋フィルタ調整 | 同等 | ◯ |
| eleaseTarget | children_ref 操作＋フィルタ調整 | 同等 | ◯ |
| `clearCache` | キャッシュクリア | 曲線長・invalidLog・meshCaches クリア | ◯ |
| `build` | 子setup→self→super | 子 setup→self→Node::build | ◯ |
| `copyFrom` | Path/Grid/Drawable からコピー | 各フィールドコピー＋rebuffer | ◯ |
| `serializeSelfImpl` | ドライバ/曲線種/物理状態保存 | 曲線種/物理フラグ/制御点＋driver type/state 保存 | ◯ |
| `deserializeFromFghj` | 上記復元＋キャッシュ更新 | 曲線/driver type/state 復元＋rebuffer | ◯ |
| `coverOthers` | true | 実装 | ◯ |
| `mustPropagate` | false | C++は true | ◯ |

