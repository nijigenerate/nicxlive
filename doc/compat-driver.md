# Driver 実装互換性チェック (D ↔ C++)

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| ctor `Driver()`/`(uuid,parent)` | Node を初期化 | Node 初期化＋uuid/parent指定 ctor 追加 | ◯ |
| method `typeId` | "Driver" | 実装 | ◯ |
| method `runBeginTask` | super.runBeginTask | Node::runBeginTask 呼び出し | ◯ |
| method `getAffectedParameters` | 影響する Parameter を返す | 空ベクター返却で同等 | ◯ |
| method `affectsParameter` | UUID 比較で判定 | UUID 比較実装 | ◯ |
| method `updateDriver` | abstract | pure virtual | ◯ |
| method `reset` | abstract | pure virtual | ◯ |
| method `drawDebug` | デバッグ描画フック | 空実装 | ◯ |
| serialize/deserialize | Driver 固有フィールドなし | Node委譲で同等 | ◯ |
