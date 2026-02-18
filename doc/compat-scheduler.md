# scheduler 実装互換性チェック (D `nijilive.core.render.scheduler` ↔ C++ `core/render/scheduler.*`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `TaskOrder` 列挙 | Init/Parameters/PreProcess/Dynamic/Post0/Post1/Post2/RenderBegin/Render/RenderEnd/Final | 同順序・同値 | ◯ |
| `TaskKind` 列挙 | Init/Parameters/PreProcess/Dynamic/PostProcess/Render/Finalize | 同等 | ◯ |
| `RenderContext.renderGraph` | RenderGraphBuilder* | 同等 | ◯ |
| `RenderContext.renderBackend` | RenderBackend 参照保持（クラス参照） | RenderBackend* 参照保持 | ◯ |
| `RenderContext.gpuState` | RenderGpuState 値保持 | 同等 | ◯ |
| `RenderContext.frameId` | 未定義 | 削除済み | ◯ |
| `TaskHandler` | `delegate(ref RenderContext)` | `std::function<void(RenderContext&)>` | ◯ |
| `Task` フィールド | order/kind/handler | 同等 | ◯ |
| `TaskScheduler.queues` | Task[][TaskOrder] | `std::map<TaskOrder, std::vector<Task>>` | ◯ |
| `TaskScheduler.orderSequence` | コンストラクタで全順序を保持 | 同等 | ◯ |
| `clearTasks` | 各キュー length=0 | 同等（clear） | ◯ |
| `addTask` | キューへ push | 同等 | ◯ |
| `executeRange` | start/end で順次実行＋profileScope(label文字列) | 同等（labelは数値化した文字列） | ◯ |
| `execute` | Init→Final 全実行 | 同等 | ◯ |

**現状**: スケジューラ層は D 実装と整合。***

