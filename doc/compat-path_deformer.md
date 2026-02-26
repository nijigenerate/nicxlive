# PathDeformer 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| フィールド `inverseMatrix` | transform 逆行列キャッシュ | 実装 | ◯ |
| フィールド `driver` | 物理ドライバ | Connected* ドライバ | ◯ |
| フィールド `physicsEnabled` | 物理有効状態 | 同等 | ◯ |
| フィールド `physicsOnly` | 物理のみ適用フラグ | 同等 | ◯ |
| フィールド `dynamicDeformation` | 動的変形フラグ | 同等 | ◯ |
| フィールド `controlPoints` | パス制御点 | std::vector<Vec2>＋再バッファ対応 | ◯ |
| フィールド `deformation` | Vec2Array | Deformableから継承 | ◯ |
| フィールド `strength` | 影響度 | 同等 | ◯ |
| フィールド `curveSamples` | 曲線サンプル | rebufferで生成 | ◯ |
| フィールド `curveNormals` | 法線 | rebufferで生成 | ◯ |
| フィールド `curveType` | Bezier/Spline | enum＋曲線生成で利用 | ◯ |
| フィールド `physicsType` | Pendulum/SpringPendulum | enum＋driver生成で利用 | ◯ |
| フィールド `frameCounter` | フレームカウント | 診断フレームで更新 | ◯ |
| フィールド `diagnostics` | invalid カウント/log | `DiagnosticsState diagnostics` を保持し、既存の invalid 追跡と同期 | ◯ |
| ctor | parent+タスク登録+初期化 | preProcess登録＋曲線/driver初期化 | ◯ |
| `typeId` | "PathDeformer"/alias | 同等 | ◯ |
| `runPreProcessTask` | transform更新＋安全逆行列＋deform更新 | 有限検証→inverse→Deformable→applyPathDeform＋診断ログ | ◯ |
| `runRenderTask` | GPUなし | 同等 | ◯ |
| `deformChildren` | パスへ投影し補間・物理適用・健全性検証 | 最近点→tan/normal射影→逆変換＋sanitizeOffsets＋invalid記録＋driver時notifyChange | ◯ |
| `deform(Vec2Array)` | `deformedCurve = createCurve(...)` | `deformedCurve` 更新を実装済み | ◯ |
| `applyPathDeform`（driver初期化） | 初回 `driver.setup()` | `PhysicsDriver::setup()` を追加し同等化 | ◯ |
| `applyDeformToChildren` | `_applyDeformToChildren(tuple(1,&deformChildren),...)` 経路 | 独自再帰で手動適用（NodeFilterMixin共通経路を使用しない） | ✗ |
| `applyDeformToChildren` 終端 | `physicsOnly = true; rebuffer(Vec2Array())` | `physicsOnly = true; rebuffer(std::vector<Vec2>{})` | ◯ |
| `switchDynamic` | 物理/動的切替 | physicsEnabled切替＋driver再生成・診断リセット・physicsOnly/dynamicDeformation反映 | ◯ |
| `setupChild` | フィルタ登録 | stage+tag で登録/解除を一致化 | △ |
| `releaseChild` | フィルタ解除 | stage+tag で解除を一致化 | △ |
| `captureTarget` | children_ref 操作＋フィルタ調整 | 同等 | ◯ |
| `releaseTarget` | children_ref 操作＋フィルタ調整 | 同等 | ◯ |
| `clearCache` | キャッシュクリア | 曲線長・invalidLog・meshCaches クリア | ◯ |
| `build` | 子setup→self→super | 子setup→self→`refresh()`→`Node::build` | △ |
| `copyFrom` | Path/Grid/Drawable からコピー | 各フィールドコピー＋rebuffer | ◯ |
| `serializeSelfImpl` | ドライバ/曲線種/物理状態保存 | 曲線種/物理フラグ/制御点＋driver type/state 保存 | ◯ |
| `deserializeFromFghj` | 上記復元＋キャッシュ更新 | 曲線/driver type/state 復元＋rebuffer | ◯ |
| `coverOthers` | true | 実装 | ◯ |
| `mustPropagate` | true | true | ◯ |

## 行単位差分（フィルタ関連）
1. `setupChildNoRecurse` の hook 形式
`nijilive/source/nijilive/core/nodes/deformer/path.d:917-935`
`nicxlive/core/nodes/path_deformer.cpp:1172-1205`
差分:
- D: `tuple(1, &deformChildren)` を upsert/remove。
- C++: `FilterHook{stage=1, tag=this}` + lambda を pre/post に insert。

2. `releaseChildNoRecurse` の hook 解除方式
`nijilive/source/nijilive/core/nodes/deformer/path.d:957-960`
`nicxlive/core/nodes/path_deformer.cpp:1208-1216`
差分:
- D: 関数ポインタ同値で remove。
- C++: `stage+tag` で remove。

3. `applyDeformToChildren` の実装構造
`nijilive/source/nijilive/core/nodes/deformer/path.d:1163-1196`
`nicxlive/core/nodes/path_deformer.cpp:1219-1280`
差分:
- D: `_applyDeformToChildren(...)` 経由。
- C++: パラメータループ/再帰 transfer を関数内に直接実装。

4. `applyDeformToChildren` の non-deformable 伝播条件
`nijilive/source/nijilive/core/nodes/deformer/path.d:1191-1194`
`nicxlive/core/nodes/path_deformer.cpp:1248-1265`
差分:
- D: `transfer() { return false; }` で非 Deformable 伝播なし。
- C++: `if (translateChildren)` 分岐で non-deformable にも offset 伝播を実施。

5. `build` 手順
`nijilive/source/nijilive/core/nodes/deformer/path.d:908-915`
`nicxlive/core/nodes/path_deformer.cpp:1165-1169`
差分:
- C++のみ `refresh()` 呼び出しが追加されている。
