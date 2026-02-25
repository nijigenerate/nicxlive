/*
 * nicxlive WebGL backend
 *
 * Write-through port structure from:
 *   nijiv/source/opengl/opengl_backend.d
 */

export const NjgRenderCommandKind = Object.freeze({
  DrawPart: 0,
  BeginDynamicComposite: 1,
  EndDynamicComposite: 2,
  BeginMask: 3,
  ApplyMask: 4,
  BeginMaskContent: 5,
  EndMask: 6,
});

export const MaskDrawableKind = Object.freeze({
  Part: 0,
  Mask: 1,
});

export const BlendMode = Object.freeze({
  Normal: 0,
  Multiply: 1,
  Screen: 2,
  Overlay: 3,
  Darken: 4,
  Lighten: 5,
  ColorDodge: 6,
  LinearDodge: 7,
  AddGlow: 8,
  ColorBurn: 9,
  HardLight: 10,
  SoftLight: 11,
  Difference: 12,
  Exclusion: 13,
  Subtract: 14,
  Inverse: 15,
  DestinationIn: 16,
  ClipToLower: 17,
  SliceFromLower: 18,
});

export const Filtering = Object.freeze({
  Linear: 0,
  Point: 1,
});

export const Wrapping = Object.freeze({
  Clamp: 0,
  Repeat: 1,
  Mirror: 2,
});

function asFloat32(v) {
  if (v instanceof Float32Array) return v;
  if (Array.isArray(v)) return new Float32Array(v);
  if (ArrayBuffer.isView(v)) return new Float32Array(v.buffer, v.byteOffset, (v.byteLength / 4) | 0);
  return new Float32Array(0);
}

function asU16(v) {
  if (v instanceof Uint16Array) return v;
  if (Array.isArray(v)) return new Uint16Array(v);
  if (ArrayBuffer.isView(v)) return new Uint16Array(v.buffer, v.byteOffset, (v.byteLength / 2) | 0);
  return new Uint16Array(0);
}

function compileShader(gl, type, source) {
  const shader = gl.createShader(type);
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    const log = gl.getShaderInfoLog(shader) || "";
    gl.deleteShader(shader);
    throw new Error(log || "shader compile failed");
  }
  return shader;
}

function createProgram(gl, vsSource, fsSource) {
  const vs = compileShader(gl, gl.VERTEX_SHADER, vsSource);
  const fs = compileShader(gl, gl.FRAGMENT_SHADER, fsSource);
  const prog = gl.createProgram();
  gl.attachShader(prog, vs);
  gl.attachShader(prog, fs);
  gl.linkProgram(prog);
  gl.deleteShader(vs);
  gl.deleteShader(fs);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
    const log = gl.getProgramInfoLog(prog) || "";
    gl.deleteProgram(prog);
    throw new Error(log || "program link failed");
  }
  return prog;
}

function makeDynamicFramebufferKey(textures, textureCount, stencil) {
  const t0 = Number(textures?.[0] || 0);
  const t1 = Number(textures?.[1] || 0);
  const t2 = Number(textures?.[2] || 0);
  return `${t0},${t1},${t2},${Number(textureCount || 0)},${Number(stencil || 0)}`;
}

class DynamicCompositeSurface {
  constructor() {
    this.textureHandles = [0, 0, 0];
    this.textureCount = 0;
    this.stencilHandle = 0;
    this.framebuffer = null;
  }
}

class DynamicCompositePass {
  constructor() {
    this.surface = null;
    this.scale = [1, 1];
    this.rotationZ = 0;
    this.origBuffer = null;
    this.origViewport = [0, 0, 0, 0];
    this.autoScaled = false;
    this.drawBufferCount = 1;
    this.hasStencil = false;
  }
}

export class WebGLRenderBackend {
  constructor(gl, opts = {}) {
    this.gl = gl;
    this.debugLog = !!opts.debugLog;
    this.extAnisotropy = gl.getExtension("EXT_texture_filter_anisotropic")
      || gl.getExtension("MOZ_EXT_texture_filter_anisotropic")
      || gl.getExtension("WEBKIT_EXT_texture_filter_anisotropic");
    this.extTextureBorderClamp = gl.getExtension("EXT_texture_border_clamp");
    this.extAdvancedBlend = gl.getExtension("KHR_blend_equation_advanced");
    this.extAdvancedBlendCoherent = gl.getExtension("KHR_blend_equation_advanced_coherent");

    this.texturesByHandle = new Map();
    this.indexBuffersByHandle = new Map();
    this.dynamicFramebufferCache = new Map();

    this.sharedVertexBuffer = null;
    this.sharedUvBuffer = null;
    this.sharedDeformBuffer = null;
    this.viewportWidthStack = [];
    this.viewportHeightStack = [];

    this.drawableVAO = null;
    this.activeDynamicPasses = [];
    this.activeRenderTargetHandleStack = [];
    this.activeRenderTargetHandles = new Set();

    this.pendingMask = false;
    this.pendingMaskUsesStencil = false;

    this.advancedBlending = false;
    this.advancedBlendingCoherent = false;
    this.clearColor = [0, 0, 0, 0];
    this.sceneAmbientLight = [0, 0, 0, 0];

    this.boundAlbedoHandle = 0;
    this.postProcessingStack = [];
    this.thumbnailGridEnabled = !!opts.thumbnailGrid;
    this.sceneVAO = null;
    this.sceneVBO = null;
    this.presentVAO = null;
    this.presentVBO = null;
    this.presentProgram = null;
    this.presentTexUniform = null;
    this.presentUseColorKeyUniform = null;
    this.useColorKeyTransparency = false;
    this.presentCopyTex = null;
    this.presentCopyWidth = 0;
    this.presentCopyHeight = 0;
    this.fBuffer = null;
    this.cfBuffer = null;
    this.fAlbedo = null;
    this.fEmissive = null;
    this.fBump = null;
    this.fStencil = null;
    this.cfAlbedo = null;
    this.cfEmissive = null;
    this.cfBump = null;
    this.cfStencil = null;
    this.sceneTargetWidth = 0;
    this.sceneTargetHeight = 0;
    this.postTmpFbo = null;
    this.postTmpTex = null;
    this.postTmpFbo2 = null;
    this.postTmpTex2 = null;
    this.postTmpWidth = 0;
    this.postTmpHeight = 0;
    this.debugPointSize = 4.0;
    this.debugLineWidth = 1.0;
    this.debugVao = null;
    this.debugVbo = null;
    this.debugIbo = null;
    this.debugIndexCount = 0;
    this.debugThumbTex = null;
    this.debugThumbProg = null;
    this.debugThumbMvpLoc = null;
    this.debugThumbVao = null;
    this.debugThumbQuadVbo = null;

    this._buildShaders();
    this.initializeRenderer();
  }

  _log(...args) {
    if (this.debugLog) {
      // eslint-disable-next-line no-console
      console.debug("[nicx-webgl]", ...args);
    }
  }

  _buildShaders() {
    const gl = this.gl;

    const vs = `#version 300 es
precision highp float;
layout(location=0) in float aVX;
layout(location=1) in float aVY;
layout(location=2) in float aUX;
layout(location=3) in float aUY;
layout(location=4) in float aDX;
layout(location=5) in float aDY;
uniform mat4 mvp;
uniform vec2 offset;
out vec2 vUV;
void main() {
  gl_Position = mvp * vec4(aVX - offset.x + aDX, aVY - offset.y + aDY, 0.0, 1.0);
  vUV = vec2(aUX, aUY);
}`;

    const fsStage1 = `#version 300 es
precision highp float;
in vec2 vUV;
uniform sampler2D albedo;
uniform float opacity;
uniform vec3 multColor;
uniform vec3 screenColor;
layout(location=0) out vec4 outAlbedo;
void main() {
  vec4 texColor = texture(albedo, vUV);
  vec3 screenOut = vec3(1.0) - ((vec3(1.0) - texColor.xyz) * (vec3(1.0) - (screenColor * texColor.a)));
  outAlbedo = vec4(screenOut.xyz, texColor.a) * vec4(multColor.xyz, 1.0) * opacity;
}`;

    const fsStage2 = `#version 300 es
precision highp float;
in vec2 vUV;
uniform sampler2D albedo;
uniform sampler2D emissive;
uniform sampler2D bumpmap;
uniform float opacity;
uniform vec3 multColor;
uniform vec3 screenColor;
uniform float emissionStrength;
layout(location=1) out vec4 outEmissive;
layout(location=2) out vec4 outBump;
vec4 screen(vec3 tcol, float a) {
  return vec4(vec3(1.0) - ((vec3(1.0) - tcol) * (vec3(1.0) - (screenColor * a))), a);
}
void main() {
  vec4 texColor = texture(albedo, vUV);
  vec4 emiColor = texture(emissive, vUV);
  vec4 bmpColor = texture(bumpmap, vUV);
  vec4 mult = vec4(multColor.xyz, 1.0);
  vec4 emissionOut = screen(emiColor.xyz, texColor.a) * mult * emissionStrength;
  outEmissive = emissionOut * texColor.a;
  outBump = bmpColor * texColor.a;
}`;

    const fsStage3 = `#version 300 es
precision highp float;
in vec2 vUV;
uniform sampler2D albedo;
uniform sampler2D emissive;
uniform sampler2D bumpmap;
uniform float opacity;
uniform vec3 multColor;
uniform vec3 screenColor;
uniform float emissionStrength;
layout(location=0) out vec4 outAlbedo;
layout(location=1) out vec4 outEmissive;
layout(location=2) out vec4 outBump;
vec4 screen(vec3 tcol, float a) {
  return vec4(vec3(1.0) - ((vec3(1.0) - tcol) * (vec3(1.0) - (screenColor * a))), a);
}
void main() {
  vec4 texColor = texture(albedo, vUV);
  vec4 emiColor = texture(emissive, vUV);
  vec4 bmpColor = texture(bumpmap, vUV);
  vec4 mult = vec4(multColor.xyz, 1.0);
  vec4 albedoOut = screen(texColor.xyz, texColor.a) * mult;
  vec4 emissionOut = screen(emiColor.xyz, texColor.a) * mult * emissionStrength;
  outAlbedo = albedoOut * opacity;
  outEmissive = emissionOut * outAlbedo.a;
  outBump = bmpColor * outAlbedo.a;
}`;

    const fsMaskPart = `#version 300 es
precision highp float;
in vec2 vUV;
uniform sampler2D tex;
uniform float threshold;
out vec4 outColor;
void main() {
  vec4 color = texture(tex, vUV);
  if (color.a <= threshold) discard;
  outColor = vec4(1.0, 1.0, 1.0, 1.0);
}`;

    const maskVs = `#version 300 es
precision highp float;
uniform mat4 mvp;
uniform vec2 offset;
layout(location=0) in float vertX;
layout(location=1) in float vertY;
layout(location=2) in float deformX;
layout(location=3) in float deformY;
void main() {
  gl_Position = mvp * vec4(vertX - offset.x + deformX, vertY - offset.y + deformY, 0.0, 1.0);
}`;

    const fsMask = `#version 300 es
precision highp float;
out vec4 outColor;
void main() {
  outColor = vec4(0.0, 0.0, 0.0, 1.0);
}`;

    const dbgVs = `#version 300 es
precision highp float;
layout(location=0) in vec2 inPos;
uniform mat4 mvp;
void main() {
  gl_Position = mvp * vec4(inPos, 0.0, 1.0);
}`;

    const dbgFs = `#version 300 es
precision highp float;
uniform vec4 inColor;
out vec4 outColor;
void main() {
  outColor = inColor;
}`;

    const ppVs = `#version 300 es
precision highp float;
layout(location=0) in vec2 inPos;
layout(location=1) in vec2 inUv;
out vec2 texUVs;
void main() {
  texUVs = inUv;
  gl_Position = vec4(inPos, 0.0, 1.0);
}`;

    const ppFs = `#version 300 es
precision highp float;
in vec2 texUVs;
uniform sampler2D albedo;
uniform sampler2D emissive;
uniform sampler2D bumpmap;
out vec4 outColor;
void main() {
  vec4 a = texture(albedo, texUVs);
  vec4 e = texture(emissive, texUVs);
  outColor = vec4(a.rgb + e.rgb, a.a);
}`;

    this.partShaderStage1 = createProgram(gl, vs, fsStage1);
    this.partShaderStage2 = createProgram(gl, vs, fsStage2);
    this.partShader = createProgram(gl, vs, fsStage3);
    this.partMaskShader = createProgram(gl, vs, fsMaskPart);
    this.maskShader = createProgram(gl, maskVs, fsMask);
    this.postProcessDefaultProgram = createProgram(gl, ppVs, ppFs);
    this.debugProgram = createProgram(gl, dbgVs, dbgFs);

    this.uPart1 = this._partUniforms(this.partShaderStage1);
    this.uPart2 = this._partUniforms(this.partShaderStage2);
    this.uPart3 = this._partUniforms(this.partShader);

    this.uPartMask = {
      mvp: gl.getUniformLocation(this.partMaskShader, "mvp"),
      offset: gl.getUniformLocation(this.partMaskShader, "offset"),
      threshold: gl.getUniformLocation(this.partMaskShader, "threshold"),
      tex: gl.getUniformLocation(this.partMaskShader, "tex"),
    };

    this.uMask = {
      mvp: gl.getUniformLocation(this.maskShader, "mvp"),
      offset: gl.getUniformLocation(this.maskShader, "offset"),
    };

    this.uPost = {
      albedo: gl.getUniformLocation(this.postProcessDefaultProgram, "albedo"),
      emissive: gl.getUniformLocation(this.postProcessDefaultProgram, "emissive"),
      bump: gl.getUniformLocation(this.postProcessDefaultProgram, "bumpmap"),
    };

    this.uDebug = {
      mvp: gl.getUniformLocation(this.debugProgram, "mvp"),
      color: gl.getUniformLocation(this.debugProgram, "inColor"),
    };
  }

