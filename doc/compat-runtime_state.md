# runtime_state 実装互換性チェック (D ↔ C++)

| 関数/機能 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| inPushViewport / inPopViewport | ビューポート幅高をスタック管理しカメラも連動 | 同等にスタック管理（width/height + カメラをpush/pop） | ◯ |
| inSetViewport / inGetViewport | 現在のビューポート設定・取得 | 同等の設定・取得のみ（リサイズ通知なし） | △ |
| inPushCamera / inPushCamera(camera) / inPopCamera | カメラスタックを管理 | 同等にスタック管理 | ◯ |
| inGetCamera / inSetCamera | カメラを取得・設定（未設定なら新規作成） | 同等 | ◯ |
| inEnsureViewportForTests / inEnsureCameraStackForTests | テスト用にスタックを初期化 | 未実装 | ✗ |
| inViewportDataLength / inDumpViewport | ビューポートのピクセルダンプ | 未実装 | ✗ |
| inSetClearColor / inGetClearColor | クリアカラーの設定・取得 | 未実装 | ✗ |
| render backend accessors (tryRenderBackend/requireRenderBackend/currentRenderBackend) | Backend の取得・初期化 | 未実装（common.cpp で get/setCurrentRenderBackend のみ） | ✗ |
| render target handles (inGetRenderImage 等) | RenderBackend から各ハンドル取得 | 未実装 | ✗ |
| initRendererCommon / initRenderer | ノード初期化・backend初期化 | 未実装 | ✗ |
| difference aggregation API | 差分評価の有効化/実行/取得 | 未実装 | ✗ |

※ 現状、ビューポート/カメラ周りの基本スタックは移植済み。その他のレンダーバックエンド連携・ダンプ系・差分評価は未移植。
