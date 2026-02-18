# render/graph_builder.hpp 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| method `beginFrame()` | dynamic frame 前進＋スタック初期化 | projectableFrame前進＋スタック初期化 | ◯ |
| method `clear()` | パススタックとトークン初期化 | 同等 | ◯ |
| method `empty()` | root以外無し＆items空を判定 | 同等 | ◯ |
| method `enqueueItem(zSort, scopeHint, builder)` | scope解決し zSort/sequence ソートで積む | scopeHint.skip対応・zSort/sequence整列で同等 | ◯ |
| method `pushDynamicComposite()` | DynamicCompositeパスをpushしtoken発行 | 同等（Projectableのスコープフラグも更新） | ◯ |
| method `popDynamicComposite()` | token検証し finalize | 同等（多重popも順次finalize） | ◯ |
| method `finalizeDynamicCompositePass()` | begin/end DynamicCompositeで子itemsをラップ | 同等 | ◯ |
| method `playback()` | スタック整合性検証後にitems再生 | 同等 | ◯ |

