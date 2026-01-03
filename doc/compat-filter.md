# NodeFilter 実装互換性チェック (D ↔ C++)

| メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| `captureTarget` | インターフェースのみ（派生実装） | 空実装 | ◯ |
| `releaseTarget` | インターフェースのみ（派生実装） | 空実装 | ◯ |
| `dispose` | mixin 内で children を releaseChild してクリア | NodeFilterMixin で同等処理 | ◯ |
| `applyDeformToChildren` | パラメータバインディングを辿り deform を転送 | deform binding を軸点ごとに補間・子へ伝搬 | ◯ |
