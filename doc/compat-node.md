# Node 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| `FilterHook` 同一性 | `(stage, func)` 単位で管理 | `stage + tag` で一意管理 | △ |
| `FilterHook` シグネチャ | `delegate(Node, Vec2Array, Vec2Array, mat4*) -> Tuple!(Vec2Array, mat4*, bool)` | `std::function(... const std::vector<Vec2>&, const std::vector<Vec2>&, const Mat4*) -> tuple<vector<Vec2>, optional<Mat4>, bool>` | ✗ |
| `serializeSelfImpl` | あり（flags対応） | transform/offset/子/型マップ対応 | ◯ |
| `serializeSelf` | あり | 同等 | ◯ |
| `serializePartial` | あり | 同等 | ◯ |
| `deserializeFromFghj` | あり（子型生成含む。子のdeserialize失敗は呼び出し元へ返す） | transform/offset/型マップ対応。子deserializeの例外を伝播 | ◯ |
| `forceSetUUID` | あり | 実装済み | ◯ |
| `inCreateUUID` | あり | 実装済み | ◯ |
| `inUnloadUUID` | あり | 実装済み | ◯ |
| `inClearUUIDs` | あり | 実装済み | ◯ |
| `runPreProcessTask` | preProcess 呼び出し | preProcess 呼び出し | ◯ |
| `runDynamicTask` | 空実装 | 空実装 | ◯ |
| `runPostTaskImpl` | ステージ別・override更新 | ステージ別・override更新 | ◯ |
| `runPostTask0` | あり | 同等 | ◯ |
| `runPostTask1` | あり | 同等 | ◯ |
| `runPostTask2` | あり | 同等 | ◯ |
| `runFinalTask` | post(-1)+flushNotify | post(-1)+flushNotify | ◯ |
| `runRenderTask` | 空実装 | 空実装 | ◯ |
| `runRenderBeginTask` | 空実装 | 空実装 | ◯ |
| `runRenderEndTask` | 空実装 | 空実装 | ◯ |
| `registerRenderTasks` | Task網羅・子 zsort | ほぼ同等（ラムダ登録） | ◯ |
| `determineRenderScopeHint` | Projectable判定 | Projectable動的/再利用判定 | ◯ |
| `transform` | TRS(quat)・override考慮・lockToRoot/puppet対応 | TRS合成＋puppet/oneTime/override適用 | ◯ |
| `getCombinedBounds` | Drawable+子+puppet変換 | Drawable+子+puppet補正 | ◯ |
| `getCombinedBoundsRect` | あり | 実装済み | ◯ |
| `reparent` | release/setupフック＋相対変換 | release/setup 伝播＋相対変換 | ◯ |
| `setupChild` | 実装あり | デフォルト停止（D同様） | ◯ |
| `releaseChild` | 実装あり | デフォルト停止（D同様） | ◯ |
| `setupSelf` | 実装あり | デフォルトno-op（D同様） | ◯ |
| `releaseSelf` | 実装あり | デフォルトno-op（D同様） | ◯ |
| `notifyChange` | puppet通知・理由付きリスナー | puppet通知＋親/リスナー再通知 | ◯ |
| `flushNotifyChange` | changePool→notify | 同等 | ◯ |
| `drawOrientation` | あり | デバッグラインをワールド座標でバッファに記録（描画は呼び出し側に委任） | ◯ |
| `drawBounds` | あり | バウンディングボックスのラインをワールド座標でバッファに記録 | ◯ |
| `normalizeUV` | UV正規化 | デフォルト空（D同等） | ◯ |
| `clearCache` | キャッシュクリア | デフォルト空（D同等） | ◯ |
| `canReparent` | 循環チェックあり | 実装済み | ◯ |
| `setOneTimeTransform` | MatrixHolder共有・子伝播・適用 | shared_ptr伝播・transform適用 | ◯ |
| `getOneTimeTransform` | あり | 保持のみ | ◯ |
| `setValue` | lockToRoot/pin連動 | 値更新・isFinite補正・transformChanged | ◯ |
| `scaleValue` | 軸別スケール対応 | 軸別スケール対応 | ◯ |
| RenderScopeHint | あり | 同等 | ◯ |
| Projectable 判定 | あり | 動的/再利用判定を実装 | ◯ |

### 補足
- ノードファクトリ／型登録 (`inRegisterNodeType`/`inHasNodeType`/`inInstantiateNode`) をC++側に実装済み。未知型はスキップし、Tmpノードも登録。
- UUID管理 (`inCreateUUID`/`inUnloadUUID`/`inClearUUIDs`) を実装済み。
- 残る差分はデバッグ描画バックエンドが未接続な点と、D側にある詳細コメント/DBG呼び出しによる行数増のみ。実装機能としては概ねパリティ達成。

### 行単位差分（フィルタ関連）
1. `Filter` 型定義
`nijilive/source/nijilive/core/nodes/node.d:244`
`nicxlive/core/nodes/node.hpp:90-92`
差分: D は `Vec2Array/mat4*`、C++ は `std::vector<Vec2>/std::optional<Mat4>`。

2. `preProcess` の filter 入出力型
`nijilive/source/nijilive/core/nodes/node.d:282-293`
`nicxlive/core/nodes/node.cpp:216-234`
差分: D は `Vec2Array localTrans/offsetTrans` をそのまま渡す。C++ は `std::vector<Vec2>` へ変換して渡す。

3. `postProcess` の filter 入出力型
`nijilive/source/nijilive/core/nodes/node.d:304-317`
`nicxlive/core/nodes/node.cpp:244-264`
差分: 上記と同様に C++ 側が `std::vector<Vec2>` 変換経路。
