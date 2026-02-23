# runtime_state 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 関数/機能 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| inPushViewport | ビューポート幅高をスタック管理しカメラも連動 | 同等 | ◯ |
| inPopViewport | ビューポート幅高をスタック管理しカメラも連動 | 同等 | ◯ |
| inSetViewport | 現在のビューポート設定 | backend resize 通知まで実装 | ◯ |
| inGetViewport | 現在のビューポート取得 | 同等 | ◯ |
| inPushCamera | カメラスタックを管理 | 同等 | ◯ |
| inPushCamera(camera) | カメラスタックを管理 | 同等 | ◯ |
| inPopCamera | カメラスタックを管理 | 同等 | ◯ |
| inGetCamera | カメラを取得 | 同等 | ◯ |
| inSetCamera | カメラを設定（未設定なら新規作成） | 同等 | ◯ |
| inEnsureViewportForTests | テスト用にスタックを初期化 | 実装済み | ◯ |
| inEnsureCameraStackForTests | テスト用にスタックを初期化 | 実装済み | ◯ |
| inViewportDataLength | ビューポートのピクセル長を返す | 実装済み | ◯ |
| inDumpViewport | ビューポートのピクセルダンプ | 実装済み（backend dump + 上下反転） | ◯ |
| inSetClearColor | クリアカラー設定 | 実装済み | ◯ |
| inGetClearColor | クリアカラー取得 | 実装済み | ◯ |
| tryRenderBackend | Backend の取得 | 実装済み（未設定時は fallback backend 作成） | ◯ |
| requireRenderBackend | Backend の取得/初期化 | 実装済み | ◯ |
| currentRenderBackend | Backend の現行取得 | 実装済み | ◯ |
| render target handles (inGetRenderImage 等) | RenderBackend から各ハンドル取得 | 実装済み（一部は 0 フォールバック） | △ |
| initRendererCommon | ノード初期化・backend初期化 | 実装済み（viewport/clearColor 初期化） | △ |
| initRenderer | ノード初期化・backend初期化 | 実装済み | ◯ |
| difference aggregation API | 差分評価の有効化/実行/取得 | 実装済み（backend 委譲） | ◯ |

※ `initRendererCommon` のノード/パラメータ初期化部分は、D の初期化シーケンスに対して引き続き追従が必要。

