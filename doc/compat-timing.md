# timing 実装互換性チェック (D `nijilive/package.d` ↔ C++ `core/timing.*`)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 関数 | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `inInit(timeFunc)` | initRenderer() を呼び、timeFunc をセット | initRenderer() 呼び出し＋timeFunc セット | ◯ |
| `inSetTimingFunc` | 時間関数をセット（null不可） | 時間関数をセット（未指定時はsteady_clock） | ◯ |
| `inUpdate` | currentTime/lastTime/deltaTime を更新 | 同等に current/last/delta を更新 | ◯ |
| `deltaTime` | 直近のdtを返す | 同等 | ◯ |
| `lastTime` | 前フレームの時刻を返す | 同等 | ◯ |
| `currentTime` | 現在時刻を返す | 同等 | ◯ |

**現状**: 時間計測APIは D 実装基準で主要差分を解消済み。***

