# Unity Backend 互換性評価 (論理フロー比較)

比較対象:
- OpenGL基準: `nijiv/source/opengl/opengl_backend.d`
- DirectX基準: `nijiv/source/directx/directx_backend.d`
- Unity実装: `nicxlive/unity_backend/unity_backend.cs`

判定基準:
- `OK`: `recall >= 0.90` かつ `precision >= 0.90` かつ 重要な余剰ステップなし
- `内容NG`: 同名だが `recall/precision` のどちらかが不足
- `NG`: 同名なし or `recall/precision` が大きく不足

canonicalフローは OpenGL/DX の共通部分(LCS)で構築。`gl*`, `SDL*`, `D3D*` などAPI固有呼び出しは正規化して比較。

### クラス比較 (OpenGL)
| Source Class | Source Line | Unity Match | 判定 | 根拠 |
|---|---:|---|---|---|
| PostProcessingShader | 32 | 907 | OK | 同名型あり |
| ShaderProgramHandle | 68 | 884 | OK | 同名型あり |
| PartDrawPacket | 142 | 890 | OK | 同名型あり |
| MaskDrawPacket | 169 | 891 | OK | 同名型あり |
| MaskApplyPacket | 182 | 892 | OK | 同名型あり |
| DynamicCompositeSurface | 189 | 893 | OK | 同名型あり |
| DynamicCompositePass | 198 | 894 | OK | 同名型あり |
| ShaderStageSource | 302 | 888 | OK | 同名型あり |
| ShaderAsset | 307 | 889 | OK | 同名型あり |
| Shader | 329 | 980 | OK | 同名型あり |
| Texture | 403 | 909 | OK | 同名型あり |
| GLShaderHandle | 564 | 886 | OK | 同名型あり |
| GLTextureHandle | 568 | 887 | OK | 同名型あり |
| RenderingBackend | 583 | 1047 | OK | 同名型あり |
| OpenGLBackendInit | 2454 | 895 | OK | 同名型あり |

### クラス比較 (DirectX)
| Source Class | Source Line | Unity Match | 判定 | 根拠 |
|---|---:|---|---|---|
| Vec2f | 31 | 898 | OK | 同名型あり |
| Vec3f | 36 | 899 | OK | 同名型あり |
| Vec4f | 42 | 900 | OK | 同名型あり |
| DirectXRuntimeOptions | 49 | 906 | OK | 同名型あり |
| Texture | 310 | 909 | OK | 同名型あり |
| RenderingBackend | 406 | 1047 | OK | 同名型あり |
| Vertex | 417 | 897 | OK | 同名型あり |
| DrawSpan | 437 | 901 | OK | 同名型あり |
| CompositeState | 461 | 902 | OK | 同名型あり |
| DirectXRuntime | 476 | 1675 | OK | 同名型あり |
| VSInput | 1329 | 903 | OK | 同名型あり |
| VSOutput | 1333 | 904 | OK | 同名型あり |
| DirectXBackendInit | 3059 | 896 | OK | 同名型あり |

### インターフェース比較 (OpenGL)
| Source Interface | Source Line | Unity Match | 判定 | 根拠 |
|---|---:|---|---|---|
| (none) | - | - | - | source側に定義なし |

### インターフェース比較 (DirectX)
| Source Interface | Source Line | Unity Match | 判定 | 根拠 |
|---|---:|---|---|---|
| (none) | - | - | - | source側に定義なし |

