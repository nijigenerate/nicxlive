# command_emitter 実装互換性チェック (D `nijilive.core.render.command_emitter` ↔ C++ `core/render/command_emitter.*`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `beginFrame(RenderBackend, ref RenderGpuState)` | state を初期化、バックエンド設定 | state=init、backend 設定、共有バッファ upload | ◯ |
| `endFrame(RenderBackend, ref RenderGpuState)` | 終了処理 | backend/state クリア | ◯ |
| `drawPart(Part, bool)` | backend->drawPart | `drawPartPacket(const PartDrawPacket&)` に名称変更 | △ |
| `beginDynamicComposite(Projectable, DynamicCompositePass)` | backend に pass 渡し | 同等（`pass.surface` 必須チェックあり） | ◯ |
| `endDynamicComposite(Projectable, DynamicCompositePass)` | backend に pass 渡し | 同等（`pass.surface` 必須チェックあり） | ◯ |
| `beginMask(bool useStencil)` | 即時に backend->beginMask | QueueEmitter は pendingMask に保持し、applyMask 成立時に BeginMask を記録 | △ |
| `applyMask(Drawable, bool)` | backend->applyMask | tryMakeMaskApplyPacket 成功時のみ ApplyMask 記録 | ◯ |
| `beginMaskContent()` | backend->beginMaskContent | pendingMask 中は記録せず破棄（D と同様） | ◯ |
| `endMask()` | backend->endMask | pendingMask 時は EndMask を記録しない（遅延 begin 前提） | △ |
| `QueueCommandEmitter::playback(RenderBackend*)` | D interface には非定義 | 記録済み queue を backend に再生 | ✗（削除候補） |
| `QueueCommandEmitter::queue() const` | D 側に対応項目なし | queue 参照アクセサ | ✗（削除候補） |
| `QueueCommandEmitter::recorded() const` | D 側に対応項目なし | queue 参照アクセサ（`queue()` と同義） | ✗（削除候補） |
| `QueueCommandEmitter::backendQueue()` | D 側に対応項目なし | backend queue 取得用 private メソッド | ✗（削除候補） |
| `QueueCommandEmitter::record(...)` | D 側に対応項目なし | `QueuedCommand` 追加用 private メソッド | ✗（削除候補） |
| `RenderQueue::ready()` | D 側に明示メソッドなし | backend/state の有効性チェック用 private メソッド | ✗（削除候補） |
| `RenderQueue::uploadSharedBuffers()` | beginFrame 経由で共有バッファ upload | 同等（dirty 判定 + markUploaded） | ◯ |
| `QueueCommandEmitter::uploadSharedBuffers()` | beginFrame 経由で共有バッファ upload | 同等（dirty 判定 + markUploaded） | ◯ |
| フィールド `RenderQueue.activeBackend_` | D 側は実装詳細非公開 | current backend ポインタ保持 | △ |
| フィールド `RenderQueue.frameState_` | D 側は実装詳細非公開 | frame state ポインタ保持 | △ |
| フィールド `QueueCommandEmitter.backend_` | D 側は backend 参照保持（実装言語差あり） | `shared_ptr<QueueRenderBackend>` 保持 | ◯ |
| フィールド `QueueCommandEmitter.activeBackend_` | D 側は実装詳細非公開 | beginFrame/endFrame で設定/クリア | △ |
| フィールド `QueueCommandEmitter.pendingMask` | D 側は beginMask 遅延発行の内部状態 | 同等に保持 | ◯ |
| フィールド `QueueCommandEmitter.pendingMaskUsesStencil` | D 側は beginMask 遅延発行の内部状態 | 同等に保持 | ◯ |

**現状**: 1項目1行で再整理。D 正準で見ると、C++ 側の queue 専用補助 API は削除候補、インターフェース差分は `drawPart` 名称差と mask 遅延発行挙動。 
