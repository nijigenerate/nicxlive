# Projectable 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 | 差分メモ(概算行) |
| --- | --- | --- | --- | --- |
| ctor `Projectable(bool delegatedMode)` | delegatedMode 受け取りのみ | 同等 | ◯ | - |
| method `dynamicScopeTokenValue()` | dynamicScopeToken を返す | 同等 | ◯ | - |
| method `selfSort()` | subParts を zSort 降順ソート | 同等 | ◯ | - |
| method `setIgnorePuppetRecurse()` | Part/Mask 子孫に ignorePuppet を伝播 | 同等 | ◯ | - |
| method `setIgnorePuppet()` | 子に伝播し puppet.rescanNodes を呼ぶ | 同等 | ◯ | - |
| method `scanPartsRecurse()` | Part/Mask を収集、Projectable 子は自身で scanParts | Projectable 子にscanPartsRecurseを委譲し取りこぼし防止 | ◯ | - |
| method `drawSelf()` | 子なしなら return、ありなら Part::drawSelf | 同等 | ◯ | - |
| method `renderMask()` | MaskApply を backend に送る | Part経由でbackend apply（同等） | ◯ | - |
| method `serializeSelfImpl()` | textures 一時退避＋ auto_resized を出力 | auto_resizedのみ保存 | ◯ | - |
| method `deserializeFromFghj()` | auto_resized 読み込み、textures null で復元 | 同等 | ◯ | - |
| method `initTarget()` | bounds で tex/stencil生成＋既存破棄 | deformオフセットを反映し旧リソース破棄・destroyDynamicComposite 呼び出し | ◯ | - |
| method `updateDynamicRenderStateFlags()` | deferred/autoResize/boundsDirty 管理 | ancestor差分反映/prevTRS更新を実装 | ◯ | - |
| method `mergeBounds()` | 配列から min/max を算出 | 実装済み | ◯ | - |
| method `fullTransform()` | 親/puppet考慮のTRS | 同等 | ◯ | - |
| method `fullTransformMatrix()` | fullTransform の行列 | 同等 | ◯ | - |
| method `boundsFromMatrix()` | 行列適用で Part bounds 算出 | 同等 | ◯ | - |
| method `getChildrenBounds()` | subParts bounds 合成＋キャッシュ | maxBoundsStartFrame管理・bounds補正を実装 | ◯ | - |
| method `deformationTranslationOffset()` | 一様変形なら平行移動返却 | 実装済み | ◯ | - |
| method `enableMaxChildrenBounds()` | max bounds を更新し保持 | maxBoundsStartFrame更新済み | ◯ | - |
| method `invalidateChildrenBounds()` | max bounds キャッシュ無効化 | 同等 | ◯ | - |
| method `createSimpleMesh()` | 子 bounds から自動リサイズメッシュ | D同等のサイズ調整・offset更新 | ◯ | - |
| method `updateAutoResizedMeshOnce()` | 1フレーム自動サイズ更新 | 同等 | ◯ | - |
| method `updatePartMeshOnce()` | 1回だけメッシュ更新 | 同等 | ◯ | - |
| method `autoResizeMeshOnce()` | 1回だけ自動リサイズ | 同等 | ◯ | - |
| method `dynamicRenderBegin()` | offscreen surface 準備 | DynamicCompositePass生成＋beginDynamicComposite呼び出し | ◯ | - |
| method `dynamicRenderEnd()` | offスクリーン終了 | endDynamicComposite 呼び出し | ◯ | - |
| method `enqueueRenderCommands()` | RenderGraphにマスク/パートを積む | pushDynamicCompositeでオフスクリーン子をキュー化 | ◯ | - |
| method `runRenderTask()` | offscreen render 実行・頂点更新 | push/popDynamicComposite経由でマスク適用＋後処理 | ◯ | - |
| method `runBeginTask()` | フラグ初期化・親差分チェック | pendingAncestor初期化・差分検知を追加 | ◯ | - |
| method `runPostTaskImpl()` | dynamicScope/rescope 処理 | priority別処理を実装 | ◯ | - |
| method `notifyChange()` | 祖先変化を記録し遅延処理 | ancestor変化キュー/autoResize反映を追加 | ◯ | - |
| field `initialized` | 初期化済みフラグ | 同等 | ◯ | - |
| field `forceResize` | 強制リサイズ | 同等 | ◯ | - |
| field `dynamicScopeActive` | 動的スコープ状態 | 同等 | ◯ | - |
| field `dynamicScopeToken` | トークン値 | 同等 | ◯ | - |
| field `reuseCachedTextureThisFrame` | 再利用フラグ | 同等 | ◯ | - |
| field `hasValidOffscreenContent` | オフスクリーン有効 | 同等 | ◯ | - |
| field `loggedFirstRenderAttempt` | 初回試行ログ | 同等 | ◯ | - |
| field `textureInvalidated` | テクスチャ無効化フラグ | 同等 | ◯ | - |
| field `shouldUpdateVertices` | 頂点更新要否 | 同等 | ◯ | - |
| field `boundsDirty` | bounds再計算要 | 同等 | ◯ | - |
| field `subParts` | Partコレクション | 同等 | ◯ | - |
| field `maskParts` | Maskコレクション | 同等 | ◯ | - |
| field `autoResizedMesh` | 自動リサイズ有効 | 追加済み | ◯ | - |
| field `textureOffset` | テクスチャオフセット | 追加済み | ◯ | - |
| field `texWidth/texHeight` | テクスチャ寸法 | 追加済み | ◯ | - |
| field `autoResizedSize` | 自動リサイズ寸法 | 追加済み | ◯ | - |
| field `deferred` | 遅延フラグ | 追加済み | ◯ | - |
| field `prevTranslation/Rotation/Scale` | 前回TRS記録 | 追加済み | ◯ | - |
| field `deferredChanged` | 遅延変化フラグ | 追加済み | ◯ | - |
| field `ancestorChangeQueued` | 祖先変化キュー | 追加済み | ◯ | - |
| field `maxChildrenBounds` | 最大子bounds | 追加済み | ◯ | - |
| field `useMaxChildrenBounds` | 最大bounds使用 | 追加済み | ◯ | - |
| field `maxBoundsStartFrame` | 最大bounds開始フレーム | 追加済み | ◯ | - |
| field `lastInitAttemptFrame` | 最終初期化試行フレーム | 追加済み | ◯ | - |
| method `registerRenderTasks()` | delegated経路のrenderタスク登録 | D同等にoverride | ◯ | - |
| method `delegatedRunRenderBeginTask()` | delegated用 renderBegin 実装 | 同等 | ◯ | - |
| method `delegatedRunRenderTask()` | delegated用 render 実装 | 同等（空処理） | ◯ | - |
| method `delegatedRunRenderEndTask()` | delegated用 renderEnd 実装 | 同等 | ◯ | - |
| method `registerDelegatedTasks()` | delegated用に各タスクをscheduler登録 | 同等 | ◯ | - |
| method `setupChild()` | ignorePuppet伝播・bounds再計算 | D同等 | ◯ | - |
| method `releaseChild()` | ignorePuppet解除・bounds再計算 | D同等 | ◯ | - |
| method `setupSelf()` | listener登録・初期化処理 | D同等 | ◯ | - |
| method `releaseSelf()` | listener解除 | D同等 | ◯ | - |
| method `onAncestorChanged()` | ancestor変化検知でdeferred設定 | D同等 | ◯ | - |
