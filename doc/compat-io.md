# compat-io (D `nijilive.fmt.io` ↔ C++ `fmt/io.hpp`)

| フィールド/メソッド | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `readValue` | BEで値読み取り | 同等（BE集計） | ◯ |
| `peekValue` | 読んで戻す | 同等 | ◯ |
| `readStr`/`peekStr` | 指定長の文字列読み取り/peek | 同等 | ◯ |
| `read`/`peek` | 指定長のバイト読み取り/peek | 同等 | ◯ |
| `skip` | 指定バイト数シーク | 同等 | ◯ |

備考: C++版は `std::ifstream` を前提。***
