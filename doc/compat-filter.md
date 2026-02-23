# NodeFilter 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| `captureTarget` | インターフェースのみ（派生実装） | 空実装 | ◯ |
| `releaseTarget` | インターフェースのみ（派生実装） | 空実装 | ◯ |
| `dispose` | mixin 内で children を releaseChild してクリア | NodeFilterMixin で同等処理 | ◯ |
| `applyDeformToChildren` | パラメータバインディングを辿り deform を転送 | deform binding を軸点ごとに補間・子へ伝搬 | ◯ |

