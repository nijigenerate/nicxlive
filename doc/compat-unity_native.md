# unity_native (D `integration/unity.d` ↔ C++ `core/unity_native.*`) 互換性チェック

| 項目 | D 実装 | C++ 現状 | 互換性 |
| --- | --- | --- | --- |
| `NjgRenderCommandKind` | 全コマンド種を定義 | 同等列挙を定義 | ◯ |
| `NjgQueuedCommand` | RenderCommandKind と各パケットを包含 | 同等構造体 | ◯ |
| `NjgPartDrawPacket` | mvp付き。indices を範囲チェック | mvp/offset/indices保持。indexCountをクランプ | ◯ |
| `NjgMaskDrawPacket` | model/mvp/origin/offset/indexをSerialize | 同等フィールド。indexCountをクランプ | ◯ |
| `NjgDynamicCompositePass` | textures/stencil/scale/rotationZ/origBuffer/viewport | 同等フィールドを設定 | ◯ |
| `UnityResourceCallbacks.createTexture` | Unity側でテクスチャ生成 | resourceQueue適用時/puppetロード時に呼び出し、ハンドル保持・統計加算（unity_native.cpp: applyTextureCommands/ensurePuppetTextures） | ◯ |
| `UnityResourceCallbacks.updateTexture` | Unity側で更新 | resourceQueue適用時/puppetロード時に呼び出し（unity_native.cpp: applyTextureCommands/ensurePuppetTextures） | ◯ |
| `UnityResourceCallbacks.releaseTexture` | Unity側で解放 | Dispose/puppetUnload/rendererDestroy で呼び出し統計更新（unity_native.cpp: releasePuppetTextures/releaseExternalTexture） | ◯ |
| `njgCreateRenderer` | backend初期化、RT確保、activeRenderers登録 | backendをcurrentに設定し initRendererCommon/initializeRenderer を呼んだ上で viewport 初期設定（RT確保はBeginFrameで実施） | ◯ |
| `njgDestroyRenderer` | activeRenderersから削除・解放 | マップerase | ◯ |
| `njgLoadPuppet` | inLoadPuppet + TextureHandle確保 + Renderer紐付け | inLoadPuppetでロードし、Unity側テクスチャ作成・更新＋backendに作成/paramコマンドを積む | ◯ |
| `njgLoadPuppetFromMemory` | バッファから inLoadPuppetFromMemory | 同等（Unityテクスチャ作成＋backendコマンド積み） | ◯ |
| `njgWritePuppetToMemory` | PuppetをINP形式でバッファ出力 | inWriteINPPuppetMemory→mallocコピーを返す（njgFreeBufferで解放） | ◯ |
| `njgGetParameters` | Puppet.parameters を列挙（uuid/isVec2/min/max/defaults/name） | 同等構造体をバッファに詰めて返す（unity_native.cpp: njgGetParameters） | ◯ |
| `njgUpdateParameters` | uuidでパラメータを引き当て値を更新 | 同等に uuid 対応で Vec2/float を更新（unity_native.cpp: njgUpdateParameters） | ◯ |
| `njgUnloadPuppet` | Renderer配列から除去 | 同等に除去 | ◯ |
| `njgBeginFrame` | コマンドバッファ初期化、RT確保、viewport設定 | viewport設定し、サイズ変化時にRTを再生成して setRenderTargets、キューをクリア（unity_native.cpp: njgBeginFrame）。RTハンドルアクセサは未公開だが現行設計では不要（コマンドキュー再生のみ） | ◯ |
| `njgTickPuppet` | 時間進行・puppet.update | inUpdate() → puppet->update を実行（unity_native.cpp: njgTickPuppet、timing.cpp: inUpdate） | ◯ |
| `njgEmitCommands` | puppet.draw→QueueEmitterからSerialize。空コマンド除外。SharedBuffer詰める | puppet.draw→backendキューをパック、SharedBuffer詰める。resourceQueueをUnityに反映 | ◯ |
| `njgFlushCommandBuffer` | バッファクリア＋GC要求 | backendキューとqueuedをクリア | △ |
| `njgGetGcHeapSize` | GC使用量返却 | malloc統計でヒープ使用量を返す（macOS依存の概算） | △ |
| `njgGetTextureStats` | 作成/解放/現在枚数返却 | create/dispose に加え puppetUnload/rendererDestroy で releaseTexture とカウント更新 | ◯ |

**現状**: コマンドパケットのシリアライズ経路は概ねD版と整合。Unityリソースコールバック、RT確保、puppet更新、GC/テクスチャ統計などDLL APIの多くが未移植。***
