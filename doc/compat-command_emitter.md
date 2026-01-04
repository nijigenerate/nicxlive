# command_emitter 実装互換性チェック (D `nijilive.core.render.command_emitter` ↔ C++ `core/render/command_emitter.*`)

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| インターフェース `RenderCommandEmitter` | beginFrame/endFrame/beginMask/applyMask/beginMaskContent/endMask/drawPart/beginDynamicComposite/endDynamicComposite | beginFrame/endFrame/beginMask/applyMask/beginMaskContent/endMask/drawPartPacket/begin/endDynamicComposite/playback | △（drawPart→drawPartPacket、playback追加） |
| `beginFrame(RenderBackend, ref RenderGpuState)` | state を初期化、バックエンド設定 | state=init、backend 設定、共有バッファ upload | ◯ |
| `endFrame(RenderBackend, ref RenderGpuState)` | 終了処理 | backend/state クリア | ◯ |
| `beginMask(bool useStencil)` | 即座にバックエンドへ/キューでは ApplyMask 成功時に初回発行 | QueueEmitter は pendingMask に保持し、applyMask 成功時に BeginMask をキュー | ◯ |
| `applyMask(Drawable, bool dodge)` | backend->applyMask | QueueEmitter: tryMakeMaskApplyPacket 成功時に BeginMask→ApplyMask を記録 | ◯ |
| `beginMaskContent` | backend->beginMaskContent | QueueEmitter: pendingMask の場合は発行せず破棄（D と同様） | ◯ |
| `endMask` | backend->endMask | QueueEmitter: pendingMask 破棄 or EndMask 発行 | △ |
| `drawPart` | backend->drawPart | C++: PartDrawPacket を構築済みで drawPartPacket | ◯ |
| `beginDynamicComposite`/`endDynamicComposite` | backend に pass 渡し | 同等（surface 必須チェックあり） | ◯ |
| `RenderQueue` (実バックエンド) | 共有バッファ dirty チェック後に upload | 同等 | ◯ |
| `QueueCommandEmitter` (キュー収集) | コマンドをキューし後で再生 | 同等、playback あり | ◯ |
| `recorded/queue` | キュー参照取得 | backendQueue() を返却 | ◯ |
| 未実装 | RenderBackend Enum 切替（OpenGL/DX12等） | QueueBackend 固定 | △ |

**現状**: 実バックエンド/キュー収集とも主要動作は実装済み。差分は D インターフェース名（drawPart→drawPartPacket）、Mask 開始の遅延発行（仕様差なし）、バックエンド種の固定化など。***
