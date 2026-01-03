# Projectable 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| ctor `Projectable(bool delegatedMode)` | delegatedMode 受け取りのみ | 同等 | ◯ |
| method `dynamicScopeTokenValue()` | dynamicScopeToken を返す | 同等 | ◯ |
| method `selfSort()` | subParts を zSort 降順ソート | 同等 | ◯ |
| method `setIgnorePuppetRecurse()` | Part/Mask 子孫に ignorePuppet を伝播 | 同等 | ◯ |
| method `setIgnorePuppet()` | 子に伝播し puppet.rescanNodes を呼ぶ | 同等 | ◯ |
| method `scanPartsRecurse()` | Part/Mask を収集、Projectable 子は自身で scanParts | Projectable 子で self 再帰のみで他子を取りこぼす可能性 | △ |
| method `drawSelf()` | 子なしなら return、ありなら Part::drawSelf | 同等 | ◯ |
| method `renderMask()` | MaskApply を backend に送る | backend未接続で実質無効 | ✗ |
| method `serializeSelfImpl()` | textures 一時退避＋ auto_resized を出力 | auto_resizedのみ、その他Projectable状態未保存 | △ |
| method `deserializeFromFghj()` | auto_resized 読み込み、textures null で復元 | auto_resized読み込みのみでProjectable固有状態未復元 | △ |
| method `initTarget()` | bounds で tex/stencil生成＋既存破棄 | Texture/Stencil生成のみ、旧リソース破棄・オフセット調整簡略 | △ |
| method `updateDynamicRenderStateFlags()` | deferred/autoResize/boundsDirty 管理 | 簡略化（autoResize一部のみ、frame判定/ancestor差分未対応） | △ |
| method `mergeBounds()` | 配列から min/max を算出 | 実装済み | ◯ |
| method `fullTransform()` | 親/puppet考慮のTRS | puppet transform適用不足（lockToRootのみ） | △ |
| method `fullTransformMatrix()` | fullTransform の行列 | 同上 | △ |
| method `boundsFromMatrix()` | 行列適用で Part bounds 算出 | 近似実装（scale/rotation簡略） | △ |
| method `getChildrenBounds()` | subParts bounds 合成＋キャッシュ | puppet補正やautoResized特有挙動なし | △ |
| method `deformationTranslationOffset()` | 一様変形なら平行移動返却 | 実装済み | ◯ |
| method `enableMaxChildrenBounds()` | max bounds を更新し保持 | 近似（target補正のみ、Frame管理なし） | △ |
| method `invalidateChildrenBounds()` | max bounds キャッシュ無効化 | 同等 | ◯ |
| method `createSimpleMesh()` | 子 bounds から自動リサイズメッシュ | クワッド生成のみ、元メッシュ/UV維持なし | △ |
| method `updateAutoResizedMeshOnce()` | 1フレーム自動サイズ更新 | createSimpleMesh のみ | △ |
| method `updatePartMeshOnce()` | 1回だけメッシュ更新 | createSimpleMesh のみ | △ |
| method `autoResizeMeshOnce()` | 1回だけ自動リサイズ | createSimpleMesh のみ | △ |
| method `dynamicRenderBegin()` | offscreen surface 準備 | 状態フラグのみ | △ |
| method `dynamicRenderEnd()` | offスクリーン終了 | 状態フラグのみ | △ |
| method `enqueueRenderCommands()` | RenderGraphにマスク/パートを積む | backend無し・直接drawSelf/renderMask呼び出しのみ | △ |
| method `runRenderTask()` | offscreen render 実行・頂点更新 | 簡易呼び出しのみ（renderGraph/texture描画なし） | △ |
| method `runBeginTask()` | フラグ初期化・親差分チェック | 再スキャンのみで差分チェック無し | △ |
| method `runPostTaskImpl()` | dynamicScope/rescope 処理 | フラグ初期化のみ | △ |
| method `notifyChange()` | 祖先変化を記録し遅延処理 | フラグのみ、recordAncestorChange無し | △ |
| field `initialized` | 初期化済みフラグ | 同等 | ◯ |
| field `forceResize` | 強制リサイズ | 同等 | ◯ |
| field `dynamicScopeActive` | 動的スコープ状態 | 同等 | ◯ |
| field `dynamicScopeToken` | トークン値 | 同等 | ◯ |
| field `reuseCachedTextureThisFrame` | 再利用フラグ | 同等 | ◯ |
| field `hasValidOffscreenContent` | オフスクリーン有効 | 同等 | ◯ |
| field `loggedFirstRenderAttempt` | 初回試行ログ | 同等 | ◯ |
| field `textureInvalidated` | テクスチャ無効化フラグ | 同等 | ◯ |
| field `shouldUpdateVertices` | 頂点更新要否 | 同等 | ◯ |
| field `boundsDirty` | bounds再計算要 | 同等 | ◯ |
| field `subParts` | Partコレクション | 同等 | ◯ |
| field `maskParts` | Maskコレクション | 同等 | ◯ |
| field `autoResizedMesh` | 自動リサイズ有効 | 追加済み | ◯ |
| field `textureOffset` | テクスチャオフセット | 追加済み | ◯ |
| field `texWidth/texHeight` | テクスチャ寸法 | 追加済み | ◯ |
| field `autoResizedSize` | 自動リサイズ寸法 | 追加済み | ◯ |
| field `deferred` | 遅延フラグ | 追加済み | ◯ |
| field `prevTranslation/Rotation/Scale` | 前回TRS記録 | 追加済み | ◯ |
| field `deferredChanged` | 遅延変化フラグ | 追加済み | ◯ |
| field `ancestorChangeQueued` | 祖先変化キュー | 追加済み | ◯ |
| field `maxChildrenBounds` | 最大子bounds | 追加済み | ◯ |
| field `useMaxChildrenBounds` | 最大bounds使用 | 追加済み | ◯ |
| field `maxBoundsStartFrame` | 最大bounds開始フレーム | 追加済み | ◯ |
| field `lastInitAttemptFrame` | 最終初期化試行フレーム | 追加済み | ◯ |
