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
| フィールド `bounds` | 境界 | doGenerateBounds ガードあり | ◯ |
| フィールド `weldedTargets` | 溶接ターゲットID | 同等 | ◯ |
| フィールド `weldedLinks` | 溶接リンク詳細 | 相互登録・逆参照を構築 | ◯ |
| フィールド `weldingApplied` | 溶接適用フラグ | 双方向セットで重複防止 | ◯ |
| `updateIndices` | IBOアップロード＋shared同期 | initialize/bind/create後にIBO upload | ◯ |
| `updateVertices` | shared buffer resize＋deformリセット＋bounds＋sharedDirty通知 | sharedResize＋dirtyマーク＋updateDeform/bounds/共有バッファ更新 | ◯ |
| `updateDeform` | deformStack＋sharedDirty＋bounds更新 | deformStack＋sharedDirty＋bounds更新 | ◯ |
| `nodeAttachProcessor` | 三角形探索＋行列適用し offset/transform 返却 | findSurroundingTriangle＋行列計算で平行移動/回転適用 | ◯ |
| `weldingProcessor` | weldリンク＋行列ベースの offset 伝達・重複防止 | 行列ベースで自己/相手にオフセット散布 | ◯ |
| `updateBounds` | mesh+deformから境界算出 | doGenerateBoundsガード付き | ◯ |
| `drawMeshLines` | デバッグ描画 | backendのdebug線APIへアップロードして描画 | ◯ |
| `drawMeshPoints` | デバッグ描画 | backendのdebug点APIへアップロードして描画 | ◯ |
| `getMesh` | backend取得 | MeshData参照を返却 | ◯ |
| `rebufferMesh` | shared buffer再構築 | MeshData受け取りindices/vertices再構築 | ◯ |
| `reset` | IBO/バッファ解放 | 頂点のみリセット（変形維持） | ◯ |
| `addWeldedTarget` | 重複管理しリンク登録＋相互設定 | 相互リンク作成・逆参照埋め＋hook登録 | ◯ |
| `removeWeldedTarget` | リンク削除＋フラグ管理 | 双方向削除＋hook解除 | ◯ |
| `isWeldedBy` | 溶接確認 | weldedLinks検索 | ◯ |
| `setupSelf` | super＋build＋weld hook登録 | super＋weld hook登録 | ◯ |
| `finalizeDrawable` | backend finalize | buildDrawable(true)でIBO/共有再upload | ◯ |
| `normalizeUv` | UV正規化 | 中心合わせを含む正規化＋dirty | ◯ |
| `clearCache` | バッファ/キャッシュクリア | attachedIndex含めリセット | ◯ |
| `centralizeDrawable` | 原点移動 | 子に伝播し境界再計算のみ | ◯ |
| `copyFromDrawable` | 全データコピー | 頂点/UV/idx/gridAxes等＋フラグをコピー | ◯ |
| `setupChildDrawable` | 子セットアップ（フィルタ登録等） | pinToMesh子にpreProcess hook追加 | ◯ |
| `releaseChildDrawable` | 子解放（フィルタ解除等） | stage0 hook削除＋attachedIndex削除 | ◯ |
| `buildDrawable` | force判定＋再計算＋shared/buffer | force時のみIBO/共有を更新しdraw呼び出し | ◯ |
| `mustPropagateDrawable` | true | true | ◯ |
| `fillDrawPacket` | modelMatrix 等設定 | 行列/オフセット＋色/ブレンド/テクスチャ等を設定 | ◯ |