  _partUniforms(program) {
    const gl = this.gl;
    return {
      mvp: gl.getUniformLocation(program, "mvp"),
      offset: gl.getUniformLocation(program, "offset"),
      opacity: gl.getUniformLocation(program, "opacity"),
      mult: gl.getUniformLocation(program, "multColor"),
      screen: gl.getUniformLocation(program, "screenColor"),
      albedo: gl.getUniformLocation(program, "albedo"),
      emissive: gl.getUniformLocation(program, "emissive"),
      bump: gl.getUniformLocation(program, "bumpmap"),
      emissionStrength: gl.getUniformLocation(program, "emissionStrength"),
    };
  }

  initializeRenderer() {
    const gl = this.gl;

    if (!this.drawableVAO) this.drawableVAO = gl.createVertexArray();
    if (!this.sharedVertexBuffer) this.sharedVertexBuffer = gl.createBuffer();
    if (!this.sharedUvBuffer) this.sharedUvBuffer = gl.createBuffer();
    if (!this.sharedDeformBuffer) this.sharedDeformBuffer = gl.createBuffer();

    gl.bindVertexArray(this.drawableVAO);
    gl.disable(gl.DEPTH_TEST);
    gl.enable(gl.BLEND);
    gl.blendEquation(gl.FUNC_ADD);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
  }

  initializePartBackendResources() {}
  initializeMaskBackend() {}

  dispose() {
    const gl = this.gl;

    for (const tex of this.texturesByHandle.values()) {
      if (tex.texture) gl.deleteTexture(tex.texture);
    }
    this.texturesByHandle.clear();

    for (const ibo of this.indexBuffersByHandle.values()) {
      gl.deleteBuffer(ibo);
    }
    this.indexBuffersByHandle.clear();

    for (const fbo of this.dynamicFramebufferCache.values()) {
      gl.deleteFramebuffer(fbo);
    }
    this.dynamicFramebufferCache.clear();

    if (this.sharedVertexBuffer) gl.deleteBuffer(this.sharedVertexBuffer);
    if (this.sharedUvBuffer) gl.deleteBuffer(this.sharedUvBuffer);
    if (this.sharedDeformBuffer) gl.deleteBuffer(this.sharedDeformBuffer);
    if (this.drawableVAO) gl.deleteVertexArray(this.drawableVAO);

    if (this.partShader) gl.deleteProgram(this.partShader);
    if (this.partShaderStage1) gl.deleteProgram(this.partShaderStage1);
    if (this.partShaderStage2) gl.deleteProgram(this.partShaderStage2);
    if (this.partMaskShader) gl.deleteProgram(this.partMaskShader);
    if (this.maskShader) gl.deleteProgram(this.maskShader);
    if (this.postProcessDefaultProgram) gl.deleteProgram(this.postProcessDefaultProgram);
    if (this.debugProgram) gl.deleteProgram(this.debugProgram);
    if (this.debugThumbProg) gl.deleteProgram(this.debugThumbProg);
    if (this.debugThumbTex) gl.deleteTexture(this.debugThumbTex);
    if (this.debugThumbQuadVbo) gl.deleteBuffer(this.debugThumbQuadVbo);
    if (this.debugThumbVao) gl.deleteVertexArray(this.debugThumbVao);
    if (this.sceneVBO) gl.deleteBuffer(this.sceneVBO);
    if (this.sceneVAO) gl.deleteVertexArray(this.sceneVAO);
    if (this.presentProgram) gl.deleteProgram(this.presentProgram);
    if (this.presentVBO) gl.deleteBuffer(this.presentVBO);
    if (this.presentVAO) gl.deleteVertexArray(this.presentVAO);
    if (this.presentCopyTex) gl.deleteTexture(this.presentCopyTex);
    if (this.fBuffer) gl.deleteFramebuffer(this.fBuffer);
    if (this.cfBuffer) gl.deleteFramebuffer(this.cfBuffer);
    if (this.fAlbedo) gl.deleteTexture(this.fAlbedo);
    if (this.fEmissive) gl.deleteTexture(this.fEmissive);
    if (this.fBump) gl.deleteTexture(this.fBump);
    if (this.fStencil) gl.deleteTexture(this.fStencil);
    if (this.cfAlbedo) gl.deleteTexture(this.cfAlbedo);
    if (this.cfEmissive) gl.deleteTexture(this.cfEmissive);
    if (this.cfBump) gl.deleteTexture(this.cfBump);
    if (this.cfStencil) gl.deleteTexture(this.cfStencil);
    if (this.postTmpTex) gl.deleteTexture(this.postTmpTex);
    if (this.postTmpFbo) gl.deleteFramebuffer(this.postTmpFbo);
    if (this.postTmpTex2) gl.deleteTexture(this.postTmpTex2);
    if (this.postTmpFbo2) gl.deleteFramebuffer(this.postTmpFbo2);
    if (this.debugVbo) gl.deleteBuffer(this.debugVbo);
    if (this.debugIbo) gl.deleteBuffer(this.debugIbo);
    if (this.debugVao) gl.deleteVertexArray(this.debugVao);
  }

  setViewport(width, height) {
    this.gl.viewport(0, 0, width | 0, height | 0);
  }

  setClearColor(r, g, b, a) {
    this.clearColor = [Number(r || 0), Number(g || 0), Number(b || 0), Number(a || 0)];
  }

  setSceneAmbientLight(r, g, b, a) {
    this.sceneAmbientLight = [Number(r || 0), Number(g || 0), Number(b || 0), Number(a || 0)];
  }

  resizeViewportTargets(width, height) {
    this.setViewport(width, height);
    this._ensureSceneTargets(width | 0, height | 0);
  }

  pushViewport(width, height) {
    this.viewportWidthStack.push(width | 0);
    this.viewportHeightStack.push(height | 0);
  }

  popViewport() {
    if (this.viewportWidthStack.length) this.viewportWidthStack.pop();
    if (this.viewportHeightStack.length) this.viewportHeightStack.pop();
  }

  getViewport() {
    const gl = this.gl;
    if (!this.viewportWidthStack.length || !this.viewportHeightStack.length) {
      return [gl.drawingBufferWidth | 0, gl.drawingBufferHeight | 0];
    }
    return [
      this.viewportWidthStack[this.viewportWidthStack.length - 1] | 0,
      this.viewportHeightStack[this.viewportHeightStack.length - 1] | 0,
    ];
  }

  _makeColorTarget(width, height) {
    const gl = this.gl;
    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width | 0, height | 0, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    return tex;
  }

