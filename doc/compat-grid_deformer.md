# GridDeformer 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| フィールド vertexBuffer | グリッド頂点 SoA | Deformable.vertices で保持（名前のみ差） | ◯ |
| フィールド axisX | グリッド軸 X | 実装・正規化あり | ◯ |
| フィールド axisY | グリッド軸 Y | 実装・正規化あり | ◯ |
| フィールド formation | GridFormation(Bilinear) | GridFormation(Bilinear) | ◯ |
| フィールド inverseMatrix | 逆行列キャッシュ | 実装 | ◯ |
| フィールド dynamic | 動的切替 | 実装 | ◯ |
| フィールド translateChildren | 子への平行移動転送 | 実装 | ◯ |
| 定数 DefaultAxis | [-0.5,0.5] | kDefaultAxis 同値 | ◯ |
| 定数 BoundaryTolerance | 1e-4 | kBoundaryTolerance 同値 | ◯ |
| コンストラクタ | 軸初期化＋preProcess要求 | 同等 | ◯ |
| メソッド gridFormation getter | formation 返却 | 同等 | ◯ |
| メソッド gridFormation setter | formation 設定 | 同等 | ◯ |
| メソッド switchDynamic | dynamic 切替＋子再setup | 同等 | ◯ |
| メソッド vertices() | vertexBuffer 参照返し | Deformable.vertices を返却 | ◯ |
| メソッド rebuffer(Vec2Array) | 頂点から軸推定し採用/初期化 | 同等 | ◯ |
| メソッド typeId | "GridDeformer" | 同等 | ◯ |
| メソッド runPreProcessTask | transform 更新→逆行列→updateDeform | transform更新→有限確認→逆行列→updateDeform | ◯ |
| メソッド build | 子 setup → self → super | 同等 | ◯ |
| メソッド clearCache | no-op | no-op | ◯ |
| メソッド setupChild | 有効グリッド時に再帰 setup | 同等 | ◯ |
| メソッド releaseChild | 再帰解除 | 同等 | ◯ |
| メソッド captureTarget | children_ref 追加＋setup | 同等 | ◯ |
| メソッド releaseTarget | children_ref 解除＋release | 同等 | ◯ |
| メソッド runRenderTask | GPU コマンドなし | 同等 | ◯ |
| メソッド deformChildren | 逆行列→サンプル→グリッド補間→オフセット変換＋guard | フロー同等（有限チェック・長さ検証・無効サンプル中断） | ◯ |
| メソッド applyDeformToChildren | dynamic 無効化＋binding 反映＋再帰伝搬 | 同等 | ◯ |
| メソッド matrixIsFinite | 行列有限判定 | 同等 | ◯ |
| メソッド computeCache | clamp→区間探索→u/v 計算 | 同等 | ◯ |
| メソッド locateInterval | 軸から区間/重み取得 | 同等 | ◯ |
| メソッド sampleGridPoints | バイリニア補間（SIMD/スカラ両方） | バイリニア補間（4レーンバッチ＋スカラ） | ◯ |
| メソッド setupChildNoRecurse | dynamic/translateChildren に応じ pre/post hook 設定。Composite が propagateMeshGroup のときは hook を外す | 同等（Composite propagateMeshGroup では hook を外す） | ◯ |
| メソッド releaseChildNoRecurse | hook 解除 | 同等 | ◯ |
| メソッド adoptGridFromAxes | 軸採用 | 同等 | ◯ |
| メソッド adoptFromVertices | 頂点から軸採用（形状保持可） | 同等 | ◯ |
| メソッド deriveAxes | 頂点から軸推定 | 同等 | ◯ |
| メソッド fillDeformationFromPositions | 位置との差分で deformation 埋め | 同等 | ◯ |
| メソッド setGridAxes | 軸正規化＋バッファ再構築 | 同等 | ◯ |
| メソッド gridIndex | x,y から一次元 index | 同等 | ◯ |
| メソッド serializeSelfImpl | 軸/formation/dynamic を保存 | 実装済 | ◯ |
| メソッド deserializeFromFghj | 軸/formation/dynamic 復元＋軸設定 | 実装済 | ◯ |

