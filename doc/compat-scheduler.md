# scheduler 実装互換性チェック (D `nijilive.core.render.scheduler` ↔ C++ `core/render/scheduler.*`)

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `TaskOrder` 列挙 | Init/Parameters/PreProcess/Dynamic/Post0/Post1/Post2/RenderBegin/Render/RenderEnd/Final | 同順序・同値 | ◯ |
| `TaskKind` 列挙 | Init/Parameters/PreProcess/Dynamic/PostProcess/Render/Finalize | 同等 | ◯ |
| `RenderContext.renderGraph` | RenderGraphBuilder* | 同等 | ◯ |
| `RenderContext.renderBackend` | RenderBackend 値保持 | RenderBackend* | △（値→ポインタ） |
| `RenderContext.gpuState` | RenderGpuState 値保持 | 同等 | ◯ |
| `RenderContext.frameId` | 未定義 | `std::size_t frameId` 追加 | △（追加フィールド） |
| `TaskHandler` | `delegate(ref RenderContext)` | `std::function<void(RenderContext&)>` | ◯ |
| `Task` フィールド | order/kind/handler | 同等 | ◯ |
| `TaskScheduler.queues` | Task[][TaskOrder] | `std::map<TaskOrder, std::vector<Task>>` | ◯ |
| `TaskScheduler.orderSequence` | コンストラクタで全順序を保持 | 同等 | ◯ |
| `clearTasks` | 各キュー length=0 | 同等（clear） | ◯ |
| `addTask` | キューへ push | 同等 | ◯ |
| `executeRange` | start/end で順次実行＋profileScope(label文字列) | 同等（labelは数値化した文字列） | ◯ |
| `execute` | Init→Final 全実行 | 同等 | ◯ |

**現状**: スケジューラの挙動はほぼ一致。相違点は RenderContext の renderBackend が値→ポインタ、frameId 追加程度で互換性に大きな影響なし。***
