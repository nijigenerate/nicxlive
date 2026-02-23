# command_emitter 実装互換性チェック (D `nijilive.core.render.command_emitter` ↔ C++ `core/render/command_emitter.*`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `beginFrame(RenderBackend, ref RenderGpuState)` | state を初期化、バックエンド設定 | state=init、backend 設定、共有バッファ upload | ◯ |
| `endFrame(RenderBackend, ref RenderGpuState)` | 終了処理 | backend/state クリア | ◯ |
| `drawPart(Part, bool)` | backend->drawPart | 基底 `drawPart` から `drawPartPacket` へ委譲 | ◯ |
| `beginDynamicComposite(Projectable, DynamicCompositePass)` | backend に pass 渡し | 同等（`pass.surface` 必須チェックあり） | ◯ |
| `endDynamicComposite(Projectable, DynamicCompositePass)` | backend に pass 渡し | 同等（`pass.surface` 必須チェックあり） | ◯ |
| `beginMask(bool useStencil)` | 遅延 begin（applyMask 成立時に記録） | `pendingMask` で同等実装 | ◯ |
| `applyMask(Drawable, bool)` | backend->applyMask | tryMakeMaskApplyPacket 成功時のみ ApplyMask 記録 | ◯ |
| `beginMaskContent()` | backend->beginMaskContent | pendingMask 中は記録せず破棄（D と同様） | ◯ |
| `endMask()` | backend->endMask | pendingMask 時は EndMask を記録しない（D と同等） | ◯ |
| `QueueCommandEmitter::playback(RenderBackend*)` | D interface には非定義 | 削除済み | ◯ |
| `QueueCommandEmitter::queue() const` | D 側に対応項目なし | 削除済み | ◯ |
| `QueueCommandEmitter::recorded() const` | D 側に対応項目なし | 削除済み | ◯ |
| `QueueCommandEmitter::backendQueue()` | D 側に対応項目なし | 削除済み | ◯ |
| `QueueCommandEmitter::record(...)` | D 側に対応項目なし | 削除済み | ◯ |
| `RenderQueue::ready()` | D 側に明示メソッドなし | `RenderQueue` クラスごと削除済み | ◯ |
| `RenderQueue::uploadSharedBuffers()` | beginFrame 経由で共有バッファ upload | `RenderQueue` クラスごと削除済み | ◯ |
| `QueueCommandEmitter::uploadSharedBuffers()` | beginFrame 経由で共有バッファ upload | 同等（dirty 判定 + markUploaded） | ◯ |
| フィールド `RenderQueue.activeBackend_` | D 側は実装詳細非公開 | `RenderQueue` クラスごと削除済み | ◯ |
| フィールド `RenderQueue.frameState_` | D 側は実装詳細非公開 | `RenderQueue` クラスごと削除済み | ◯ |
| フィールド `QueueCommandEmitter.backend_` | D 側は backend 参照保持（実装言語差あり） | `shared_ptr<QueueRenderBackend>` 保持 | ◯ |
| フィールド `QueueCommandEmitter.activeBackend_` | D 側は実装詳細非公開 | beginFrame/endFrame で設定/クリア | ◯ |
| フィールド `QueueCommandEmitter.pendingMask` | D 側は beginMask 遅延発行の内部状態 | 同等に保持 | ◯ |
| フィールド `QueueCommandEmitter.pendingMaskUsesStencil` | D 側は beginMask 遅延発行の内部状態 | 同等に保持 | ◯ |

**現状**: D 正準との差分（削除候補 API / mask 挙動）は反映済み。 
