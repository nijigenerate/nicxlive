# Texture 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| 項目 | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| ctor() | 空のテクスチャ | 同等（CPUバッファなし） | ◯ |
| ctor(file) | 画像読み込み・UUID割当・GPUへアップロード | stb_imageでロードしQueueBackend createTexture呼び出し（インターフェース同等） | ◯ |
| ctor(ShallowTexture) | ShallowTextureから生成 | QueueBackend createTexture呼び出し | ◯ |
| ctor(width,height,channels,stencil,useMipmaps) | 空テクスチャ生成・GPUハンドル作成 | QueueBackend createTexture呼び出し | ◯ |
| ctor(data,width,height,inChannels,outChannels,stencil,useMipmaps) | データからGPUテクスチャ生成・フィルタ/ラップ設定 | QueueBackend createTexture呼び出し | ◯ |
| setData | GPUへデータ転送 | QueueBackend updateTexture呼び出し | ◯ |
| setFiltering(bool) | ポイント/リニア切替 | QueueBackend setTextureParams呼び出し | ◯ |
| setFiltering(enum) | Filtering指定で設定 | QueueBackend setTextureParams呼び出し | ◯ |
| setWrapping | Wrapping設定 | QueueBackend setTextureParams呼び出し | ◯ |
| setAnisotropy | 異方性設定 | QueueBackend setTextureParams呼び出し | ◯ |
| lock | データロック | フラグ更新のみ | ✗ |
| unlock | ロック解除・書き戻し | フラグ更新のみ | ✗ |
| dispose | GPUリソース解放 | QueueBackend disposeTexture＋CPUクリア | ◯ |
| width | 幅のゲッター | 同等 | ◯ |
| height | 高さのゲッター | 同等 | ◯ |
| channels | チャネル数ゲッター | 同等 | ◯ |
| stencil | ステンシル判定 | 同等 | ◯ |
| getRuntimeUUID | UUID取得 | ローカルカウンタ（D も同等管理） | ◯ |
| setRuntimeUUID | UUID設定 | 同等 | ◯ |

