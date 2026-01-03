# core/common utils 実装互換性チェック (D ↔ C++)

| クラス/メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| `Vec2Array` 形式 | `veca!(float,2)` (SoA) | SoA 構造体（x/y 別配列） | ◯ |
| `Vec2Array::push_back/resize/size` | 配列操作 | 同等 | ◯ |
| `Vec2Array::operator[]/at` | 要素取得 | 同等 | ◯ |
| `InterpolateMode` | Nearest/Linear/Cubic/Step | 同等 | ◯ |
| `gatherVec2` | indices で抽出 | 同等 | ◯ |
| `scatterAddVec2` | indices で加算散布 | 同等 | ◯ |
