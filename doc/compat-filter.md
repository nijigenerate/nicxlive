# NodeFilter 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| `captureTarget` | インターフェースのみ（派生実装） | 空実装 | ◯ |
| `releaseTarget` | インターフェースのみ（派生実装） | 空実装 | ◯ |
| `dispose` | mixin 内で children を releaseChild してクリア | NodeFilterMixin で同等処理 | ◯ |
| `applyDeformToChildren` | `_applyDeformToChildren` で `filterChildren` を介し、`DeformationParameterBinding`/`ValueParameterBinding`/TRS binding を更新。最後に `param.removeBinding(binding)` | `applyDeformToChildrenInternal` に D 同等の転送処理を実装済み（`filterChildren` 呼び出し、TRS抽出、再帰転送、`removeBinding` 実施） | ◯ |

## 行単位差分（フィルタ関連）
1. `NodeFilter` インターフェース引数型
`nijilive/source/nijilive/core/nodes/filter.d:8-11`
`nicxlive/core/nodes/filter.hpp:15-18`
差分: D は `Node`/`Parameter[]`、C++ は `std::shared_ptr<Node>`/`std::vector<std::shared_ptr<Parameter>>`。

2. `_applyDeformToChildren` 相当実装
`nijilive/source/nijilive/core/nodes/filter.d:24-145`
`nicxlive/core/nodes/filter.cpp:15-190`
差分: なし（D の中核アルゴリズムを `applyDeformToChildrenInternal` として移植済み）。

3. `trsBindings` 抽出・`TransformBindingNames` 適用
`nijilive/source/nijilive/core/nodes/filter.d:26-48`
`nicxlive/core/nodes/filter.cpp:24-53`
差分: なし。

4. `filterChildren` 呼び出し経路
`nijilive/source/nijilive/core/nodes/filter.d:77-95`
`nicxlive/core/nodes/filter.cpp:110-114, 134-143`
差分: なし（`filterChildren` の戻り変形を各 binding に反映）。

5. `resetOffset` 再帰
`nijilive/source/nijilive/core/nodes/filter.d:105-112`
`nicxlive/core/nodes/filter.cpp:63-71, 155-157`
差分: なし。

6. `param.removeBinding(binding)`
`nijilive/source/nijilive/core/nodes/filter.d:141`
`nicxlive/core/nodes/filter.cpp:188`
差分: なし。
