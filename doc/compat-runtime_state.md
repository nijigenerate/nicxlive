# runtime_state 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 関数/機能 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| inPushViewport | ビューポート幅高をスタック管理しカメラも連動 | 同等 | ◯ |
| inPopViewport | ビューポート幅高をスタック管理しカメラも連動 | 同等 | ◯ |
| inSetViewport | 現在のビューポート設定 | 同等の設定（リサイズ通知なし） | △ |
| inGetViewport | 現在のビューポート取得 | 同等の取得（リサイズ通知なし） | △ |
| inPushCamera | カメラスタックを管理 | 同等 | ◯ |
| inPushCamera(camera) | カメラスタックを管理 | 同等 | ◯ |
| inPopCamera | カメラスタックを管理 | 同等 | ◯ |
| inGetCamera | カメラを取得 | 同等 | ◯ |
| inSetCamera | カメラを設定（未設定なら新規作成） | 同等 | ◯ |
| inEnsureViewportForTests | テスト用にスタックを初期化 | 未実装 | ✗（未実装） |
| inEnsureCameraStackForTests | テスト用にスタックを初期化 | 未実装 | ✗（未実装） |
| inViewportDataLength | ビューポートのピクセル長を返す | 未実装 | ✗（未実装） |
| inDumpViewport | ビューポートのピクセルダンプ | 未実装 | ✗（未実装） |
| inSetClearColor | クリアカラー設定 | 未実装 | ✗（未実装） |
| inGetClearColor | クリアカラー取得 | 未実装 | ✗（未実装） |
| tryRenderBackend | Backend の取得 | 未実装 | ✗（未実装） |
| requireRenderBackend | Backend の取得/初期化 | 未実装 | ✗（未実装） |
| currentRenderBackend | Backend の現行取得 | 未実装（common.cpp の get/set とは別） | ✗（未実装） |
| render target handles (inGetRenderImage 等) | RenderBackend から各ハンドル取得 | 未実装 | ✗（未実装） |
| initRendererCommon | ノード初期化・backend初期化 | 未実装 | ✗（未実装） |
| initRenderer | ノード初期化・backend初期化 | 未実装 | ✗（未実装） |
| difference aggregation API | 差分評価の有効化/実行/取得 | 未実装 | ✗（未実装） |

※ 現状、ビューポート/カメラ周りの基本スタックは移植済み。その他のレンダーバックエンド連携・ダンプ系・差分評価は未移植。

