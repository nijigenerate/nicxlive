# Composite 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

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
| method `serializeSelfImpl` | Node/Part状態＋blend/tint/screenTint/opacity/mask_threshold/propagate_meshgroup/masks；textures退避 | 同等（mask mode保持・tint/screenTint文字列互換保存） | ◯ |
| method `deserializeFromFghj` | 上記フィールド読み込み、textures=null、autoResizedMesh=true | 同等（mask mode復元・texturesクリア） | ◯ |
| method `getChildrenBounds` | autoResizedMesh時は行列でbounds、非autoはmergeBounds、frameでmaxBoundsリセット | 同等（frame管理／autoResizedを反映） | ◯ |
| method `enableMaxChildrenBounds` | frame管理しつつmaxChildrenBounds更新、target補正 | 同等（frame管理＋autoResized時のローカルbounds補正） | ◯ |
| method `childOffscreenMatrix` | textureOffset考慮で子行列生成 | 同等（transform逆行列ベース） | ◯ |
| method `childCoreMatrix` | 子transform行列を返す | 同等 | ◯ |
| method `localBoundsFromMatrix` | 行列適用で子bounds算出 | 同等 | ◯ |
| field `propagateMeshGroup` | 有 | 同等 | ◯ |
| field `autoResizedMesh` | true初期化 | 同等 | ◯ |
| field `offscreen` | OffscreenState | 同等 | ◯ |
| field `maxChildrenBounds` | boundsキャッシュ | 同等 | ◯ |
| field `useMaxChildrenBounds` | キャッシュ使用フラグ | 同等 | ◯ |
| field `maxBoundsStartFrame` | キャッシュ開始フレーム | 同等 | ◯ |

