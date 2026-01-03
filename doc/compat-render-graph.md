# render/graph_builder.hpp 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| method `empty()` | タスク有無を返す | 同等 | ◯ |
| method `beginFrame()` | フレーム初期化 | No-Op | △ |
| method `playback()` | キューをEmitterへ再生 | タスクを順次実行（同等） | ◯ |
| method `addTask()` | コマンドを積む | 同等 | ◯ |
| method `pushDynamicComposite()` | DynamicCompositeをスタックしレンダーキューへ登録 | トークン採番のみ（Queueへの追加なし） | ✗ |
| method `popDynamicComposite()` | スタックから取り出し終了コマンドを積む | タスク追加でEmitterに委譲 | △ |
| method `applyMask()` | Mask命令をキューに積む | QueueEmitterに委譲（記録のみ） | △ |
| method `drawPart()` | Part描画命令をキューに積む | QueueEmitterに委譲（記録のみ） | △ |
| field `tasks_` | コマンドキュー | 同等 | ◯ |
| field `tokenCounter_` | DynamicComposite用トークン | 同等 | ◯ |