### メソッド比較（論理フロー）
| Method | GL Line | DX Line | Unity Line | 判定 | Recall | Precision | Canonical Flow | Unity Flow | 根拠 |
|---|---:|---:|---:|---|---:|---:|---|---|---|
| acquireDynamicFramebuffer | 2509 | - | 1313 | OK | 1.00 | 1.00 | acquiredynamicframebuffer | acquiredynamicframebuffer | recall=1.00, precision=1.00, pflow=acquiredynamicframebuffer |
| alignUp | - | 1076 | 1625 | OK | 1.00 | 1.00 | alignup | alignup | recall=1.00, precision=1.00, pflow=alignup |
| applyBlendMode | 1927 | - | 2590 | OK | 1.00 | 1.00 | blend.apply -> blend.legacy_set -> blend.adv_set | blend.apply -> blend.legacy_set -> blend.adv_set | recall=1.00, precision=1.00, pflow=blend.apply -> blend.legacy_set -> blend.adv_set |
| applyBlendingCapabilities | 1827 | - | 1418 | OK | 1.00 | 1.00 | blend.cap_apply -> blend.adv_supported -> blend.coherent_supported -> blend.coherent_set | blend.cap_apply -> blend.adv_supported -> blend.coherent_supported -> blend.coherent_set | recall=1.00, precision=1.00, pflow=blend.cap_apply -> blend.adv_supported -> blend.coherent_supported -> blend.coherent_set |
| applyCompositeTransform | - | 2696 | 2886 | OK | 1.00 | 1.00 | applycompositetransform | applycompositetransform | recall=1.00, precision=1.00, pflow=applycompositetransform |
| applyMask | 1448 | 2987 | 1163 | OK | 1.00 | 1.00 | mask.apply -> draw.part -> draw.mask_exec | mask.apply -> ensureruntimeready -> ensuremaskbackendinitialized -> draw.part -> draw.mask_exec | recall=1.00, precision=1.00, pflow=mask.apply -> draw.part -> draw.mask_exec |
| applyTextureAnisotropy | 1648 | - | 713 | OK | 1.00 | 1.00 | tex.aniso | tex.aniso | recall=1.00, precision=1.00, pflow=tex.aniso |
| applyTextureFiltering | 1619 | - | 677 | OK | 1.00 | 1.00 | tex.filter | tex.filter | recall=1.00, precision=1.00, pflow=tex.filter |
| applyTextureWrapping | 1631 | - | 697 | OK | 1.00 | 1.00 | tex.wrap | tex.wrap | recall=1.00, precision=1.00, pflow=tex.wrap |
| backendHandle | 538 | - | 959 | OK | 1.00 | 1.00 | backendhandle | backendhandle | recall=1.00, precision=1.00, pflow=backendhandle |
| beginDynamicComposite | 1342 | 2924 | 1129 | OK | 1.00 | 1.00 | comp.begin | comp.begin -> ensureruntimeready | recall=1.00, precision=1.00, pflow=comp.begin |
| beginFrame | - | 2378 | 1786 | OK | 1.00 | 1.00 | frame.begin -> device.init -> frame.shutdown | frame.begin -> device.init -> frame.shutdown | recall=1.00, precision=1.00, pflow=frame.begin -> device.init -> frame.shutdown |
| beginMask | 1439 | 2979 | 1155 | OK | 1.00 | 1.00 | mask.begin | mask.begin -> ensureruntimeready | recall=1.00, precision=1.00, pflow=mask.begin |
| beginMaskContent | 1486 | 3002 | 1184 | OK | 1.00 | 1.00 | mask.content_begin | mask.content_begin | recall=1.00, precision=1.00, pflow=mask.content_begin |
| beginScene | 997 | 2639 | 1072 | OK | 1.00 | 1.00 | scene.begin | scene.begin -> ensurepipelineobjects -> viewport.set -> ensureruntimeready | recall=1.00, precision=1.00, pflow=scene.begin |
| bind | 504 | - | 954 | OK | 1.00 | 1.00 | tex.bind -> backend.current | tex.bind -> backend.current | recall=1.00, precision=1.00, pflow=tex.bind -> backend.current |
| bindDrawableVao | 900 | - | 2076 | OK | 1.00 | 1.00 | binddrawablevao | binddrawablevao | recall=1.00, precision=1.00, pflow=binddrawablevao |
| bindPartShader | 724 | - | 2071 | OK | 1.00 | 1.00 | shader.use | shader.use | recall=1.00, precision=1.00, pflow=shader.use |
| bindTextureHandle | 1591 | - | 632 | OK | 1.00 | 1.00 | tex.bind | tex.bind | recall=1.00, precision=1.00, pflow=tex.bind |
| blendModeBarrier | 1932 | - | 2637 | OK | 1.00 | 1.00 | blend.barrier -> isadvancedblendmode -> blend.barrier_issue | blend.barrier -> isadvancedblendmode -> blend.barrier_issue | recall=1.00, precision=1.00, pflow=blend.barrier -> isadvancedblendmode -> blend.barrier_issue |
| buildBlendDesc | - | 619 | 1667 | OK | 1.00 | 1.00 | blend.desc | blend.desc | recall=1.00, precision=1.00, pflow=blend.desc |
| channelFormat | 112 | - | 925 | OK | 1.00 | 1.00 | channelformat | channelformat | recall=1.00, precision=1.00, pflow=channelformat |
| checkProgram | 89 | - | 1569 | OK | 1.00 | 1.00 | diag.check | diag.check | recall=1.00, precision=1.00, pflow=diag.check |
| checkShader | 74 | - | 1560 | OK | 1.00 | 1.00 | diag.check | diag.check | recall=1.00, precision=1.00, pflow=diag.check |
| configureMacOpenGLSurfaceOpacity | 250 | - | 1577 | OK | 1.00 | 1.00 | platform.init | platform.init | recall=1.00, precision=1.00, pflow=platform.init |
| consumeDeviceResetFlag | - | 1283 | 1688 | OK | 1.00 | 1.00 | consumedeviceresetflag | consumedeviceresetflag | recall=1.00, precision=1.00, pflow=consumedeviceresetflag |
| createDepthStencilTarget | - | 923 | 1695 | OK | 1.00 | 1.00 | rt.setup | rt.setup | recall=1.00, precision=1.00, pflow=rt.setup |
| createDrawableBuffers | 1691 | - | 2294 | OK | 1.00 | 1.00 | draw.buffers_create | draw.buffers_create | recall=1.00, precision=1.00, pflow=draw.buffers_create |
| createDynamicCompositePass | 2301 | - | 1147 | OK | 1.00 | 1.00 | comp.create_pass | comp.create_pass | recall=1.00, precision=1.00, pflow=comp.create_pass |
| createPipelineState | - | 1325 | 1702 | OK | 1.00 | 1.00 | pipeline.setup | pipeline.setup | recall=1.00, precision=1.00, pflow=pipeline.setup |
| createRenderTargets | - | 761 | 1704 | OK | 1.00 | 1.00 | rt.setup -> rt.release | rt.setup -> rt.release | recall=1.00, precision=1.00, pflow=rt.setup -> rt.release |
| createShader | 1506 | - | 760 | OK | 1.00 | 1.00 | shader.create | shader.create | recall=1.00, precision=1.00, pflow=shader.create |
| createSrvResources | - | 1040 | 1709 | OK | 1.00 | 1.00 | srv.setup | srv.setup | recall=1.00, precision=1.00, pflow=srv.setup |
| createSwapChainAndTargets | - | 805 | 1715 | OK | 1.00 | 1.00 | rt.setup | rt.setup | recall=1.00, precision=1.00, pflow=rt.setup |
| createTextureHandle | 1573 | - | 622 | OK | 1.00 | 1.00 | tex.create | tex.create | recall=1.00, precision=1.00, pflow=tex.create |
| currentRenderBackend | 296 | - | 1531 | OK | 1.00 | 1.00 | backend.current -> backend.require | backend.current -> backend.require | recall=1.00, precision=1.00, pflow=backend.current -> backend.require |
| currentRtvHandle | - | 574 | 1387 | OK | 1.00 | 1.00 | currentrtvhandle | currentrtvhandle | recall=1.00, precision=1.00, pflow=currentrtvhandle |
| defaultCompositeState | - | 2624 | 1657 | OK | 1.00 | 1.00 | state.default | state.default | recall=1.00, precision=1.00, pflow=state.default |
| descriptorHeapCpuStart | - | 538 | 1392 | OK | 1.00 | 1.00 | srv.setup | srv.setup | recall=1.00, precision=1.00, pflow=srv.setup |
| descriptorHeapGpuStart | - | 556 | 1393 | OK | 1.00 | 1.00 | srv.setup | srv.setup | recall=1.00, precision=1.00, pflow=srv.setup |
| destroyShader | 1528 | - | 772 | OK | 1.00 | 1.00 | shader.destroy | shader.destroy | recall=1.00, precision=1.00, pflow=shader.destroy |
| destroyTextureHandle | 1582 | - | 627 | OK | 1.00 | 1.00 | tex.destroy | tex.destroy | recall=1.00, precision=1.00, pflow=tex.destroy |
| dispose | 531 | 388 | 970 | OK | 1.00 | 1.00 | frame.dispose | frame.dispose -> backend.try | recall=1.00, precision=1.00, pflow=frame.dispose |
| drawDrawableElements | 1947 | - | 1379 | OK | 1.00 | 1.00 | draw.elements | draw.elements | recall=1.00, precision=1.00, pflow=draw.elements |
| drawMaskPacket | - | 2823 | 1395 | OK | 1.00 | 1.00 | draw.mask_exec | draw.mask_exec | recall=1.00, precision=1.00, pflow=draw.mask_exec |
| drawPartPacket | 1228 | 2703 | 1194 | OK | 1.00 | 1.00 | draw.part | draw.part -> ensureruntimeready -> binddrawablevao -> draw.stage -> draw.stage_setup | recall=1.00, precision=1.00, pflow=draw.part |
| drawUploadedGeometry | - | 1832 | 1396 | OK | 1.00 | 1.00 | draw.uploaded | draw.uploaded | recall=1.00, precision=1.00, pflow=draw.uploaded |
| dsvHandle | - | 593 | 1390 | OK | 1.00 | 1.00 | dsvhandle | dsvhandle | recall=1.00, precision=1.00, pflow=dsvhandle |
| dxTrace | - | 130 | 1611 | OK | 1.00 | 1.00 | diag.trace | diag.trace | recall=1.00, precision=1.00, pflow=diag.trace |
| dynamicFramebufferKeyUsesHandle | 2500 | - | 1312 | OK | 1.00 | 1.00 | dynamicframebufferkeyuseshandle | dynamicframebufferkeyuseshandle | recall=1.00, precision=1.00, pflow=dynamicframebufferkeyuseshandle |
| endDynamicComposite | 1413 | 2962 | 1138 | OK | 1.00 | 1.00 | comp.end | comp.end -> ensureruntimeready | recall=1.00, precision=1.00, pflow=comp.end |
| endFrame | - | 2453 | 1814 | OK | 1.00 | 1.00 | frame.end | frame.end -> render.exec | recall=1.00, precision=1.00, pflow=frame.end |
| endMask | 1491 | 3005 | 1189 | OK | 1.00 | 1.00 | mask.end | mask.end | recall=1.00, precision=1.00, pflow=mask.end |
| endScene | 1049 | 3013 | 1086 | OK | 1.00 | 1.00 | scene.end | scene.end -> ensureruntimeready | recall=1.00, precision=1.00, pflow=scene.end |
| enforceHr | - | 136 | 1620 | OK | 1.00 | 1.00 | enforcehr | enforcehr | recall=1.00, precision=1.00, pflow=enforcehr |
| enqueueSpan | - | 2656 | 1394 | OK | 1.00 | 1.00 | enqueuespan | enqueuespan | recall=1.00, precision=1.00, pflow=enqueuespan |
| ensureDebugRendererInitialized | 1756 | - | 1386 | OK | 1.00 | 1.00 | ensuredebugrendererinitialized | ensuredebugrendererinitialized | recall=1.00, precision=1.00, pflow=ensuredebugrendererinitialized |
| ensureDrawableBackendInitialized | 1941 | - | 2090 | OK | 1.00 | 1.00 | ensuredrawablebackendinitialized | ensuredrawablebackendinitialized | recall=1.00, precision=1.00, pflow=ensuredrawablebackendinitialized |
| ensureMaskBackendInitialized | 1722 | - | 2081 | OK | 1.00 | 1.00 | ensuremaskbackendinitialized | ensuremaskbackendinitialized | recall=1.00, precision=1.00, pflow=ensuremaskbackendinitialized |
| ensureOffscreenDepthStencilTarget | - | 975 | 1720 | OK | 1.00 | 1.00 | rt.setup | rt.setup | recall=1.00, precision=1.00, pflow=rt.setup |
| ensurePresentProgram | 1062 | - | 2285 | OK | 1.00 | 1.00 | pipeline.setup | pipeline.setup | recall=1.00, precision=1.00, pflow=pipeline.setup |
| ensureRenderBackend | 275 | - | 1515 | OK | 1.00 | 1.00 | backend.ensure | backend.ensure | recall=1.00, precision=1.00, pflow=backend.ensure |
| ensureSharedIndexBuffer | 1731 | - | 2310 | OK | 1.00 | 1.00 | ensuresharedindexbuffer | ensuresharedindexbuffer | recall=1.00, precision=1.00, pflow=ensuresharedindexbuffer |
| ensureTextureUploaded | - | 1107 | 1722 | OK | 1.00 | 1.00 | upload.ensure | upload.ensure | recall=1.00, precision=1.00, pflow=upload.ensure |
| ensureUploadBuffer | - | 1742 | 1723 | OK | 1.00 | 1.00 | upload.ensure | upload.ensure | recall=1.00, precision=1.00, pflow=upload.ensure |
| executeMaskPacket | 1956 | - | 1231 | OK | 1.00 | 1.00 | draw.mask_exec -> ensuremaskbackendinitialized -> binddrawablevao -> shader.use -> shader.uniform_set -> draw.elements | draw.mask_exec -> ensureruntimeready -> ensuremaskbackendinitialized -> binddrawablevao -> shader.use -> shader.uniform_set -> draw.elements | recall=1.00, precision=1.00, pflow=draw.mask_exec -> ensuremaskbackendinitialized -> binddrawablevao -> shader.use -> shader.uniform_set -> draw.elements |
| framebufferHandle | 1497 | - | 2275 | OK | 1.00 | 1.00 | framebufferhandle | framebufferhandle | recall=1.00, precision=1.00, pflow=framebufferhandle |
| fromOpenGLSource | 310 | - | 991 | OK | 1.00 | 1.00 | shader.asset | shader.asset | recall=1.00, precision=1.00, pflow=shader.asset |
| genMipmap | 498 | - | 949 | OK | 1.00 | 1.00 | tex.mipmap -> backend.current | tex.mipmap -> backend.current | recall=1.00, precision=1.00, pflow=tex.mipmap -> backend.current |
| generateTextureMipmap | 1613 | - | 659 | OK | 1.00 | 1.00 | tex.mipmap | tex.mipmap | recall=1.00, precision=1.00, pflow=tex.mipmap |
| getOrCreateIbo | 905 | - | 1358 | OK | 1.00 | 1.00 | draw.ibo_get -> draw.buffers_create -> draw.indices_upload | draw.ibo_get -> draw.buffers_create -> draw.indices_upload | recall=1.00, precision=1.00, pflow=draw.ibo_get -> draw.buffers_create -> draw.indices_upload |
| getOrCreateIboByHandle | 933 | - | 1368 | OK | 1.00 | 1.00 | draw.ibo_get -> draw.buffers_create -> draw.indices_upload | draw.ibo_get -> draw.buffers_create -> draw.indices_upload | recall=1.00, precision=1.00, pflow=draw.ibo_get -> draw.buffers_create -> draw.indices_upload |
| getShaderUniformLocation | 1540 | - | 794 | OK | 1.00 | 1.00 | shader.uniform_loc | shader.uniform_loc | recall=1.00, precision=1.00, pflow=shader.uniform_loc |
| getTextureData | 517 | - | 960 | OK | 1.00 | 1.00 | tex.read -> backend.current | tex.read -> backend.current | recall=1.00, precision=1.00, pflow=tex.read -> backend.current |
| getTextureId | 510 | - | 965 | OK | 1.00 | 1.00 | tex.id -> backend.try -> tex.native | tex.id -> backend.try -> tex.native | recall=1.00, precision=1.00, pflow=tex.id -> backend.try -> tex.native |
| getUniform | 51 | - | 1002 | OK | 1.00 | 1.00 | getuniform | getuniform | recall=1.00, precision=1.00, pflow=getuniform |
| getUniformLocation | 355 | - | 997 | OK | 1.00 | 1.00 | shader.uniform_loc -> backend.current | shader.uniform_loc -> backend.current | recall=1.00, precision=1.00, pflow=shader.uniform_loc -> backend.current |
| getViewport | 1681 | - | 2243 | OK | 1.00 | 1.00 | viewport.get | viewport.get | recall=1.00, precision=1.00, pflow=viewport.get |
| hasUniform | 61 | - | 1001 | OK | 1.00 | 1.00 | hasuniform | hasuniform | recall=1.00, precision=1.00, pflow=hasuniform |
| height | 467 | - | 923 | OK | 1.00 | 1.00 | height | height | recall=1.00, precision=1.00, pflow=height |
| inSetRenderBackend | 281 | - | 1516 | OK | 1.00 | 1.00 | insetrenderbackend | insetrenderbackend | recall=1.00, precision=1.00, pflow=insetrenderbackend |
| initDirectXBackend | - | 3101 | 1627 | OK | 1.00 | 1.00 | backend.init | backend.init | recall=1.00, precision=1.00, pflow=backend.init |
| initOpenGLBackend | 2550 | - | 1588 | OK | 1.00 | 1.00 | backend.init -> backend.current | backend.init -> backend.current | recall=1.00, precision=1.00, pflow=backend.init -> backend.current |
| initialize | - | 2227 | 1724 | OK | 1.00 | 1.00 | device.init | device.init | recall=1.00, precision=1.00, pflow=device.init |
| initializeMaskBackend | 838 | - | 2066 | OK | 1.00 | 1.00 | ensuremaskbackendinitialized | ensuremaskbackendinitialized | recall=1.00, precision=1.00, pflow=ensuremaskbackendinitialized |
| initializePartBackendResources | 790 | - | 2057 | OK | 1.00 | 1.00 | render.init_part | render.init_part | recall=1.00, precision=1.00, pflow=render.init_part |
| initializeRenderer | 749 | 2569 | 1066 | OK | 1.00 | 1.00 | render.init | render.init | recall=1.00, precision=1.00, pflow=render.init |
| invalidateGpuObjects | - | 393 | 1725 | OK | 1.00 | 1.00 | invalidategpuobjects | invalidategpuobjects | recall=1.00, precision=1.00, pflow=invalidategpuobjects |
| isDxTraceEnabled | - | 126 | 1610 | OK | 1.00 | 1.00 | isdxtraceenabled | isdxtraceenabled | recall=1.00, precision=1.00, pflow=isdxtraceenabled |
| issueBlendBarrier | 1937 | - | 1298 | OK | 1.00 | 1.00 | blend.barrier_issue | blend.barrier_issue | recall=1.00, precision=1.00, pflow=blend.barrier_issue |
| makeDynamicFramebufferKey | 2490 | - | 1328 | OK | 1.00 | 1.00 | makedynamicframebufferkey | makedynamicframebufferkey | recall=1.00, precision=1.00, pflow=makedynamicframebufferkey |
| maxAnisotropy | 404 | - | 924 | OK | 1.00 | 1.00 | maxanisotropy | maxanisotropy | recall=1.00, precision=1.00, pflow=maxanisotropy |
| maxTextureAnisotropy | 1654 | - | 1307 | OK | 1.00 | 1.00 | maxtextureanisotropy | maxtextureanisotropy | recall=1.00, precision=1.00, pflow=maxtextureanisotropy |
| mulMat4 | - | 2595 | 1659 | OK | 1.00 | 1.00 | mulmat4 | mulmat4 | recall=1.00, precision=1.00, pflow=mulmat4 |
| mulMat4Vec4 | - | 2609 | 1660 | OK | 1.00 | 1.00 | mulmat4vec4 | mulmat4vec4 | recall=1.00, precision=1.00, pflow=mulmat4vec4 |
| offscreenDsvHandle | - | 599 | 1391 | OK | 1.00 | 1.00 | offscreendsvhandle | offscreendsvhandle | recall=1.00, precision=1.00, pflow=offscreendsvhandle |
| offscreenRtvHandle | - | 581 | 1388 | OK | 1.00 | 1.00 | offscreenrtvhandle | offscreenrtvhandle | recall=1.00, precision=1.00, pflow=offscreenrtvhandle |
| offscreenRtvHandleAt | - | 587 | 1389 | OK | 1.00 | 1.00 | offscreenrtvhandleat | offscreenrtvhandleat | recall=1.00, precision=1.00, pflow=offscreenrtvhandleat |
| popViewport | 1674 | - | 2235 | OK | 1.00 | 1.00 | viewport.pop | viewport.pop | recall=1.00, precision=1.00, pflow=viewport.pop |
| postProcessScene | 1165 | 3012 | 1099 | OK | 1.00 | 1.00 | scene.post | scene.post -> ensureruntimeready | recall=1.00, precision=1.00, pflow=scene.post |
| presentSceneToBackbuffer | 1139 | - | 1107 | OK | 1.00 | 1.00 | scene.present -> pipeline.setup -> scene.rebind -> viewport.set | scene.present -> ensureruntimeready -> pipeline.setup -> scene.rebind -> viewport.set | recall=1.00, precision=1.00, pflow=scene.present -> pipeline.setup -> scene.rebind -> viewport.set |
| pushViewport | 1668 | - | 2230 | OK | 1.00 | 1.00 | viewport.push | viewport.push | recall=1.00, precision=1.00, pflow=viewport.push |
| queryWindowPixelSize | - | 3076 | 1649 | OK | 1.00 | 1.00 | window.query | window.query | recall=1.00, precision=1.00, pflow=window.query |
| rangeInBounds | - | 2618 | 1661 | OK | 1.00 | 1.00 | rangeinbounds | rangeinbounds | recall=1.00, precision=1.00, pflow=rangeinbounds |
| readTextureData | 1660 | - | 722 | OK | 1.00 | 1.00 | tex.read | tex.read | recall=1.00, precision=1.00, pflow=tex.read |
| rebindActiveTargets | 957 | - | 1122 | OK | 1.00 | 1.00 | scene.rebind | scene.rebind -> ensureruntimeready | recall=1.00, precision=1.00, pflow=scene.rebind |
| recoverFromDeviceLoss | - | 1251 | 1726 | OK | 1.00 | 1.00 | device.recover | device.recover | recall=1.00, precision=1.00, pflow=device.recover |
| releaseDxResource | - | 606 | 1727 | OK | 1.00 | 1.00 | device.release | device.release | recall=1.00, precision=1.00, pflow=device.release |
| releaseDynamicFramebuffersForTextureHandle | 2524 | - | 1320 | OK | 1.00 | 1.00 | releasedynamicframebuffersfortexturehandle | releasedynamicframebuffersfortexturehandle | recall=1.00, precision=1.00, pflow=releasedynamicframebuffersfortexturehandle |
| releaseRenderTargets | - | 755 | 1732 | OK | 1.00 | 1.00 | rt.release | rt.release | recall=1.00, precision=1.00, pflow=rt.release |
| renderCommands | 2646 | 3161 | 1430 | OK | 1.00 | 1.00 | render.exec | render.exec | recall=1.00, precision=1.00, pflow=render.exec |
| renderScene | 1772 | - | 1408 | OK | 1.00 | 1.00 | render.exec | render.exec | recall=1.00, precision=1.00, pflow=render.exec |
| renderStage | 2176 | - | 2485 | OK | 1.00 | 1.00 | draw.stage -> draw.elements -> blend.barrier | draw.stage -> draw.elements -> blend.barrier | recall=1.00, precision=1.00, pflow=draw.stage -> draw.elements -> blend.barrier |
| requireRenderBackend | 290 | - | 1525 | OK | 1.00 | 1.00 | backend.require -> backend.try | backend.require -> backend.try | recall=1.00, precision=1.00, pflow=backend.require -> backend.try |
| requireWindowHandle | - | 2559 | 1656 | OK | 1.00 | 1.00 | window.handle | window.handle | recall=1.00, precision=1.00, pflow=window.handle |
| resizeSwapChain | - | 1289 | 1736 | OK | 1.00 | 1.00 | rt.setup -> rt.release | rt.setup -> rt.release | recall=1.00, precision=1.00, pflow=rt.setup -> rt.release |
| resizeViewportTargets | 842 | - | 2262 | OK | 1.00 | 1.00 | viewport.resize -> viewport.set | viewport.resize -> viewport.set | recall=1.00, precision=1.00, pflow=viewport.resize -> viewport.set |
| sampleTex | - | 1364 | 1663 | OK | 1.00 | 1.00 | shader.sample | shader.sample | recall=1.00, precision=1.00, pflow=shader.sample |
| sanitizeBlendMode | - | 612 | 1666 | OK | 1.00 | 1.00 | sanitizeblendmode | sanitizeblendmode | recall=1.00, precision=1.00, pflow=sanitizeblendmode |
| screenBlend | - | 1361 | 1662 | OK | 1.00 | 1.00 | screenblend | screenblend | recall=1.00, precision=1.00, pflow=screenblend |
| sdlError | 2543 | 3070 | 1532 | OK | 1.00 | 1.00 | platform.error | platform.error | recall=1.00, precision=1.00, pflow=platform.error |
| setAdvancedBlendCoherent | 1837 | - | 2662 | OK | 1.00 | 1.00 | blend.coherent_set | blend.coherent_set | recall=1.00, precision=1.00, pflow=blend.coherent_set |
| setAdvancedBlendEquation | 1903 | - | 1280 | OK | 1.00 | 1.00 | blend.adv_set -> blend.legacy_set | blend.adv_set -> blend.legacy_set | recall=1.00, precision=1.00, pflow=blend.adv_set -> blend.legacy_set |
| setAnisotropy | 476 | - | 934 | OK | 1.00 | 1.00 | tex.aniso -> backend.current | tex.aniso -> backend.current | recall=1.00, precision=1.00, pflow=tex.aniso -> backend.current |
| setData | 486 | 340 | 939 | OK | 1.00 | 1.00 | tex.upload | tex.upload -> backend.current -> tex.mipmap | recall=1.00, precision=1.00, pflow=tex.upload |
| setDirectXRuntimeOptions | - | 63 | 1668 | OK | 1.00 | 1.00 | setdirectxruntimeoptions | setdirectxruntimeoptions | recall=1.00, precision=1.00, pflow=setdirectxruntimeoptions |
| setFiltering | 471 | - | 926 | OK | 1.00 | 1.00 | tex.filter -> backend.current | tex.filter -> backend.current | recall=1.00, precision=1.00, pflow=tex.filter -> backend.current |
| setLegacyBlendMode | 1842 | - | 1276 | OK | 1.00 | 1.00 | blend.legacy_set | blend.legacy_set | recall=1.00, precision=1.00, pflow=blend.legacy_set |
| setShaderUniform | 1545 | - | 808 | OK | 1.00 | 1.00 | shader.uniform_set | shader.uniform_set | recall=1.00, precision=1.00, pflow=shader.uniform_set |
| setSharedSnapshot | - | 2590 | 1425 | OK | 1.00 | 1.00 | setsharedsnapshot | setsharedsnapshot | recall=1.00, precision=1.00, pflow=setsharedsnapshot |
| setUniform | 360 | - | 1003 | OK | 1.00 | 1.00 | shader.uniform_set -> backend.current | shader.uniform_set -> backend.current | recall=1.00, precision=1.00, pflow=shader.uniform_set -> backend.current |
| setViewport | 731 | 2581 | 2252 | OK | 1.00 | 1.00 | viewport.set | viewport.set -> viewport.resize | recall=1.00, precision=1.00, pflow=viewport.set |
| setWrapping | 481 | - | 930 | OK | 1.00 | 1.00 | tex.wrap -> backend.current | tex.wrap -> backend.current | recall=1.00, precision=1.00, pflow=tex.wrap -> backend.current |
| setupShaderStage | 2045 | - | 1251 | OK | 1.00 | 1.00 | draw.stage_setup -> shader.use -> shader.uniform_set -> blend.apply | draw.stage_setup -> shader.use -> shader.uniform_set -> blend.apply | recall=1.00, precision=1.00, pflow=draw.stage_setup -> shader.use -> shader.uniform_set -> blend.apply |
| shaderAsset | 317 | - | 1584 | OK | 1.00 | 1.00 | shader.asset | shader.asset | recall=1.00, precision=1.00, pflow=shader.asset |
| sharedDeformBufferHandle | 990 | - | 1310 | OK | 1.00 | 1.00 | draw.elements | draw.elements | recall=1.00, precision=1.00, pflow=draw.elements |
| sharedUvBufferHandle | 983 | - | 1309 | OK | 1.00 | 1.00 | draw.elements | draw.elements | recall=1.00, precision=1.00, pflow=draw.elements |
| sharedVertexBufferHandle | 976 | - | 1308 | OK | 1.00 | 1.00 | draw.elements | draw.elements | recall=1.00, precision=1.00, pflow=draw.elements |
| shutdown | - | 2487 | 1807 | OK | 1.00 | 1.00 | frame.shutdown | frame.shutdown | recall=1.00, precision=1.00, pflow=frame.shutdown |
| shutdownDirectXBackend | - | 3205 | 1642 | OK | 1.00 | 1.00 | frame.dispose | frame.dispose | recall=1.00, precision=1.00, pflow=frame.dispose |
| supportsAdvancedBlend | 1819 | - | 1303 | OK | 1.00 | 1.00 | blend.adv_supported | blend.adv_supported | recall=1.00, precision=1.00, pflow=blend.adv_supported |
| supportsAdvancedBlendCoherent | 1823 | - | 1305 | OK | 1.00 | 1.00 | blend.coherent_supported | blend.coherent_supported | recall=1.00, precision=1.00, pflow=blend.coherent_supported |
| textureId | 1765 | - | 1311 | OK | 1.00 | 1.00 | tex.id | tex.id | recall=1.00, precision=1.00, pflow=tex.id |
| textureNativeHandle | 2296 | - | 751 | OK | 1.00 | 1.00 | tex.native | tex.native | recall=1.00, precision=1.00, pflow=tex.native |
| transitionResource | - | 1080 | 1741 | OK | 1.00 | 1.00 | transitionresource | transitionresource | recall=1.00, precision=1.00, pflow=transitionresource |
| transitionTextureState | - | 1091 | 1753 | OK | 1.00 | 1.00 | transitiontexturestate | transitiontexturestate | recall=1.00, precision=1.00, pflow=transitiontexturestate |
| tryRenderBackend | 285 | - | 1517 | OK | 1.00 | 1.00 | backend.try -> backend.ensure | backend.try -> backend.ensure | recall=1.00, precision=1.00, pflow=backend.try -> backend.ensure |
| unPremultiplyRgba | 409 | - | 1533 | OK | 1.00 | 1.00 | unpremultiplyrgba | unpremultiplyrgba | recall=1.00, precision=1.00, pflow=unpremultiplyrgba |
| updateHeapSrvTexture2D | - | 1060 | 1754 | OK | 1.00 | 1.00 | srv.setup | srv.setup | recall=1.00, precision=1.00, pflow=srv.setup |
| uploadData | - | 1786 | 1763 | OK | 1.00 | 1.00 | upload.data -> upload.ensure | upload.data -> upload.ensure | recall=1.00, precision=1.00, pflow=upload.data -> upload.ensure |
| uploadDrawableIndices | 1698 | - | 2300 | OK | 1.00 | 1.00 | draw.indices_upload | draw.indices_upload | recall=1.00, precision=1.00, pflow=draw.indices_upload |
| uploadGeometry | - | 1806 | 1769 | OK | 1.00 | 1.00 | upload.geometry -> upload.data | upload.geometry -> upload.data | recall=1.00, precision=1.00, pflow=upload.geometry -> upload.data |
| uploadTextureData | 1597 | - | 642 | OK | 1.00 | 1.00 | tex.upload | tex.upload | recall=1.00, precision=1.00, pflow=tex.upload |
| use | 350 | - | 992 | OK | 1.00 | 1.00 | shader.use -> backend.current | shader.use -> backend.current | recall=1.00, precision=1.00, pflow=shader.use -> backend.current |
| useShader | 1501 | - | 785 | OK | 1.00 | 1.00 | shader.use | shader.use | recall=1.00, precision=1.00, pflow=shader.use |
| vsMain | - | 1337 | 1665 | OK | 1.00 | 1.00 | shader.vs | shader.vs | recall=1.00, precision=1.00, pflow=shader.vs |
| waitForGpu | - | 1228 | 1781 | OK | 1.00 | 1.00 | gpu.wait | gpu.wait | recall=1.00, precision=1.00, pflow=gpu.wait |
| width | 463 | - | 921 | OK | 1.00 | 1.00 | width | width | recall=1.00, precision=1.00, pflow=width |

## サマリー
- OK: 158
- 内容NG: 0
- NG: 0