  _makeStencilTarget(width, height) {
    const gl = this.gl;
    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.DEPTH24_STENCIL8,
      width | 0,
      height | 0,
      0,
      gl.DEPTH_STENCIL,
      gl.UNSIGNED_INT_24_8,
      null,
    );
    return tex;
  }

  _ensureSceneTargets(width, height) {
    const gl = this.gl;
    const w = Math.max(1, width | 0);
    const h = Math.max(1, height | 0);
    if (this.fBuffer && this.cfBuffer && this.sceneTargetWidth === w && this.sceneTargetHeight === h) return;

    if (this.fBuffer) gl.deleteFramebuffer(this.fBuffer);
    if (this.cfBuffer) gl.deleteFramebuffer(this.cfBuffer);
    if (this.fAlbedo) gl.deleteTexture(this.fAlbedo);
    if (this.fEmissive) gl.deleteTexture(this.fEmissive);
    if (this.fBump) gl.deleteTexture(this.fBump);
    if (this.fStencil) gl.deleteTexture(this.fStencil);
    if (this.cfAlbedo) gl.deleteTexture(this.cfAlbedo);
    if (this.cfEmissive) gl.deleteTexture(this.cfEmissive);
    if (this.cfBump) gl.deleteTexture(this.cfBump);
    if (this.cfStencil) gl.deleteTexture(this.cfStencil);

    this.fBuffer = gl.createFramebuffer();
    this.cfBuffer = gl.createFramebuffer();
    this.fAlbedo = this._makeColorTarget(w, h);
    this.fEmissive = this._makeColorTarget(w, h);
    this.fBump = this._makeColorTarget(w, h);
    this.fStencil = this._makeStencilTarget(w, h);
    this.cfAlbedo = this._makeColorTarget(w, h);
    this.cfEmissive = this._makeColorTarget(w, h);
    this.cfBump = this._makeColorTarget(w, h);
    this.cfStencil = this._makeStencilTarget(w, h);

    this.sceneTargetWidth = w;
    this.sceneTargetHeight = h;
    this.rebindActiveTargets();
  }

  rebindActiveTargets() {
    const gl = this.gl;
    if (!this.fBuffer || !this.cfBuffer) return;
    const prev = gl.getParameter(gl.FRAMEBUFFER_BINDING);

    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fBuffer);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.fAlbedo, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT1, gl.TEXTURE_2D, this.fEmissive, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT2, gl.TEXTURE_2D, this.fBump, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_STENCIL_ATTACHMENT, gl.TEXTURE_2D, this.fStencil, 0);

    gl.bindFramebuffer(gl.FRAMEBUFFER, this.cfBuffer);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.cfAlbedo, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT1, gl.TEXTURE_2D, this.cfEmissive, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT2, gl.TEXTURE_2D, this.cfBump, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_STENCIL_ATTACHMENT, gl.TEXTURE_2D, this.cfStencil, 0);

    gl.bindFramebuffer(gl.FRAMEBUFFER, prev);
  }

  beginScene() {
    const gl = this.gl;
    this._ensureSceneQuad();
    this.boundAlbedoHandle = 0;
    gl.bindVertexArray(this.sceneVAO);
    gl.enable(gl.BLEND);
    if (typeof gl.enablei === "function") {
      gl.enablei(gl.BLEND, 0);
      gl.enablei(gl.BLEND, 1);
      gl.enablei(gl.BLEND, 2);
    }
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.CULL_FACE);

    const [vw, vh] = this.getViewport();
    gl.viewport(0, 0, vw, vh);
    this._ensureSceneTargets(vw, vh);
    this.rebindActiveTargets();

    gl.bindFramebuffer(gl.FRAMEBUFFER, this.cfBuffer);
    this.setDrawBuffersSafe(3);
    gl.clearColor(0, 0, 0, 0);

    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fBuffer);
    gl.checkFramebufferStatus(gl.FRAMEBUFFER);
    gl.getError();
    this.setDrawBuffersSafe(1);
    gl.clearColor(this.clearColor[0], this.clearColor[1], this.clearColor[2], this.clearColor[3]);
    gl.clear(gl.COLOR_BUFFER_BIT);

    const attEmissiveType = gl.getFramebufferAttachmentParameter(
      gl.FRAMEBUFFER,
      gl.COLOR_ATTACHMENT1,
      gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
    );
    const attBumpType = gl.getFramebufferAttachmentParameter(
      gl.FRAMEBUFFER,
      gl.COLOR_ATTACHMENT2,
      gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
    );
    const hasAtt1 = !!attEmissiveType && attEmissiveType !== gl.NONE;
    const hasAtt2 = !!attBumpType && attBumpType !== gl.NONE;
    if (hasAtt1 || hasAtt2) {
      gl.drawBuffers([
        gl.NONE,
        hasAtt1 ? gl.COLOR_ATTACHMENT1 : gl.NONE,
        hasAtt2 ? gl.COLOR_ATTACHMENT2 : gl.NONE,
      ]);
      gl.clearColor(0, 0, 0, 1);
      gl.clear(gl.COLOR_BUFFER_BIT);
    }

    this.setDrawBuffersSafe(3);
    gl.activeTexture(gl.TEXTURE0);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
  }

  endScene() {
    const gl = this.gl;
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    if (typeof gl.disablei === "function") {
      gl.disablei(gl.BLEND, 0);
      gl.disablei(gl.BLEND, 1);
      gl.disablei(gl.BLEND, 2);
    }
    gl.enable(gl.DEPTH_TEST);
    gl.enable(gl.CULL_FACE);
    gl.disable(gl.BLEND);
    gl.useProgram(null);
    gl.bindVertexArray(null);
    gl.flush();
    if (gl.getParameter(gl.FRAMEBUFFER_BINDING)) gl.drawBuffers([gl.COLOR_ATTACHMENT0]);
  }

  _ensureSceneQuad() {
    const gl = this.gl;
    if (this.sceneVAO && this.sceneVBO) return;

    this.sceneVAO = gl.createVertexArray();
    this.sceneVBO = gl.createBuffer();

    gl.bindVertexArray(this.sceneVAO);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.sceneVBO);
    const quad = new Float32Array([
      -1, -1, 0, 0,
       1, -1, 1, 0,
      -1,  1, 0, 1,
      -1,  1, 0, 1,
       1, -1, 1, 0,
       1,  1, 1, 1,
    ]);
    gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 16, 0);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 16, 8);
  }

  _ensurePresentProgram() {
    const gl = this.gl;
    if (this.presentProgram && this.presentVAO && this.presentVBO) return;

    const vs = `#version 300 es
precision highp float;
layout(location=0) in vec2 inPos;
layout(location=1) in vec2 inUv;
out vec2 texUVs;
void main() {
  texUVs = inUv;
  gl_Position = vec4(inPos, 0.0, 1.0);
}`;

    const fs = `#version 300 es
precision highp float;
in vec2 texUVs;
out vec4 outColor;
uniform sampler2D srcTex;
uniform int useColorKey;
void main() {
  vec4 c = texture(srcTex, texUVs);
  if (useColorKey != 0) {
    if (c.a <= 0.001) {
      outColor = vec4(1.0, 0.0, 1.0, 1.0);
    } else {
      vec3 straight = clamp(c.rgb / max(c.a, 0.0001), 0.0, 1.0);
      outColor = vec4(straight, 1.0);
    }
  } else {
    outColor = c;
  }
}`;

    this.presentProgram = createProgram(gl, vs, fs);
    this.presentTexUniform = gl.getUniformLocation(this.presentProgram, "srcTex");
    this.presentUseColorKeyUniform = gl.getUniformLocation(this.presentProgram, "useColorKey");

    this.presentVAO = gl.createVertexArray();
    this.presentVBO = gl.createBuffer();
    gl.bindVertexArray(this.presentVAO);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.presentVBO);
    const quad = new Float32Array([
      -1.0, -1.0, 0.0, 0.0,
       1.0, -1.0, 1.0, 0.0,
       1.0,  1.0, 1.0, 1.0,
      -1.0, -1.0, 0.0, 0.0,
       1.0,  1.0, 1.0, 1.0,
      -1.0,  1.0, 0.0, 1.0,
    ]);
    gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 16, 0);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 16, 8);
    gl.bindVertexArray(null);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
  }

  _ensurePresentCopyTexture(width, height) {
    const gl = this.gl;
    const w = width | 0;
    const h = height | 0;
    if (this.presentCopyTex && this.presentCopyWidth === w && this.presentCopyHeight === h) return;

    if (this.presentCopyTex) gl.deleteTexture(this.presentCopyTex);
    this.presentCopyTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this.presentCopyTex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    this.presentCopyWidth = w;
    this.presentCopyHeight = h;
  }

  _resolvePresentSourceTexture(width, height) {
    const gl = this.gl;
    if (this.fAlbedo) return this.fAlbedo;

    const sourceFb = gl.getParameter(gl.FRAMEBUFFER_BINDING);
    if (sourceFb) {
      const type = gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
      if (type === gl.TEXTURE) {
        return gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
      }
    }

    this._ensurePresentCopyTexture(width, height);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.bindTexture(gl.TEXTURE_2D, this.presentCopyTex);
    gl.copyTexImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 0, 0, width | 0, height | 0, 0);
    return this.presentCopyTex;
  }

  _ensurePostTemp(width, height) {
    const gl = this.gl;
    const w = width | 0;
    const h = height | 0;
    if (this.postTmpTex && this.postTmpFbo && this.postTmpTex2 && this.postTmpFbo2 && this.postTmpWidth === w && this.postTmpHeight === h) {
      return;
    }
    if (this.postTmpTex) gl.deleteTexture(this.postTmpTex);
    if (this.postTmpFbo) gl.deleteFramebuffer(this.postTmpFbo);
    if (this.postTmpTex2) gl.deleteTexture(this.postTmpTex2);
    if (this.postTmpFbo2) gl.deleteFramebuffer(this.postTmpFbo2);

    this.postTmpTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this.postTmpTex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);

    this.postTmpFbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.postTmpFbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.postTmpTex, 0);

    this.postTmpTex2 = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this.postTmpTex2);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);

    this.postTmpFbo2 = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.postTmpFbo2);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.postTmpTex2, 0);

    this.postTmpWidth = w;
    this.postTmpHeight = h;
  }

  _ortho(left, right, bottom, top, near, far) {
    const rl = right - left;
    const tb = top - bottom;
    const fn = far - near;
    return new Float32Array([
      2 / rl, 0, 0, 0,
      0, 2 / tb, 0, 0,
      0, 0, -2 / fn, 0,
      -(right + left) / rl, -(top + bottom) / tb, -(far + near) / fn, 1,
    ]);
  }

  _translate(x, y, z) {
    return new Float32Array([
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      x, y, z, 1,
    ]);
  }

  _mulMat4(a, b) {
    const out = new Float32Array(16);
    for (let c = 0; c < 4; c += 1) {
      for (let r = 0; r < 4; r += 1) {
        out[c * 4 + r] =
          a[0 * 4 + r] * b[c * 4 + 0] +
          a[1 * 4 + r] * b[c * 4 + 1] +
          a[2 * 4 + r] * b[c * 4 + 2] +
          a[3 * 4 + r] * b[c * 4 + 3];
      }
    }
    return out;
  }

  // D backend sends mat4 with glUniformMatrix4fv(..., GL_TRUE, ...).
  // WebGL requires transpose=false, so transpose on JS side.
  _transposeMat4(m) {
    const inM = asFloat32(m);
    const out = new Float32Array(16);
    for (let r = 0; r < 4; r += 1) {
      for (let c = 0; c < 4; c += 1) {
        out[c * 4 + r] = inM[r * 4 + c];
      }
    }
    return out;
  }

  _mulMat4RowMajor(a, b) {
    const lhs = asFloat32(a);
    const rhs = asFloat32(b);
    const out = new Float32Array(16);
    for (let r = 0; r < 4; r += 1) {
      for (let c = 0; c < 4; c += 1) {
        out[r * 4 + c] =
          lhs[r * 4 + 0] * rhs[0 * 4 + c] +
          lhs[r * 4 + 1] * rhs[1 * 4 + c] +
          lhs[r * 4 + 2] * rhs[2 * 4 + c] +
          lhs[r * 4 + 3] * rhs[3 * 4 + c];
      }
    }
    return out;
  }

  _drawPostScene(program, albedoTex, emissiveTex, bumpTex, area) {
    const gl = this.gl;
    this._ensureSceneQuad();
    const width = Math.max(1, area.z | 0);
    const height = Math.max(1, area.w | 0);

    gl.viewport(0, 0, width | 0, height | 0);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.CULL_FACE);
    gl.enable(gl.BLEND);
    gl.blendEquation(gl.FUNC_ADD);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);

    const prog = program || this.postProcessDefaultProgram;
    gl.useProgram(prog);

    const mvpLoc = gl.getUniformLocation(prog, "mvp");
    const ambientLoc = gl.getUniformLocation(prog, "ambientLight");
    const fbSizeLoc = gl.getUniformLocation(prog, "fbSize");
    const albedoLoc = gl.getUniformLocation(prog, "albedo");
    const emissiveLoc = gl.getUniformLocation(prog, "emissive");
    const bumpLoc = gl.getUniformLocation(prog, "bumpmap");
    if (mvpLoc !== null && mvpLoc !== -1) {
      const ortho = this._ortho(0, area.z, area.w, 0, 0, Math.max(area.z, area.w));
      const trans = this._translate(area.x, area.y, 0);
      const mvp = this._mulMat4(ortho, trans);
      gl.uniformMatrix4fv(
        mvpLoc,
        false,
        mvp,
      );
    }
    if (ambientLoc !== null && ambientLoc !== -1) {
      gl.uniform4f(
        ambientLoc,
        this.sceneAmbientLight[0],
        this.sceneAmbientLight[1],
        this.sceneAmbientLight[2],
        this.sceneAmbientLight[3],
      );
    }
    if (fbSizeLoc !== null && fbSizeLoc !== -1) {
      gl.uniform2f(fbSizeLoc, width | 0, height | 0);
    }
    if (albedoLoc !== null && albedoLoc !== -1) gl.uniform1i(albedoLoc, 0);
    if (emissiveLoc !== null && emissiveLoc !== -1) gl.uniform1i(emissiveLoc, 1);
    if (bumpLoc !== null && bumpLoc !== -1) gl.uniform1i(bumpLoc, 2);

    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, albedoTex || null);
    gl.activeTexture(gl.TEXTURE1);
    gl.bindTexture(gl.TEXTURE_2D, emissiveTex || albedoTex || null);
    gl.activeTexture(gl.TEXTURE2);
    gl.bindTexture(gl.TEXTURE_2D, bumpTex || albedoTex || null);

    gl.bindVertexArray(this.sceneVAO);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
  }

  postProcessScene() {
    if (!this.postProcessingStack.length) return;

    const gl = this.gl;
    const [width, height] = this.getViewport();
    if (width <= 0 || height <= 0 || !this.fBuffer || !this.cfBuffer) return;
    const area = { x: 0, y: 0, z: width, w: height };

    const data = new Float32Array([
      area.x, area.y + area.w, 0, 0,
      area.x, area.y, 0, 1,
      area.x + area.z, area.y + area.w, 1, 0,
      area.x + area.z, area.y + area.w, 1, 0,
      area.x, area.y, 0, 1,
      area.x + area.z, area.y, 1, 1,
    ]);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.sceneVBO);
    gl.bufferData(gl.ARRAY_BUFFER, data, gl.DYNAMIC_DRAW);

    let targetBuffer = false;

    gl.activeTexture(gl.TEXTURE1);
    gl.bindTexture(gl.TEXTURE_2D, this.fEmissive);
    gl.generateMipmap(gl.TEXTURE_2D);

    gl.bindFramebuffer(gl.FRAMEBUFFER, this.cfBuffer);
    this.setDrawBuffersSafe(3);
    gl.clearColor(this.clearColor[0], this.clearColor[1], this.clearColor[2], this.clearColor[3]);
    gl.clear(gl.COLOR_BUFFER_BIT);

    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fBuffer);
    this.setDrawBuffersSafe(3);

    for (const shader of this.postProcessingStack) {
      targetBuffer = !targetBuffer;
      if (targetBuffer) {
        gl.bindFramebuffer(gl.FRAMEBUFFER, this.cfBuffer);
        this._drawPostScene(shader, this.fAlbedo, this.fEmissive, this.fBump, area);
      } else {
        gl.bindFramebuffer(gl.FRAMEBUFFER, this.fBuffer);
        this._drawPostScene(shader, this.cfAlbedo, this.cfEmissive, this.cfBump, area);
      }
    }

    if (targetBuffer) {
      gl.bindFramebuffer(gl.READ_FRAMEBUFFER, this.cfBuffer);
      gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, this.fBuffer);
      gl.blitFramebuffer(0, 0, width, height, 0, 0, width, height, gl.COLOR_BUFFER_BIT, gl.LINEAR);
    }

    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  setDebugPointSize(size) {
    this.debugPointSize = Number(size || 1);
  }

  setDebugLineWidth(size) {
    this.debugLineWidth = Number(size || 1);
  }

  addPostProcessShader(program) {
    if (program) this.postProcessingStack.push(program);
  }

  clearPostProcessShaders() {
    this.postProcessingStack.length = 0;
  }

  setThumbnailGridEnabled(enabled) {
    this.thumbnailGridEnabled = !!enabled;
  }

  _ensureDebugRenderer() {
    const gl = this.gl;
    if (this.debugVao && this.debugVbo && this.debugIbo) return;

    this.debugVao = gl.createVertexArray();
    this.debugVbo = gl.createBuffer();
    this.debugIbo = gl.createBuffer();

    gl.bindVertexArray(this.debugVao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.debugVbo);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 8, 0);
  }

  uploadDebugBuffer(positions, indices) {
    const gl = this.gl;
    this._ensureDebugRenderer();

    gl.bindVertexArray(this.debugVao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.debugVbo);
    gl.bufferData(gl.ARRAY_BUFFER, asFloat32(positions), gl.DYNAMIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.debugIbo);
    const idx = asU16(indices);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, idx, gl.DYNAMIC_DRAW);
    this.debugIndexCount = idx.length;
  }

  _toColor4(color) {
    if (Array.isArray(color)) {
      return [Number(color[0] ?? 1), Number(color[1] ?? 1), Number(color[2] ?? 1), Number(color[3] ?? 1)];
    }
    if (color && typeof color === "object") {
      return [Number(color.x ?? color.r ?? 1), Number(color.y ?? color.g ?? 1), Number(color.z ?? color.b ?? 1), Number(color.w ?? color.a ?? 1)];
    }
    return [1, 1, 1, 1];
  }

  _toMat4(trans) {
    const m = asFloat32(trans);
    if (m.length >= 16) return m.subarray(0, 16);
    return new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
  }

  drawDebugPoints(color, trans) {
    if (!this.debugIndexCount) return;
    const gl = this.gl;
    this._ensureDebugRenderer();
    const c = this._toColor4(color);

    gl.useProgram(this.debugProgram);
    gl.uniformMatrix4fv(this.uDebug.mvp, false, this._toMat4(trans));
    gl.uniform4f(this.uDebug.color, c[0], c[1], c[2], c[3]);
    gl.bindVertexArray(this.debugVao);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.debugIbo);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    gl.disable(gl.CULL_FACE);
    gl.disable(gl.DEPTH_TEST);
    gl.drawElements(gl.POINTS, this.debugIndexCount, gl.UNSIGNED_SHORT, 0);
  }

  drawDebugLines(color, trans) {
    if (!this.debugIndexCount) return;
    const gl = this.gl;
    this._ensureDebugRenderer();
    const c = this._toColor4(color);

    gl.useProgram(this.debugProgram);
    gl.uniformMatrix4fv(this.uDebug.mvp, false, this._toMat4(trans));
    gl.uniform4f(this.uDebug.color, c[0], c[1], c[2], c[3]);
    gl.bindVertexArray(this.debugVao);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.debugIbo);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    gl.disable(gl.CULL_FACE);
    gl.disable(gl.DEPTH_TEST);
    gl.lineWidth(this.debugLineWidth);
    gl.drawElements(gl.LINES, this.debugIndexCount, gl.UNSIGNED_SHORT, 0);
  }

  _ensureThumbProgram() {
    if (this.debugThumbProg) return;
    const gl = this.gl;
    const vs = `#version 300 es
precision highp float;
uniform mat4 mvp;
layout(location=0) in vec2 inPos;
layout(location=1) in vec2 inUv;
out vec2 vUv;
void main() {
  gl_Position = mvp * vec4(inPos, 0.0, 1.0);
  vUv = inUv;
}`;
    const fs = `#version 300 es
precision highp float;
in vec2 vUv;
uniform sampler2D albedo;
out vec4 outColor;
void main() {
  outColor = texture(albedo, vUv);
}`;
    this.debugThumbProg = createProgram(gl, vs, fs);
    this.debugThumbMvpLoc = gl.getUniformLocation(this.debugThumbProg, "mvp");
    gl.useProgram(this.debugThumbProg);
    const albedoLoc = gl.getUniformLocation(this.debugThumbProg, "albedo");
    if (albedoLoc !== null) gl.uniform1i(albedoLoc, 0);
    gl.useProgram(null);
  }

  _ensureThumbBuffers() {
    const gl = this.gl;
    if (!this.debugThumbVao) this.debugThumbVao = gl.createVertexArray();
    if (!this.debugThumbQuadVbo) this.debugThumbQuadVbo = gl.createBuffer();
  }

  _ensureDebugTestTex() {
    if (this.debugThumbTex) return;
    const gl = this.gl;
    const sz = 48;
    const pixels = new Uint8Array(sz * sz * 4);
    for (let y = 0; y < sz; y += 1) {
      for (let x = 0; x < sz; x += 1) {
        const on = (((x / 6) | 0) ^ ((y / 6) | 0)) & 1;
        const idx = (y * sz + x) * 4;
        pixels[idx + 0] = on ? 255 : 30;
        pixels[idx + 1] = on ? 128 : 30;
        pixels[idx + 2] = on ? 64 : 30;
        pixels[idx + 3] = 255;
      }
    }

    this.debugThumbTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this.debugThumbTex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, sz, sz, 0, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  }

  _drawThumbTile(tex, x, y, size, screenW, screenH) {
    if (!tex) return;
    const gl = this.gl;
    this._ensureThumbProgram();
    this._ensureThumbBuffers();

    const left = (x / screenW) * 2.0 - 1.0;
    const right = ((x + size) / screenW) * 2.0 - 1.0;
    const top = (y / screenH) * 2.0 - 1.0;
    const bottom = ((y + size) / screenH) * 2.0 - 1.0;

    const verts = new Float32Array([
      left, top, 0, 0,
      right, top, 1, 0,
      left, bottom, 0, 1,
      right, top, 1, 0,
      right, bottom, 1, 1,
      left, bottom, 0, 1,
    ]);

    gl.useProgram(this.debugThumbProg);
    if (this.debugThumbMvpLoc) {
      gl.uniformMatrix4fv(
        this.debugThumbMvpLoc,
        false,
        new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]),
      );
    }
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, tex);

    gl.bindVertexArray(this.debugThumbVao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.debugThumbQuadVbo);
    gl.bufferData(gl.ARRAY_BUFFER, verts, gl.STREAM_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 16, 0);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 16, 8);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    gl.bindVertexArray(null);
  }

  renderThumbnailGrid(screenW, screenH, textureHandles = []) {
    const gl = this.gl;
    const prevFb = gl.getParameter(gl.FRAMEBUFFER_BINDING);
    const prevProgram = gl.getParameter(gl.CURRENT_PROGRAM);
    const prevVao = gl.getParameter(gl.VERTEX_ARRAY_BINDING);
    const prevViewport = gl.getParameter(gl.VIEWPORT);
    const prevDepth = gl.isEnabled(gl.DEPTH_TEST);
    const prevStencil = gl.isEnabled(gl.STENCIL_TEST);
    const prevCull = gl.isEnabled(gl.CULL_FACE);
    const prevScissor = gl.isEnabled(gl.SCISSOR_TEST);

    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.STENCIL_TEST);
    gl.disable(gl.CULL_FACE);
    gl.viewport(0, 0, screenW, screenH);

    gl.enable(gl.SCISSOR_TEST);
    const tile = 48;
    const pad = 2;
    const sidebarW = (tile + pad) * 8;
    gl.scissor(0, 0, sidebarW | 0, screenH | 0);
    gl.clearColor(0.18, 0.18, 0.18, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.disable(gl.SCISSOR_TEST);

    this._ensureDebugTestTex();

    let tx = pad;
    let ty = pad;
    this._drawThumbTile(this.debugThumbTex, tx, ty, tile, screenW, screenH);
    ty += tile + pad;

    for (const h of textureHandles) {
      let tex = null;
      if (typeof h === "number") {
        tex = this.texturesByHandle.get(h)?.texture || null;
      } else if (h && typeof h === "object" && h.texture) {
        tex = h.texture;
      } else {
        tex = h;
      }
      this._drawThumbTile(tex, tx, ty, tile, screenW, screenH);
      ty += tile + pad;
      if (ty + tile > screenH - pad) {
        ty = pad;
        tx += tile + pad;
      }
    }

    if (prevDepth) gl.enable(gl.DEPTH_TEST); else gl.disable(gl.DEPTH_TEST);
    if (prevStencil) gl.enable(gl.STENCIL_TEST); else gl.disable(gl.STENCIL_TEST);
    if (prevCull) gl.enable(gl.CULL_FACE); else gl.disable(gl.CULL_FACE);
    if (prevScissor) gl.enable(gl.SCISSOR_TEST); else gl.disable(gl.SCISSOR_TEST);
    gl.bindFramebuffer(gl.FRAMEBUFFER, prevFb);
    gl.viewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    gl.useProgram(prevProgram);
    gl.bindVertexArray(prevVao);
  }

  presentSceneToBackbuffer(width, height) {
    const gl = this.gl;
    this._ensurePresentProgram();

    const srcTex = this._resolvePresentSourceTexture(width, height);

    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.viewport(0, 0, width | 0, height | 0);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.CULL_FACE);
    gl.disable(gl.BLEND);
    if (this.useColorKeyTransparency) {
      gl.clearColor(1, 0, 1, 1);
    } else {
      gl.clearColor(0, 0, 0, 0);
    }
    gl.clear(gl.COLOR_BUFFER_BIT);

    gl.useProgram(this.presentProgram);
    if (this.presentTexUniform !== null && this.presentTexUniform !== -1) gl.uniform1i(this.presentTexUniform, 0);
    if (this.presentUseColorKeyUniform !== null && this.presentUseColorKeyUniform !== -1) {
      gl.uniform1i(this.presentUseColorKeyUniform, this.useColorKeyTransparency ? 1 : 0);
    }
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, srcTex || null);
    gl.bindVertexArray(this.presentVAO);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    gl.bindVertexArray(null);
  }

  bindDrawableVao() {
    this.gl.bindVertexArray(this.drawableVAO);
  }

  bindPartShader() {
    this.gl.useProgram(this.partShader);
  }

  sharedVertexBufferHandle() {
    return this.sharedVertexBuffer;
  }

  sharedUvBufferHandle() {
    return this.sharedUvBuffer;
  }

  sharedDeformBufferHandle() {
    return this.sharedDeformBuffer;
  }

  setDrawBuffersSafe(desired) {
    const gl = this.gl;
    const fb = gl.getParameter(gl.DRAW_FRAMEBUFFER_BINDING) || gl.getParameter(gl.FRAMEBUFFER_BINDING);
    if (!fb) {
      return 1;
    }

    const bufs = [];
    const addIfPresent = (att) => {
      const type = gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, att, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
      if (type && type !== gl.NONE) bufs.push(att);
    };

    addIfPresent(gl.COLOR_ATTACHMENT0);
    if (desired > 1) addIfPresent(gl.COLOR_ATTACHMENT1);
    if (desired > 2) addIfPresent(gl.COLOR_ATTACHMENT2);

    if (bufs.length === 0) bufs.push(gl.COLOR_ATTACHMENT0);
    gl.drawBuffers(bufs);
    return bufs.length;
  }

  _collectColorAttachmentTextures() {
    const gl = this.gl;
    const out = new Set();
    const fb = gl.getParameter(gl.DRAW_FRAMEBUFFER_BINDING) || gl.getParameter(gl.FRAMEBUFFER_BINDING);
    if (!fb) return out;
    const atts = [gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1, gl.COLOR_ATTACHMENT2];
    for (const att of atts) {
      const type = gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, att, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
      if (type === gl.TEXTURE) {
        const tex = gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, att, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
        if (tex) out.add(tex);
      }
    }
    return out;
  }

  uploadSharedVertexBuffer(data) {
    const gl = this.gl;
    gl.bindBuffer(gl.ARRAY_BUFFER, this.sharedVertexBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, asFloat32(data), gl.DYNAMIC_DRAW);
  }

  uploadSharedUvBuffer(data) {
    const gl = this.gl;
    gl.bindBuffer(gl.ARRAY_BUFFER, this.sharedUvBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, asFloat32(data), gl.DYNAMIC_DRAW);
  }

  uploadSharedDeformBuffer(data) {
    const gl = this.gl;
    gl.bindBuffer(gl.ARRAY_BUFFER, this.sharedDeformBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, asFloat32(data), gl.DYNAMIC_DRAW);
  }

  createTexture(width, height, channels, _mipLevels, _format, _renderTarget, stencil) {
    const gl = this.gl;
    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, stencil ? gl.NEAREST : gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, stencil ? gl.NEAREST : gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    if (stencil) {
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.DEPTH24_STENCIL8,
        width,
        height,
        0,
        gl.DEPTH_STENCIL,
        gl.UNSIGNED_INT_24_8,
        null,
      );
    } else {
      const fmt = channels === 1 ? gl.RED : channels === 2 ? gl.RG : channels === 3 ? gl.RGB : gl.RGBA;
      gl.texImage2D(gl.TEXTURE_2D, 0, fmt, width, height, 0, fmt, gl.UNSIGNED_BYTE, null);
    }

    const handle = this._allocTextureHandle();
    this.texturesByHandle.set(handle, { texture, width, height, channels, stencil: !!stencil });
    return handle;
  }

  _channelFormat(channels) {
    const gl = this.gl;
    if (channels === 1) return gl.RED;
    if (channels === 2) return gl.RG;
    if (channels === 3) return gl.RGB;
    return gl.RGBA;
  }

  updateTexture(handle, data, dataLen, width, height, channels) {
    const gl = this.gl;
    const e = this.texturesByHandle.get(Number(handle));
    if (!e) return;
    if (e.stencil) return;

    const expected = (width | 0) * (height | 0) * (channels | 0);
    if (!data || dataLen < expected || expected <= 0) return;

    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data.buffer || data, data.byteOffset || 0, expected);
    const fmt = channels === 1 ? gl.RED : channels === 2 ? gl.RG : channels === 3 ? gl.RGB : gl.RGBA;

    gl.bindTexture(gl.TEXTURE_2D, e.texture);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texImage2D(gl.TEXTURE_2D, 0, fmt, width, height, 0, fmt, gl.UNSIGNED_BYTE, bytes.subarray(0, expected));

    e.width = width;
    e.height = height;
    e.channels = channels;
  }

  releaseTexture(handle) {
    const gl = this.gl;
    const k = Number(handle);
    const e = this.texturesByHandle.get(k);
    if (!e) return;

    gl.deleteTexture(e.texture);
    this.texturesByHandle.delete(k);

    for (const [key, fbo] of this.dynamicFramebufferCache.entries()) {
      if (key.includes(`${k},`) || key.endsWith(`,${k}`)) {
        gl.deleteFramebuffer(fbo);
        this.dynamicFramebufferCache.delete(key);
      }
    }
  }

  bindTextureHandle(handle, unit) {
    const gl = this.gl;
    const e = this.texturesByHandle.get(Number(handle));
    gl.activeTexture(gl.TEXTURE0 + Math.max(0, Math.min(31, Number(unit || 0))));
    gl.bindTexture(gl.TEXTURE_2D, e ? e.texture : null);
  }

  uploadTextureData(handle, width, height, inChannels, outChannels, stencil, data) {
    const gl = this.gl;
    const e = this.texturesByHandle.get(Number(handle));
    if (!e) return;
    gl.bindTexture(gl.TEXTURE_2D, e.texture);

    if (stencil) {
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.DEPTH24_STENCIL8,
        width | 0,
        height | 0,
        0,
        gl.DEPTH_STENCIL,
        gl.UNSIGNED_INT_24_8,
        null,
      );
      e.width = width | 0;
      e.height = height | 0;
      e.channels = outChannels | 0;
      e.stencil = true;
      return;
    }

    const inFmt = this._channelFormat(inChannels | 0);
    const outFmt = this._channelFormat(outChannels | 0);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.pixelStorei(gl.PACK_ALIGNMENT, 1);
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data?.buffer || data || 0, data?.byteOffset || 0, data?.byteLength || 0);
    gl.texImage2D(gl.TEXTURE_2D, 0, outFmt, width | 0, height | 0, 0, inFmt, gl.UNSIGNED_BYTE, bytes);
    e.width = width | 0;
    e.height = height | 0;
    e.channels = outChannels | 0;
    e.stencil = false;
  }

  generateTextureMipmap(handle) {
    const gl = this.gl;
    const e = this.texturesByHandle.get(Number(handle));
    if (!e || e.stencil) return;
    gl.bindTexture(gl.TEXTURE_2D, e.texture);
    gl.generateMipmap(gl.TEXTURE_2D);
  }

  applyTextureFiltering(handle, filtering, useMipmaps = true) {
    const gl = this.gl;
    const e = this.texturesByHandle.get(Number(handle));
    if (!e) return;
    gl.bindTexture(gl.TEXTURE_2D, e.texture);
    const linear = Number(filtering) === Filtering.Linear;
    const minFilter = useMipmaps
      ? (linear ? gl.LINEAR_MIPMAP_LINEAR : gl.NEAREST_MIPMAP_NEAREST)
      : (linear ? gl.LINEAR : gl.NEAREST);
    const magFilter = linear ? gl.LINEAR : gl.NEAREST;
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, minFilter);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, magFilter);
  }

  applyTextureWrapping(handle, wrapping) {
    const gl = this.gl;
    const e = this.texturesByHandle.get(Number(handle));
    if (!e) return;
    gl.bindTexture(gl.TEXTURE_2D, e.texture);
    let wrapValue = gl.CLAMP_TO_EDGE;
    if (Number(wrapping) === Wrapping.Repeat) {
      wrapValue = gl.REPEAT;
    } else if (Number(wrapping) === Wrapping.Mirror) {
      wrapValue = gl.MIRRORED_REPEAT;
    } else if (this.extTextureBorderClamp && this.extTextureBorderClamp.CLAMP_TO_BORDER_EXT) {
      wrapValue = this.extTextureBorderClamp.CLAMP_TO_BORDER_EXT;
    }
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, wrapValue);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, wrapValue);
    if (Number(wrapping) === Wrapping.Clamp && this.extTextureBorderClamp) {
      const pname = this.extTextureBorderClamp.TEXTURE_BORDER_COLOR_EXT;
      if (pname) gl.texParameterfv(gl.TEXTURE_2D, pname, new Float32Array([0, 0, 0, 0]));
    }
  }

  applyTextureAnisotropy(handle, value) {
    const gl = this.gl;
    const e = this.texturesByHandle.get(Number(handle));
    if (!e || !this.extAnisotropy) return;
    gl.bindTexture(gl.TEXTURE_2D, e.texture);
    gl.texParameterf(gl.TEXTURE_2D, this.extAnisotropy.TEXTURE_MAX_ANISOTROPY_EXT, Number(value || 1));
  }

  readTextureData(handle, channels, stencil, outBuffer) {
    const gl = this.gl;
    const e = this.texturesByHandle.get(Number(handle));
    if (!e || !e.texture) return;

    const prevFb = gl.getParameter(gl.FRAMEBUFFER_BINDING);
    const fb = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
    const attachment = stencil ? gl.DEPTH_STENCIL_ATTACHMENT : gl.COLOR_ATTACHMENT0;
    gl.framebufferTexture2D(gl.FRAMEBUFFER, attachment, gl.TEXTURE_2D, e.texture, 0);

    const w = e.width | 0;
    const h = e.height | 0;
    if (w <= 0 || h <= 0) {
      gl.bindFramebuffer(gl.FRAMEBUFFER, prevFb);
      gl.deleteFramebuffer(fb);
      return;
    }

    if (stencil) {
      const tmp = new Uint32Array(w * h);
      gl.readPixels(0, 0, w, h, gl.DEPTH_STENCIL, gl.UNSIGNED_INT_24_8, tmp);
      if (outBuffer && outBuffer.set) outBuffer.set(new Uint8Array(tmp.buffer));
    } else {
      const fmt = this._channelFormat(channels | 0);
      const elems = Math.max(1, channels | 0);
      const tmp = new Uint8Array(w * h * elems);
      gl.readPixels(0, 0, w, h, fmt, gl.UNSIGNED_BYTE, tmp);
      if (outBuffer && outBuffer.set) outBuffer.set(tmp);
    }

    gl.bindFramebuffer(gl.FRAMEBUFFER, prevFb);
    gl.deleteFramebuffer(fb);
  }

  createUnityResourceCallbacks() {
    return {
      createTexture: (w, h, channels, mipLevels, format, renderTarget, stencil) =>
        this.createTexture(w, h, channels, mipLevels, format, renderTarget, stencil),
      updateTexture: (handle, data, dataLen, w, h, channels) =>
        this.updateTexture(handle, data, dataLen, w, h, channels),
      releaseTexture: (handle) => this.releaseTexture(handle),
    };
  }

  supportsAdvancedBlend() {
    return !!(this.extAdvancedBlend || this.extAdvancedBlendCoherent);
  }

  supportsAdvancedBlendCoherent() {
    return !!this.extAdvancedBlendCoherent;
  }

  applyBlendingCapabilities() {
    const desiredAdvanced = this.supportsAdvancedBlend();
    const desiredCoherent = this.supportsAdvancedBlendCoherent();
    if (desiredCoherent !== this.advancedBlendingCoherent) {
      this.setAdvancedBlendCoherent(desiredCoherent);
    }
    this.advancedBlending = desiredAdvanced;
    this.advancedBlendingCoherent = desiredCoherent;
  }

  setAdvancedBlendCoherent(enabled) {
    this.advancedBlendingCoherent = !!enabled;
  }

  setLegacyBlendMode(mode) {
    const gl = this.gl;
    switch (mode) {
      case BlendMode.Normal:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
        break;
      case BlendMode.Multiply:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.DST_COLOR, gl.ONE_MINUS_SRC_ALPHA);
        break;
      case BlendMode.Screen:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_COLOR);
        break;
      case BlendMode.Lighten:
        gl.blendEquation(gl.MAX);
        gl.blendFunc(gl.ONE, gl.ONE);
        break;
      case BlendMode.ColorDodge:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.DST_COLOR, gl.ONE);
        break;
      case BlendMode.LinearDodge:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFuncSeparate(gl.ONE, gl.ONE_MINUS_SRC_COLOR, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
        break;
      case BlendMode.AddGlow:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFuncSeparate(gl.ONE, gl.ONE, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
        break;
      case BlendMode.Subtract:
        gl.blendEquationSeparate(gl.FUNC_REVERSE_SUBTRACT, gl.FUNC_ADD);
        gl.blendFunc(gl.ONE_MINUS_DST_COLOR, gl.ONE);
        break;
      case BlendMode.Exclusion:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFuncSeparate(gl.ONE_MINUS_DST_COLOR, gl.ONE_MINUS_SRC_COLOR, gl.ONE, gl.ONE);
        break;
      case BlendMode.Inverse:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.ONE_MINUS_DST_COLOR, gl.ONE_MINUS_SRC_ALPHA);
        break;
      case BlendMode.DestinationIn:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.ZERO, gl.SRC_ALPHA);
        break;
      case BlendMode.ClipToLower:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.DST_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
        break;
      case BlendMode.SliceFromLower:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.ZERO, gl.ONE_MINUS_SRC_ALPHA);
        break;
      default:
        gl.blendEquation(gl.FUNC_ADD);
        gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
        break;
    }
  }

  setAdvancedBlendEquation(mode) {
    const gl = this.gl;
    const ext = this.extAdvancedBlendCoherent || this.extAdvancedBlend;
    if (!ext) {
      this.setLegacyBlendMode(mode);
      return;
    }

    const map = new Map([
      [BlendMode.Multiply, ext.MULTIPLY_KHR],
      [BlendMode.Screen, ext.SCREEN_KHR],
      [BlendMode.Overlay, ext.OVERLAY_KHR],
      [BlendMode.Darken, ext.DARKEN_KHR],
      [BlendMode.Lighten, ext.LIGHTEN_KHR],
      [BlendMode.ColorDodge, ext.COLORDODGE_KHR],
      [BlendMode.ColorBurn, ext.COLORBURN_KHR],
      [BlendMode.HardLight, ext.HARDLIGHT_KHR],
      [BlendMode.SoftLight, ext.SOFTLIGHT_KHR],
      [BlendMode.Difference, ext.DIFFERENCE_KHR],
      [BlendMode.Exclusion, ext.EXCLUSION_KHR],
    ]);
    const eq = map.get(mode);
    if (typeof eq === "number") gl.blendEquation(eq);
    else this.setLegacyBlendMode(mode);
  }

  isAdvancedBlendMode(mode) {
    return mode === BlendMode.Multiply || mode === BlendMode.Screen || mode === BlendMode.Overlay ||
      mode === BlendMode.Darken || mode === BlendMode.Lighten || mode === BlendMode.ColorDodge ||
      mode === BlendMode.ColorBurn || mode === BlendMode.HardLight || mode === BlendMode.SoftLight ||
      mode === BlendMode.Difference || mode === BlendMode.Exclusion;
  }

  applyBlendMode(mode, legacyOnly = false) {
    if (!this.advancedBlending || legacyOnly) this.setLegacyBlendMode(mode);
    else this.setAdvancedBlendEquation(mode);
  }

  blendModeBarrier(mode) {
    if (this.advancedBlending && !this.advancedBlendingCoherent && this.isAdvancedBlendMode(mode)) {
      this.issueBlendBarrier();
    }
  }

  issueBlendBarrier() {
    const ext = this.extAdvancedBlendCoherent || this.extAdvancedBlend;
    if (ext && typeof ext.blendBarrierKHR === "function") {
      ext.blendBarrierKHR();
    }
  }

  getOrCreateIboByHandle(indexHandle, indices, indexCount) {
    const gl = this.gl;
    const h = Number(indexHandle || 0);
    if (!h || !indexCount) return null;

    let ibo = this.indexBuffersByHandle.get(h);
    if (!ibo) {
      ibo = gl.createBuffer();
      this.indexBuffersByHandle.set(h, ibo);
    }

    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
    if (indices) {
      gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, asU16(indices), gl.DYNAMIC_DRAW);
    }
    return ibo;
  }

  drawDrawableElements(indexBuffer, indexCount) {
    const gl = this.gl;
    if (!indexBuffer || !indexCount) return;
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, indexBuffer);
    gl.drawElements(gl.TRIANGLES, Number(indexCount), gl.UNSIGNED_SHORT, 0);
  }

  _bindPartSoA(packet) {
    const gl = this.gl;

    const vOff = Number(packet.vertexOffset || 0) * 4;
    const uvOff = Number(packet.uvOffset || 0) * 4;
    const dOff = Number(packet.deformOffset || 0) * 4;

    const vLane1 = Number(packet.vertexAtlasStride || 0) * 4 + vOff;
    const uvLane1 = Number(packet.uvAtlasStride || 0) * 4 + uvOff;
    const dLane1 = Number(packet.deformAtlasStride || 0) * 4 + dOff;

    gl.bindBuffer(gl.ARRAY_BUFFER, this.sharedVertexBuffer);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 1, gl.FLOAT, false, 0, vOff);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 1, gl.FLOAT, false, 0, vLane1);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.sharedUvBuffer);
    gl.enableVertexAttribArray(2);
    gl.vertexAttribPointer(2, 1, gl.FLOAT, false, 0, uvOff);
    gl.enableVertexAttribArray(3);
    gl.vertexAttribPointer(3, 1, gl.FLOAT, false, 0, uvLane1);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.sharedDeformBuffer);
    gl.enableVertexAttribArray(4);
    gl.vertexAttribPointer(4, 1, gl.FLOAT, false, 0, dOff);
    gl.enableVertexAttribArray(5);
    gl.vertexAttribPointer(5, 1, gl.FLOAT, false, 0, dLane1);
  }

  _bindMaskSoA(packet) {
    const gl = this.gl;

    const vOff = Number(packet.vertexOffset || 0) * 4;
    const dOff = Number(packet.deformOffset || 0) * 4;

    const vLane1 = Number(packet.vertexAtlasStride || 0) * 4 + vOff;
    const dLane1 = Number(packet.deformAtlasStride || 0) * 4 + dOff;

    gl.bindBuffer(gl.ARRAY_BUFFER, this.sharedVertexBuffer);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 1, gl.FLOAT, false, 0, vOff);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 1, gl.FLOAT, false, 0, vLane1);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.sharedDeformBuffer);
    gl.enableVertexAttribArray(2);
    gl.vertexAttribPointer(2, 1, gl.FLOAT, false, 0, dOff);
    gl.enableVertexAttribArray(3);
    gl.vertexAttribPointer(3, 1, gl.FLOAT, false, 0, dLane1);

    gl.disableVertexAttribArray(4);
    gl.disableVertexAttribArray(5);
  }

  setupShaderStage(packet, stage, matrix, renderMatrix) {
    const gl = this.gl;

    let program = this.partShader;
    let u = this.uPart3;

    if (stage === 0) {
      program = this.partShaderStage1;
      u = this.uPart1;
      this.setDrawBuffersSafe(1);
    } else if (stage === 1) {
      program = this.partShaderStage2;
      u = this.uPart2;
      this.setDrawBuffersSafe(2);
    } else {
      this.setDrawBuffersSafe(3);
    }

    gl.useProgram(program);

    const m = asFloat32(matrix);
    const r = asFloat32(renderMatrix);
    const mvpRowMajor = this._mulMat4RowMajor(r, m);
    const mvp = this._transposeMat4(mvpRowMajor);
    const origin = asFloat32(packet.origin || [0, 0]);
    const tint = asFloat32(packet.clampedTint || [1, 1, 1]);
    const screen = asFloat32(packet.clampedScreen || [0, 0, 0]);

    gl.uniformMatrix4fv(u.mvp, false, mvp);
    gl.uniform2f(u.offset, origin[0] || 0, origin[1] || 0);
    gl.uniform1f(u.opacity, Number(packet.opacity ?? 1));
    gl.uniform3f(u.mult, tint[0] ?? 1, tint[1] ?? 1, tint[2] ?? 1);
    gl.uniform3f(u.screen, screen[0] ?? 0, screen[1] ?? 0, screen[2] ?? 0);

    if (u.emissionStrength) {
      gl.uniform1f(u.emissionStrength, Number(packet.emissionStrength ?? 0));
    }

    if (u.albedo) gl.uniform1i(u.albedo, 0);
    if (u.emissive) gl.uniform1i(u.emissive, 1);
    if (u.bump) gl.uniform1i(u.bump, 2);

    if (stage === 0) this.applyBlendMode(Number(packet.blendingMode || 0), false);
    else this.applyBlendMode(Number(packet.blendingMode || 0), true);
  }

  renderStage(packet, advanced) {
    if (!packet || !packet.indexCount || !packet.vertexCount) return;
    if (!packet.vertexAtlasStride || !packet.uvAtlasStride || !packet.deformAtlasStride) return;

    const ibo = this.getOrCreateIboByHandle(packet.indexHandle, packet.indices, packet.indexCount);
    if (!ibo) return;

    this._bindPartSoA(packet);
    this.drawDrawableElements(ibo, packet.indexCount);

    if (advanced) {
      this.blendModeBarrier(Number(packet.blendingMode || 0));
    }
  }

  drawPartPacket(packet, texturesByHandle = this.texturesByHandle) {
    if (!packet || !packet.renderable || !packet.indexCount || !packet.vertexCount) return;

    const gl = this.gl;
    const textureCount = Math.min(Number(packet.textureCount || 0), 3);
    if (textureCount <= 0) return;

    const albedoHandle = Number(packet.textureHandles?.[0] || 0);
    const albedo = texturesByHandle.get(albedoHandle);
    if (!albedo) return;

    if (this.boundAlbedoHandle !== albedoHandle) {
      const rtTextures = this._collectColorAttachmentTextures();
      for (let i = 0; i < textureCount; i += 1) {
        const h = Number(packet.textureHandles?.[i] || 0);
        const tex = texturesByHandle.get(h);
        gl.activeTexture(gl.TEXTURE0 + i);
        if (this.activeRenderTargetHandles.has(h) || (tex?.texture && rtTextures.has(tex.texture))) {
          gl.bindTexture(gl.TEXTURE_2D, null);
        } else {
          gl.bindTexture(gl.TEXTURE_2D, tex ? tex.texture : null);
        }
      }
      this.boundAlbedoHandle = albedoHandle;
    }

    if (packet.isMask) {
      gl.useProgram(this.partMaskShader);
      const mvpMaskRowMajor = this._mulMat4RowMajor(asFloat32(packet.renderMatrix), asFloat32(packet.modelMatrix));
      const mvpMask = this._transposeMat4(mvpMaskRowMajor);
      const origin = asFloat32(packet.origin || [0, 0]);
      gl.uniformMatrix4fv(this.uPartMask.mvp, false, mvpMask);
      gl.uniform2f(this.uPartMask.offset, origin[0] || 0, origin[1] || 0);
      gl.uniform1f(this.uPartMask.threshold, Number(packet.maskThreshold ?? 0.5));
      gl.uniform1i(this.uPartMask.tex, 0);
      gl.blendEquation(gl.FUNC_ADD);
      gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
      this.renderStage(packet, false);
    } else if (packet.useMultistageBlend) {
      this.setupShaderStage(packet, 0, packet.modelMatrix, packet.renderMatrix);
      this.renderStage(packet, true);
      if (packet.hasEmissionOrBumpmap) {
        this.setupShaderStage(packet, 1, packet.modelMatrix, packet.renderMatrix);
        this.renderStage(packet, false);
      }
    } else {
      this.setupShaderStage(packet, 2, packet.modelMatrix, packet.renderMatrix);
      this.renderStage(packet, false);
    }

    this.setDrawBuffersSafe(3);
    gl.blendEquation(gl.FUNC_ADD);
  }

  executeMaskPacket(packet) {
    if (!packet || !packet.indexCount || !packet.vertexCount) return;

    const gl = this.gl;
    const ibo = this.getOrCreateIboByHandle(packet.indexHandle, packet.indices, packet.indexCount);
    if (!ibo) return;

    gl.useProgram(this.maskShader);
    gl.uniformMatrix4fv(this.uMask.mvp, false, this._transposeMat4(asFloat32(packet.mvp || packet.modelMatrix)));
    const origin = asFloat32(packet.origin || [0, 0]);
    gl.uniform2f(this.uMask.offset, origin[0] || 0, origin[1] || 0);

    this._bindMaskSoA(packet);
    this.drawDrawableElements(ibo, packet.indexCount);
  }

  createDynamicCompositePass(packet, texturesByHandle = this.texturesByHandle) {
    const pass = new DynamicCompositePass();
    const surface = new DynamicCompositeSurface();

    surface.textureCount = Math.min(Number(packet.textureCount || 0), 3);
    for (let i = 0; i < surface.textureCount; i += 1) {
      surface.textureHandles[i] = Number(packet.textures?.[i] || 0);
    }
    surface.stencilHandle = Number(packet.stencil || 0);

    const key = makeDynamicFramebufferKey(surface.textureHandles, surface.textureCount, surface.stencilHandle);
    let fbo = this.dynamicFramebufferCache.get(key);

    if (!fbo) {
      const gl = this.gl;
      const baseTex = texturesByHandle.get(surface.textureHandles[0]);
      if (baseTex) {
        fbo = gl.createFramebuffer();
        gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
        for (let i = 0; i < surface.textureCount; i += 1) {
          const h = surface.textureHandles[i];
          const tex = texturesByHandle.get(h);
          const att = gl.COLOR_ATTACHMENT0 + i;
          gl.framebufferTexture2D(gl.FRAMEBUFFER, att, gl.TEXTURE_2D, tex ? tex.texture : null, 0);
        }
        if (surface.stencilHandle) {
          const stencilTex = texturesByHandle.get(surface.stencilHandle);
          if (stencilTex && stencilTex.texture) {
            gl.framebufferTexture2D(
              gl.FRAMEBUFFER,
              gl.STENCIL_ATTACHMENT,
              gl.TEXTURE_2D,
              stencilTex.texture,
              0,
            );
            pass.hasStencil = true;
          } else {
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.STENCIL_ATTACHMENT, gl.TEXTURE_2D, null, 0);
            pass.hasStencil = false;
          }
        } else {
          gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.STENCIL_ATTACHMENT, gl.TEXTURE_2D, null, 0);
          pass.hasStencil = false;
        }

        if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) !== gl.FRAMEBUFFER_COMPLETE) {
          gl.deleteFramebuffer(fbo);
          fbo = null;
        }
      }
      if (fbo) this.dynamicFramebufferCache.set(key, fbo);
    }

    surface.framebuffer = fbo;
    pass.surface = surface;
    pass.scale = [Number(packet.scale?.[0] || 1), Number(packet.scale?.[1] || 1)];
    pass.rotationZ = Number(packet.rotationZ || 0);
    pass.origBuffer = packet.origBuffer || null;
    pass.origViewport = [
      Number(packet.origViewport?.[0] || 0),
      Number(packet.origViewport?.[1] || 0),
      Number(packet.origViewport?.[2] || 0),
      Number(packet.origViewport?.[3] || 0),
    ];
    pass.autoScaled = !!packet.autoScaled;
    pass.drawBufferCount = Number(packet.drawBufferCount || 1);
    pass.hasStencil = !!packet.hasStencil || !!surface.stencilHandle;

    return pass;
  }

  beginDynamicComposite(pass) {
    if (!pass?.surface?.framebuffer) return;

    const gl = this.gl;
    const prevBuffer = gl.getParameter(gl.DRAW_FRAMEBUFFER_BINDING) || gl.getParameter(gl.FRAMEBUFFER_BINDING);
    const prevViewport = gl.getParameter(gl.VIEWPORT);
    pass.origBuffer = prevBuffer;
    pass.origViewport = [prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]];
    const prevHandleSet = new Set(this.activeRenderTargetHandles);

    this.activeDynamicPasses.push({ prevBuffer, prevViewport, pass, prevHandleSet });
    this.activeRenderTargetHandleStack.push(prevHandleSet);
    this.activeRenderTargetHandles = new Set();
    for (let i = 0; i < pass.surface.textureCount; i += 1) {
      const h = Number(pass.surface.textureHandles[i] || 0);
      if (h) this.activeRenderTargetHandles.add(h);
    }
    this.boundAlbedoHandle = 0;

    gl.bindFramebuffer(gl.FRAMEBUFFER, pass.surface.framebuffer);

    const tex = this.texturesByHandle.get(pass.surface.textureHandles[0]);
    const w = tex?.width || gl.canvas.width;
    const h = tex?.height || gl.canvas.height;

    pass.drawBufferCount = this.setDrawBuffersSafe(pass.surface.textureCount || 1);
    this.pushViewport(w, h);
    gl.viewport(0, 0, w, h);
    gl.clearColor(0, 0, 0, 0);
    if (pass.hasStencil) {
      gl.clear(gl.STENCIL_BUFFER_BIT);
    }
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.activeTexture(gl.TEXTURE0);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
  }

  endDynamicComposite(pass) {
    const st = this.activeDynamicPasses.pop();
    if (!st) return;

    const gl = this.gl;
    this.rebindActiveTargets();
    const origBuffer = pass?.origBuffer ?? st.prevBuffer;
    const origViewport = pass?.origViewport ?? st.prevViewport;
    gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, origBuffer);
    gl.bindFramebuffer(gl.READ_FRAMEBUFFER, origBuffer);

    const vp = origViewport;
    this.popViewport();
    this.activeRenderTargetHandles = st.prevHandleSet || new Set();
    if (this.activeRenderTargetHandleStack.length) this.activeRenderTargetHandleStack.pop();
    this.boundAlbedoHandle = 0;
    if (vp && vp.length === 4) {
      gl.viewport(vp[0], vp[1], vp[2], vp[3]);
    }
  }

  framebufferHandle() {
    return this.fBuffer || null;
  }

  useShader(shader) {
    if (!shader) return;
    this.gl.useProgram(shader);
  }

  createShader(vertexSource, fragmentSource) {
    const gl = this.gl;
    if (!vertexSource || !fragmentSource) return null;
    return createProgram(gl, String(vertexSource), String(fragmentSource));
  }

  destroyShader(shader) {
    if (!shader) return;
    this.gl.deleteProgram(shader);
  }

  getShaderUniformLocation(shader, name) {
    if (!shader || !name) return -1;
    const loc = this.gl.getUniformLocation(shader, String(name));
    return loc === null ? -1 : loc;
  }

  setShaderUniform(shader, location, value) {
    const gl = this.gl;
    if (!shader || location === null || location === -1) return;
    gl.useProgram(shader);

    if (typeof value === "boolean") {
      gl.uniform1i(location, value ? 1 : 0);
      return;
    }
    if (typeof value === "number") {
      if (Number.isInteger(value)) gl.uniform1i(location, value);
      else gl.uniform1f(location, value);
      return;
    }
    if (value instanceof Float32Array || Array.isArray(value)) {
      const arr = value instanceof Float32Array ? value : new Float32Array(value);
      if (arr.length === 2) gl.uniform2fv(location, arr);
      else if (arr.length === 3) gl.uniform3fv(location, arr);
      else if (arr.length === 4) gl.uniform4fv(location, arr);
      else if (arr.length >= 16) gl.uniformMatrix4fv(location, false, this._transposeMat4(arr));
    }
  }

  setShaderUniformBool(shader, location, value) {
    this.setShaderUniform(shader, location, !!value);
  }

  setShaderUniformInt(shader, location, value) {
    this.setShaderUniform(shader, location, value | 0);
  }

  setShaderUniformFloat(shader, location, value) {
    this.setShaderUniform(shader, location, Number(value || 0));
  }

  setShaderUniformVec2(shader, location, value) {
    const v = asFloat32(value);
    this.setShaderUniform(shader, location, [v[0] || 0, v[1] || 0]);
  }

  setShaderUniformVec3(shader, location, value) {
    const v = asFloat32(value);
    this.setShaderUniform(shader, location, [v[0] || 0, v[1] || 0, v[2] || 0]);
  }

  setShaderUniformVec4(shader, location, value) {
    const v = asFloat32(value);
    this.setShaderUniform(shader, location, [v[0] || 0, v[1] || 0, v[2] || 0, v[3] || 0]);
  }

  setShaderUniformMat4(shader, location, value) {
    const v = asFloat32(value);
    this.setShaderUniform(shader, location, v.length >= 16 ? v.subarray(0, 16) : v);
  }

  beginMask(useStencil) {
    const gl = this.gl;
    this.pendingMask = true;
    this.pendingMaskUsesStencil = !!useStencil;

    gl.enable(gl.STENCIL_TEST);
    gl.clearStencil(this.pendingMaskUsesStencil ? 0 : 1);
    gl.clear(gl.STENCIL_BUFFER_BIT);
    gl.stencilMask(0xff);
    gl.stencilFunc(gl.ALWAYS, this.pendingMaskUsesStencil ? 0 : 1, 0xff);
    gl.stencilOp(gl.KEEP, gl.KEEP, gl.KEEP);
  }

  beginMaskContent() {
    const gl = this.gl;
    gl.stencilFunc(gl.EQUAL, 1, 0xff);
    gl.stencilMask(0x00);
  }

  endMask() {
    const gl = this.gl;
    this.pendingMask = false;
    this.pendingMaskUsesStencil = false;

    gl.stencilMask(0xff);
    gl.stencilFunc(gl.ALWAYS, 1, 0xff);
    gl.disable(gl.STENCIL_TEST);
  }

  applyMask(packet, texturesByHandle = this.texturesByHandle) {
    if (!packet) return;

    const gl = this.gl;
    gl.colorMask(false, false, false, false);
    gl.stencilOp(gl.KEEP, gl.KEEP, gl.REPLACE);
    gl.stencilFunc(gl.ALWAYS, packet.isDodge ? 0 : 1, 0xff);
    gl.stencilMask(0xff);

    if (Number(packet.kind) === MaskDrawableKind.Part) {
      this.drawPartPacket(packet.partPacket, texturesByHandle);
    } else {
      this.executeMaskPacket(packet.maskPacket);
    }

    gl.colorMask(true, true, true, true);
  }

  _allocTextureHandle() {
    if (!this._nextTextureHandle) this._nextTextureHandle = 1;
    const h = this._nextTextureHandle;
    this._nextTextureHandle += 1;
    return h;
  }
}

