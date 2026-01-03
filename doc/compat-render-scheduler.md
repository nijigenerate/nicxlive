# render/scheduler.hpp 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| enum `TaskOrder` | Init〜RenderEnd まで列挙 | 同等 | ◯ |
| enum `TaskKind` | Init〜Finalize まで列挙 | 同等 | ◯ |
| method `clearTasks()` | キューをクリア | 同等 | ◯ |
| method `addTask()` | order/kindと関数を登録 | kind未使用だが登録は可能（関数キューは同等） | ◯ |
| method `executeRange()` | order範囲でタスク実行 | 同等 | ◯ |
| 内部構造 `tasks_` | (order,fn) の配列 | 同等 | ◯ |
