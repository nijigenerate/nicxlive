# Deformable 実装互換性チェック (D ↔ C++)

| メソッド/フィールド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| フィールド `vertices` | Vec2Array(SoA) | Vec2Array(SoA) | ◯ |
| フィールド `deformation` | Vec2Array(SoA) | Vec2Array(SoA) | ◯ |
| フィールド `deformStack` | DeformationStack (push/preUpdate/update) | push/preUpdate/update 実装 | ◯ |
| コンストラクタ | 親/uuid指定＋タスク登録 | 親/uuid指定＋タスク登録 | ◯ |
| `updateVertices` | 抽象 | 抽象 | ◯ |
| `refresh` | updateVertices 呼び出し | 同等 | ◯ |
| `refreshDeform` | updateDeform 呼び出し | 同等 | ◯ |
| `rebuffer` | vertices 再設定＋deformation 長リセット | 同等 | ◯ |
| `pushDeformation` | deformStack.push(Deformation) | Deformation push | ◯ |
| `mustPropagate` | true | true | ◯ |
| `updateDeform` | deformation 長を vertices に合わせゼロ初期化 | 同等 | ◯ |
| `remapDeformationBindings` | Puppet 内 deform binding をリマップ | remapOffsets 追加で同等処理 | ◯ |
| `runBeginTask` | deformStack.preUpdate＋overrideTransform クリア＋super | 同等 | ◯ |
| `runPreProcessTask` | super 後に deformStack.update | 同等 | ◯ |
| `runPostTaskImpl` | super 後に updateDeform | 同等 | ◯ |
| `onDeformPushed` | フック（デフォルト空） | フック（空） | ◯ |
| `notifyDeformPushed` | onDeformPushed 呼び出し | 同等 | ◯ |
