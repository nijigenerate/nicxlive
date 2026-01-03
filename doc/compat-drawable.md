# Drawable 実装互換性チェック (D ↔ C++)

| クラス/メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| フィールド `mesh.vertices` | メッシュ頂点 | 同等 | ◯ |
| フィールド `mesh.uvs` | メッシュUV | 同等 | ◯ |
| フィールド `mesh.indices` | メッシュインデックス | 同等 | ◯ |
| フィールド `mesh.origin` | 原点 | 同等 | ◯ |
| フィールド `mesh.gridAxes` | グリッド軸 | 同等 | ◯ |
| フィールド `deformationOffsets` | 頂点ごとのオフセット | 同等 | ◯ |
| フィールド `vertexOffset` | バッファオフセット | 同等 | ◯ |
| フィールド `uvOffset` | バッファオフセット | 同等 | ◯ |
| フィールド `deformOffset` | バッファオフセット | 同等 | ◯ |
| フィールド `tint` | 色 | 同等 | ◯ |
| フィールド `screenTint` | 色 | 同等 | ◯ |
| フィールド `emissionStrength` | 発光 | 同等 | ◯ |
| フィールド `textures` | テクスチャ配列 | 同等 | ◯ |
| フィールド `bounds` | 境界 | 同等 | ◯ |
| フィールド `weldedTargets` | 溶接ターゲットID | 同等 | ◯ |
| フィールド `weldedLinks` | 溶接リンク詳細 | 簡易（weight/indicesのみ） | ✗ |
| フィールド `weldingApplied` | 溶接適用フラグ | setだけで連携なし | △ |
| `updateIndices` | IBOアップロード＋shared同期 | ローカル検証のみ（IBO/バックエンド呼び出し無し） | ✗ |
| `updateVertices` | shared buffer resize＋deformリセット＋bounds＋sharedDirty通知 | Vec2Array再構成＋shared書き込みのみ（dirty/バックエンド未通知） | ✗ |
| `updateDeform` | deformStack＋sharedDirty＋bounds更新 | Deformable更新のみ（sharedDirty/バックエンド未通知） | ✗ |
| `nodeAttachProcessor` | 三角形探索＋行列適用し offset/transform 返却 | 平行移動のみで行列・頂点選択簡略 | ✗ |
| `weldingProcessor` | weldリンク＋行列ベースの offset 伝達・重複防止 | 重みブレンドのみ・リンク管理/dirty通知なし | ✗ |
| `updateBounds` | mesh+deformから境界算出 | mesh+deform/offset含む | ◯ |
| `drawMeshLines` | デバッグ描画 | デバッグラインを記録（バックエンド描画は未接続） | △ |
| `drawMeshPoints` | デバッグ描画 | デバッグクロスを記録（バックエンド描画は未接続） | △ |
| `getMesh` | backend取得 | 未実装 | ✗ |
| `rebufferMesh` | shared buffer再構築 | updateVertices+indicesのみ（dirty/IBO/renderer未連携） | ✗ |
| `reset` | IBO/バッファ解放 | フィールドクリア＋共有バッファオフセット初期化 | △ |
| `addWeldedTarget` | 重複管理しリンク登録＋相互設定 | 双方向だがリンク内容/dirtyなし | △ |
| `removeWeldedTarget` | リンク削除＋フラグ管理 | 双方向削除だがdirtyなし | △ |
| `isWeldedBy` | 溶接確認 | weldedTargets/linksのみ参照 | △ |
| `setupSelf` | super＋buildDrawable＋weld hook登録 | super＋updateVerticesのみ／hook簡略 | ✗ |
| `finalizeDrawable` | backend finalize | indices/vertices更新のみ（renderer未呼出） | ✗ |
| `normalizeUv` | UV正規化 | 単純平均で正規化（パディング・原点調整なし） | △ |
| `clearCache` | バッファ/キャッシュクリア | deformationOffsets/boundsのみ | ✗ |
| `centralizeDrawable` | 原点移動 | 実装（境界計算/子調整簡略） | △ |
| `copyFromDrawable` | 全データコピー | 頂点/UV/idx/tint等＋溶接情報をコピー（バッファ再登録） | △ |
| `setupChildDrawable` | 子セットアップ（フィルタ登録等） | 未実装 | ✗ |
| `releaseChildDrawable` | 子解放（フィルタ解除等） | 未実装 | ✗ |
| `buildDrawable` | force判定＋再計算＋shared/buffer | updateVertices+boundsのみ | ✗ |
| `mustPropagateDrawable` | true | true | ◯ |
| `fillDrawPacket` | modelMatrix 等設定 | transformのみ（テクスチャ/色/IBO未設定） | ✗ |