export class WebGLBackendInit {
  constructor(canvas, opts = {}) {
    const gl = canvas.getContext("webgl2", {
      alpha: true,
      antialias: true,
      depth: false,
      stencil: true,
      premultipliedAlpha: false,
    });
    if (!gl) throw new Error("webgl2 context is unavailable");

    this.canvas = canvas;
    this.gl = gl;
    this.drawableW = canvas.width | 0;
    this.drawableH = canvas.height | 0;

    this.backend = new WebGLRenderBackend(gl, opts);
    this.callbacks = this.backend.createUnityResourceCallbacks();
  }
}

export function initWebGLBackend(canvas, opts = {}) {
  return new WebGLBackendInit(canvas, opts);
}

export function renderCommands(glInit, snapshot, view) {
  if (!glInit || !view) return;

  const backend = glInit.backend;
  const gl = glInit.gl;

  backend.rebindActiveTargets();

  const v = snapshot?.vertices?.data ?? snapshot?.vertices ?? [];
  const uv = snapshot?.uvs?.data ?? snapshot?.uvs ?? [];
  const d = snapshot?.deform?.data ?? snapshot?.deform ?? [];

  backend.uploadSharedVertexBuffer(v);
  backend.uploadSharedUvBuffer(uv);
  backend.uploadSharedDeformBuffer(d);

  backend.beginScene();
  backend.bindDrawableVao();
  backend.bindPartShader();

  const cmds = view.commands || [];
  const dynPassStack = [];

  for (const cmd of cmds) {
    switch (Number(cmd.kind)) {
      case NjgRenderCommandKind.DrawPart:
        backend.drawPartPacket(cmd.partPacket);
        break;
      case NjgRenderCommandKind.BeginMask:
        backend.beginMask(!!cmd.usesStencil);
        break;
      case NjgRenderCommandKind.ApplyMask:
        backend.applyMask(cmd.maskApplyPacket);
        break;
      case NjgRenderCommandKind.BeginMaskContent:
        backend.beginMaskContent();
        break;
      case NjgRenderCommandKind.EndMask:
        backend.endMask();
        break;
      case NjgRenderCommandKind.BeginDynamicComposite: {
        const pass = backend.createDynamicCompositePass(cmd.dynamicPass);
        dynPassStack.push(pass);
        backend.beginDynamicComposite(pass);
        break;
      }
      case NjgRenderCommandKind.EndDynamicComposite: {
        const pass = dynPassStack.length
          ? dynPassStack.pop()
          : backend.createDynamicCompositePass(cmd.dynamicPass);
        backend.endDynamicComposite(pass);
        break;
      }
      default:
        break;
    }
  }

  backend.postProcessScene();
  backend.presentSceneToBackbuffer(glInit.drawableW, glInit.drawableH);

  if (backend.thumbnailGridEnabled) {
    const handles = Array.from(backend.texturesByHandle.keys());
    backend.renderThumbnailGrid(glInit.drawableW, glInit.drawableH, handles);
  }

  gl.useProgram(null);
  gl.bindVertexArray(null);
  gl.flush();

  backend.endScene();
}
