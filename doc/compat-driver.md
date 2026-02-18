# Driver 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| ctor Driver() | Node を初期化 | 同等 | ◯ |
| ctor Driver(uuid, parent) | Node を初期化 | uuid/parent 指定 ctor 追加 | ◯ |
| method `typeId` | "Driver" | 実装 | ◯ |
| method `runBeginTask` | super.runBeginTask | Node::runBeginTask 呼び出し | ◯ |
| method `getAffectedParameters` | 影響する Parameter を返す | 空ベクター返却で同等 | ◯ |
| method `affectsParameter` | UUID 比較で判定 | UUID 比較実装 | ◯ |
| method `updateDriver` | abstract | pure virtual | ◯ |
| method `reset` | abstract | pure virtual | ◯ |
| method `drawDebug` | デバッグ描画フック | 空実装 | ◯ |
| serialize | Driver 固有フィールドなし | Node委譲で同等 | ◯ |
| deserialize | Driver 固有フィールドなし | Node委譲で同等 | ◯ |

