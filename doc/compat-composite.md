# Composite 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| ctor `Composite(parent)` | autoResizedMesh=true で初期化 | 同等 | ◯ |
| ctor `Composite(data,uuid,parent)` | 同上 | data/uuid対応・autoResizedMesh=true | ◯ |
| method `typeId` | "Composite" | 同等 | ◯ |
| method `transform` | Partのtransformを返す | 同等 | ◯ |
| method `mustPropagate` | propagateMeshGroup を返す | 同等 | ◯ |
| method `propagateMeshGroupEnabled` | propagateMeshGroup getter | 同等 | ◯ |
| method `setPropagateMeshGroup` | propagateMeshGroup setter | 同等 | ◯ |
| method `threshold` | maskAlphaThreshold を返す | maskAlphaThreshold を返す | ◯ |
| method `setThreshold` | maskAlphaThreshold を設定 | 同等 | ◯ |
| method `serializeSelfImpl` | Node/Part状態＋blend/tint/screenTint/opacity/mask_threshold/propagate_meshgroup/masks；textures退避 | 同等フィールド出力だがmask modeを落とす | △ |
| method `deserializeFromFghj` | 上記フィールド読み込み、textures=null、autoResizedMesh=true | 近似（mask mode未復元） | △ |
| method `getChildrenBounds` | autoResizedMesh時は行列でbounds、非autoはmergeBounds、frameでmaxBoundsリセット | 近似（frame管理なし・puppet補正なし） | △ |
| method `enableMaxChildrenBounds` | frame管理しつつmaxChildrenBounds更新、target補正 | frame管理なしの近似 | △ |
| method `childOffscreenMatrix` | textureOffset考慮で子行列生成 | 近似 | △ |
| method `childCoreMatrix` | 子transform行列を返す | 同等 | ◯ |
| method `localBoundsFromMatrix` | 行列適用で子bounds算出 | 近似 | △ |
| field `propagateMeshGroup` | 有 | 同等 | ◯ |
| field `autoResizedMesh` | true初期化 | 同等 | ◯ |
| field `offscreen` | OffscreenState | 同等 | ◯ |
| field `maxChildrenBounds` | boundsキャッシュ | 同等 | ◯ |
| field `useMaxChildrenBounds` | キャッシュ使用フラグ | 同等 | ◯ |
| field `maxBoundsStartFrame` | キャッシュ開始フレーム | フレーム管理なし | △ |
