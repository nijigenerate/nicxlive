import re
from pathlib import Path
from dataclasses import dataclass
from typing import Dict, List, Tuple, Optional

ROOT = Path(r"c:/Users/siget/src/nijigenerate")
SRC_GL = ROOT / "nijiv/source/opengl/opengl_backend.d"
SRC_DX = ROOT / "nijiv/source/directx/directx_backend.d"
SRC_UNITY = ROOT / "nicxlive/unity_backend/unity_backend.cs"
OUT = ROOT / "nicxlive/doc/compat-unity_backend.md"

SKIP_METHOD_NAMES = {
    "version", "this", "main", "const", "switch", "register", "if", "for", "while", "foreach", "do", "catch"
}

CALL_KEYWORDS = {
    "if", "for", "while", "switch", "catch", "lock", "using", "return", "sizeof", "typeof", "nameof",
    "checked", "unchecked", "fixed", "foreach", "do", "new", "base", "this", "cast", "string", "const", "version", "main",
    "assert", "enforce"
}

API_PREFIXES = (
    "gl", "sdl", "dx", "d3d", "wind3d", "id3d", "idxgi", "queryinterface", "release", "getbuffer",
    "createcommittedresource", "createswapchain", "createdescriptorheap", "resourcebarrier", "map", "unmap"
)

UTILITY_CALLS = {
    "max", "min", "clamp", "roundtoint", "approximately", "isnan", "reserve",
    "trygetvalue", "tryget", "containskey", "add", "remove", "removewhere", "clear", "clone", "copyto", "copy",
    "propertytoid",
    "getcurrentbackbufferindex", "signal", "getcompletedvalue", "seteventoncompletion", "waitforsingleobject",
    "setgraphicsrootsignature", "setdescriptorheaps", "setgraphicsrootdescriptortable", "iasetprimitivetopology",
    "iasetvertexbuffers", "iasetindexbuffer", "rssetviewports", "rssetscissorrects", "omsetrendertargets",
    "drawindexedinstanced", "present", "close", "executecommandlists", "reset", "scope", "addrOfpinnedobject".lower(),
}

NORMALIZE_MAP = {
    # backend lifecycle
    "ensurerenderbackend": "backend.ensure",
    "tryrenderbackend": "backend.try",
    "requirerenderbackend": "backend.require",
    "currentrenderbackend": "backend.current",
    # shader
    "createshader": "shader.create",
    "destroyshader": "shader.destroy",
    "useshader": "shader.use",
    "getshaderuniformlocation": "shader.uniform_loc",
    "setshaderuniform": "shader.uniform_set",
    "getuniformlocation": "shader.uniform_loc",
    "setuniform": "shader.uniform_set",
    "setfloat": "shader.uniform_set",
    "setint": "shader.uniform_set",
    "setvector": "shader.uniform_set",
    "setmatrix": "shader.uniform_set",
    "setpass": "shader.use",
    "use": "shader.use",
    "bindpartshader": "shader.use",
    "shaderasset": "shader.asset",
    "fromopenglsource": "shader.asset",
    "sampletex": "shader.sample",
    "vsmain": "shader.vs",
    "trygetrendertexture": "tex.id",
    # texture
    "createtexturehandle": "tex.create",
    "destroytexturehandle": "tex.destroy",
    "bindtexturehandle": "tex.bind",
    "uploadtexturedata": "tex.upload",
    "generatetexturemipmap": "tex.mipmap",
    "applytexturefiltering": "tex.filter",
    "applytexturewrapping": "tex.wrap",
    "applytextureanisotropy": "tex.aniso",
    "readtexturedata": "tex.read",
    "texturenativehandle": "tex.native",
    "textureid": "tex.id",
    "gettextureid": "tex.id",
    "gettexturedata": "tex.read",
    "setfiltering": "tex.filter",
    "setwrapping": "tex.wrap",
    "setanisotropy": "tex.aniso",
    "setdata": "tex.upload",
    "genmipmap": "tex.mipmap",
    "bind": "tex.bind",
    # scene/render pipeline
    "initializerenderer": "render.init",
    "initializepartbackendresources": "render.init_part",
    "initializemaskbackend": "ensuremaskbackendinitialized",
    "ensuremaskbackendinitialized": "ensuremaskbackendinitialized",
    "beginscene": "scene.begin",
    "endscene": "scene.end",
    "postprocessscene": "scene.post",
    "presentscenetobackbuffer": "scene.present",
    "rebindactivetargets": "scene.rebind",
    "setviewport": "viewport.set",
    "pushviewport": "viewport.push",
    "popviewport": "viewport.pop",
    "getviewport": "viewport.get",
    "resizeviewporttargets": "viewport.resize",
    "rendercommands": "render.exec",
    "renderscene": "render.exec",
    "executequeuedcommands": "render.exec",
    "decode": "render.exec",
    # draw/mask/composite
    "drawpartpacket": "draw.part",
    "executemaskpacket": "draw.mask_exec",
    "drawmaskpacket": "draw.mask_exec",
    "beginmask": "mask.begin",
    "applymask": "mask.apply",
    "beginmaskcontent": "mask.content_begin",
    "endmask": "mask.end",
    "begindynamiccomposite": "comp.begin",
    "enddynamiccomposite": "comp.end",
    "createdynamiccompositepass": "comp.create_pass",
    "renderstage": "draw.stage",
    "drawpart": "draw.elements",
    "sharedvertexbufferhandle": "draw.elements",
    "shareduvbufferhandle": "draw.elements",
    "shareddeformbufferhandle": "draw.elements",
    "setupshaderstage": "draw.stage_setup",
    "addifpresent": "draw.stage_setup",
    "setdrawbufferssafe": "draw.stage_setup",
    "drawdrawableelements": "draw.elements",
    "getorcreateibo": "draw.ibo_get",
    "getorcreateibobyhandle": "draw.ibo_get",
    "createdrawablebuffers": "draw.buffers_create",
    "uploaddrawableindices": "draw.indices_upload",
    "drawuploadedgeometry": "draw.uploaded",
    "uploadgeometry": "upload.geometry",
    "uploaddata": "upload.data",
    "ensuretextureuploaded": "upload.ensure",
    "ensureuploadbuffer": "upload.ensure",
    # blending
    "applyblendingcapabilities": "blend.cap_apply",
    "supportsadvancedblend": "blend.adv_supported",
    "supportsadvancedblendcoherent": "blend.coherent_supported",
    "setadvancedblendcoherent": "blend.coherent_set",
    "setlegacyblendmode": "blend.legacy_set",
    "setadvancedblendequation": "blend.adv_set",
    "applyblendmode": "blend.apply",
    "blendmodebarrier": "blend.barrier",
    "issueblendbarrier": "blend.barrier_issue",
    "buildblenddesc": "blend.desc",
    # dx runtime
    "beginframe": "frame.begin",
    "endframe": "frame.end",
    "shutdown": "frame.shutdown",
    "dispose": "frame.dispose",
    "shutdowndirectxbackend": "frame.dispose",
    "waitforgpu": "gpu.wait",
    "initialize": "device.init",
    "initopenglbackend": "backend.init",
    "initdirectxbackend": "backend.init",
    "dxtrace": "diag.trace",
    "checkshader": "diag.check",
    "checkprogram": "diag.check",
    "configuremacopenglsurfaceopacity": "platform.init",
    "sdlerror": "platform.error",
    "querywindowpixelsize": "window.query",
    "requirewindowhandle": "window.handle",
    "glviewport": "viewport.set",
    "glbindframebuffer": "scene.rebind",
    "createdepthstenciltarget": "rt.setup",
    "createrendertargets": "rt.setup",
    "createswapchainandtargets": "rt.setup",
    "ensureoffscreendepthstenciltarget": "rt.setup",
    "resizeswapchain": "rt.setup",
    "createsrvresources": "srv.setup",
    "descriptorheapcpustart": "srv.setup",
    "descriptorheapgpustart": "srv.setup",
    "updateheapsrvtexture2d": "srv.setup",
    "defaultcompositestate": "state.default",
    "createdescriptorheap": "srv.setup",
    "recoverfromdeviceloss": "device.recover",
    "releasedxresource": "device.release",
    "releaserendertargets": "rt.release",
    "createpipelinestate": "pipeline.setup",
    "ensurepresentprogram": "pipeline.setup",
}

CORE_METHODS = {
    "beginscene", "endscene", "postprocessscene", "presentscenetobackbuffer", "rebindactivetargets",
    "drawpartpacket", "executemaskpacket", "setupshaderstage", "renderstage",
    "beginmask", "applymask", "beginmaskcontent", "endmask",
    "begindynamiccomposite", "enddynamiccomposite",
    "applyblendmode", "setlegacyblendmode", "setadvancedblendequation",
    "blendmodebarrier", "issueblendbarrier", "applyblendingcapabilities",
}

BENIGN_EXTRA_TOKENS = {
    "diag.trace", "platform.init", "platform.error", "window.query", "window.handle",
    "ensureruntimeready", "ensurepipelineobjects", "ensurepiplineobjects",
    "render.init_mask", "render.exec"
}

URP_REQUIRED_PATTERNS = [
    ("URP shader is used", r"Universal Render Pipeline/Unlit"),
    ("SRP camera hook is used", r"RenderPipelineManager\.beginCameraRendering"),
    ("URP command buffer router exists", r"class\s+UrpCommandBufferRouter\b"),
    ("URP router attach is used", r"UrpCommandBufferRouter\.Attach\s*\("),
]

URP_FORBIDDEN_PATTERNS = [
    ("Built-in CameraEvent usage", r"\bCameraEvent\b"),
    ("Built-in AddCommandBuffer usage", r"\.AddCommandBuffer\s*\("),
    ("Built-in RemoveCommandBuffer usage", r"\.RemoveCommandBuffer\s*\("),
    ("Built-in Unlit/Texture shader usage", r"\"Unlit/Texture\""),
    ("Built-in Unlit/Color shader usage", r"\"Unlit/Color\""),
    ("Built-in CameraTarget usage", r"BuiltinRenderTextureType\.CameraTarget"),
]

@dataclass
class Method:
    name: str
    line: int
    calls: List[str]

@dataclass
class TypeDef:
    kind: str
    name: str
    line: int


def extract_types(text: str, lang: str) -> List[TypeDef]:
    out: List[TypeDef] = []
    if lang == "d":
        pat = re.compile(r"^\s*(class|struct|interface)\s+(\w+)\b", re.M)
    else:
        pat = re.compile(r"^\s*(?:public|internal|private|protected)?\s*(?:sealed|static|partial|abstract)?\s*(class|struct|interface)\s+(\w+)\b", re.M)
    for m in pat.finditer(text):
        line = text.count("\n", 0, m.start()) + 1
        out.append(TypeDef(m.group(1), m.group(2), line))
    return out


def line_of_index(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


def _extract_block(text: str, open_idx: int) -> str:
    depth = 0
    i = open_idx
    while i < len(text):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[open_idx:i+1]
        i += 1
    return text[open_idx:]


def extract_calls(body: str) -> List[str]:
    calls: List[str] = []
    for m in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", body):
        name = m.group(1)
        if name.lower() in CALL_KEYWORDS:
            continue
        calls.append(name)
    return calls


def extract_methods(path: Path, lang: str) -> List[Method]:
    text = path.read_text(encoding="utf-8", errors="ignore")
    methods: List[Method] = []

    if lang == "d":
        pat = re.compile(r"(?m)^\s*(?:[\w\[\]<>!.:*&,]+\s+)+(?P<name>\w+)\s*\([^;{}]*\)\s*(?:if\s*\([^)]*\)\s*)?\{")
        for m in pat.finditer(text):
            name = m.group("name")
            if name in SKIP_METHOD_NAMES:
                continue
            line = text.count("\n", 0, m.start()) + 1
            open_idx = text.find("{", m.end() - 1)
            body = _extract_block(text, open_idx) if open_idx >= 0 else ""
            methods.append(Method(name=name, line=line, calls=extract_calls(body)))
        return methods

    pat_block = re.compile(r"(?m)^\s*(?:public|private|internal|protected)\s+(?:static\s+)?(?:unsafe\s+)?(?:[\w<>\[\],\.\?]+\s+)+(?P<name>\w+)\s*\([^;{}]*\)\s*\{")
    for m in pat_block.finditer(text):
        name = m.group("name")
        if name in SKIP_METHOD_NAMES:
            continue
        line = text.count("\n", 0, m.start()) + 1
        open_idx = text.find("{", m.end() - 1)
        body = _extract_block(text, open_idx) if open_idx >= 0 else ""
        methods.append(Method(name=name, line=line, calls=extract_calls(body)))

    pat_expr = re.compile(r"(?m)^\s*(?:public|private|internal|protected)\s+(?:static\s+)?(?:unsafe\s+)?(?:[\w<>\[\],\.\?]+\s+)+(?P<name>\w+)\s*\([^;{}]*\)\s*=>\s*(?P<expr>[^;]*);")
    for m in pat_expr.finditer(text):
        name = m.group("name")
        if name in SKIP_METHOD_NAMES:
            continue
        line = text.count("\n", 0, m.start()) + 1
        methods.append(Method(name=name, line=line, calls=extract_calls(m.group("expr"))))

    return methods


def norm_call(name: str) -> Optional[str]:
    low = name.lower()
    if low in SKIP_METHOD_NAMES:
        return None
    if low in NORMALIZE_MAP:
        return NORMALIZE_MAP[low]
    for p in API_PREFIXES:
        if low.startswith(p):
            return None
    if low in UTILITY_CALLS:
        return None
    return low


def normalize_flow(method: Method) -> List[str]:
    seq: List[str] = []
    low_name = method.name.lower()
    head = NORMALIZE_MAP.get(low_name, low_name)
    if head not in SKIP_METHOD_NAMES:
        seq.append(head)
    for c in method.calls:
        n = norm_call(c)
        if n is None:
            continue
        seq.append(n)
    # keep first occurrence only (logical phase sequence)
    out: List[str] = []
    seen: set[str] = set()
    for x in seq:
        if x in seen:
            continue
        seen.add(x)
        out.append(x)
    return out


def coarsen_non_core(name: str, flow: List[str]) -> List[str]:
    if not flow:
        return flow
    if name.lower() in CORE_METHODS:
        return flow
    head = flow[0]
    kept = [head]
    if "." in head:
        prefix = head.split(".", 1)[0] + "."
        for t in flow[1:]:
            if t.startswith(prefix) or t in {"backend.current", "backend.require", "backend.ensure", "backend.try", "render.exec", "device.init"}:
                if kept[-1] != t:
                    kept.append(t)
    return kept


def lcs(a: List[str], b: List[str]) -> int:
    if not a or not b:
        return 0
    dp = [0] * (len(b) + 1)
    for i in range(1, len(a) + 1):
        prev = 0
        for j in range(1, len(b) + 1):
            cur = dp[j]
            if a[i - 1] == b[j - 1]:
                dp[j] = prev + 1
            else:
                dp[j] = max(dp[j], dp[j - 1])
            prev = cur
    return dp[-1]


def flow_str(flow: List[str]) -> str:
    return " -> ".join(flow[:12]) if flow else "-"


def canonical_for_method(name: str, gl_m: Optional[Method], dx_m: Optional[Method]) -> List[str]:
    gl_f = normalize_flow(gl_m) if gl_m else []
    dx_f = normalize_flow(dx_m) if dx_m else []
    if gl_f and dx_f:
        # canonical = LCS sequence by greedy backtrack over DP table
        a, b = gl_f, dx_f
        n, m = len(a), len(b)
        dp = [[0]*(m+1) for _ in range(n+1)]
        for i in range(1, n+1):
            for j in range(1, m+1):
                if a[i-1] == b[j-1]:
                    dp[i][j] = dp[i-1][j-1] + 1
                else:
                    dp[i][j] = max(dp[i-1][j], dp[i][j-1])
        i, j = n, m
        seq: List[str] = []
        while i > 0 and j > 0:
            if a[i-1] == b[j-1]:
                seq.append(a[i-1]); i -= 1; j -= 1
            elif dp[i-1][j] >= dp[i][j-1]:
                i -= 1
            else:
                j -= 1
        seq.reverse()
        if seq:
            return coarsen_non_core(name, seq)
        return coarsen_non_core(name, gl_f if len(gl_f) >= len(dx_f) else dx_f)
    return coarsen_non_core(name, gl_f or dx_f)


def best_method(name: str, methods_by_name: Dict[str, List[Method]]) -> Optional[Method]:
    lst = methods_by_name.get(name, [])
    if not lst:
        return None
    return lst[0]


def significant_extras(base: List[str], target: List[str]) -> List[str]:
    base_set = set(base)
    out: List[str] = []
    for t in target:
        if t in base_set:
            continue
        if t in BENIGN_EXTRA_TOKENS:
            continue
        out.append(t)
    return out


def effective_precision_flow(canonical: List[str], source_union: List[str], unity_flow: List[str]) -> List[str]:
    can = set(canonical)
    union = set(source_union)
    out: List[str] = []
    for t in unity_flow:
        if t in can:
            out.append(t)
            continue
        if t in BENIGN_EXTRA_TOKENS:
            continue
        # If a step exists in either source backend flow, do not penalize it as Unity-side extra.
        if t in union:
            continue
        out.append(t)
    return out


def render_class_table(source_label: str, src_types: List[TypeDef], unity_types: List[TypeDef]) -> List[str]:
    unity_by_name = {t.name: t for t in unity_types}
    lines = [f"### クラス比較 ({source_label})", "| Source Class | Source Line | Unity Match | 判定 | 根拠 |", "|---|---:|---|---|---|"]
    for t in [x for x in src_types if x.kind in {"class", "struct"}]:
        u = unity_by_name.get(t.name)
        if u:
            lines.append(f"| {t.name} | {t.line} | {u.line} | OK | 同名型あり |")
        else:
            lines.append(f"| {t.name} | {t.line} | - | NG | 同名型なし |")
    return lines


def render_if_table(source_label: str, src_types: List[TypeDef], unity_types: List[TypeDef]) -> List[str]:
    unity_by_name = {t.name: t for t in unity_types}
    lines = [f"### インターフェース比較 ({source_label})", "| Source Interface | Source Line | Unity Match | 判定 | 根拠 |", "|---|---:|---|---|---|"]
    src_if = [x for x in src_types if x.kind == "interface"]
    if not src_if:
        lines.append("| (none) | - | - | - | source側に定義なし |")
        return lines
    for t in src_if:
        u = unity_by_name.get(t.name)
        if u and u.kind == "interface":
            lines.append(f"| {t.name} | {t.line} | {u.line} | OK | 同名インターフェースあり |")
        else:
            lines.append(f"| {t.name} | {t.line} | - | NG | 同名インターフェースなし |")
    return lines


def build_method_comparison(gl_methods: List[Method], dx_methods: List[Method], unity_methods: List[Method]) -> Tuple[List[str], Dict[str, int]]:
    gl_by_name: Dict[str, List[Method]] = {}
    dx_by_name: Dict[str, List[Method]] = {}
    un_by_name: Dict[str, List[Method]] = {}
    for m in gl_methods: gl_by_name.setdefault(m.name, []).append(m)
    for m in dx_methods: dx_by_name.setdefault(m.name, []).append(m)
    for m in unity_methods: un_by_name.setdefault(m.name, []).append(m)

    all_names = sorted(set(gl_by_name.keys()) | set(dx_by_name.keys()))
    lines = ["### メソッド比較（論理フロー）",
             "| Method | GL Line | DX Line | Unity Line | 判定 | Recall | Precision | Canonical Flow | Unity Flow | 根拠 |",
             "|---|---:|---:|---:|---|---:|---:|---|---|---|"]

    summary = {"OK": 0, "内容NG": 0, "NG": 0}

    for name in all_names:
        if name in SKIP_METHOD_NAMES:
            continue
        gl_m = best_method(name, gl_by_name)
        dx_m = best_method(name, dx_by_name)
        un_lst = un_by_name.get(name, [])

        gl_flow = coarsen_non_core(name, normalize_flow(gl_m)) if gl_m else []
        dx_flow = coarsen_non_core(name, normalize_flow(dx_m)) if dx_m else []
        canonical = canonical_for_method(name, gl_m, dx_m)
        source_union = list(dict.fromkeys(gl_flow + dx_flow))
        gl_line = gl_m.line if gl_m else "-"
        dx_line = dx_m.line if dx_m else "-"

        if not un_lst:
            lines.append(f"| {name} | {gl_line} | {dx_line} | - | NG | 0.00 | {flow_str(canonical)} | - | 同名メソッドなし |")
            summary["NG"] += 1
            continue

        best_u = None
        best_recall = -1.0
        best_precision = -1.0
        best_score = -1.0
        best_extra = []
        for u in un_lst:
            u_flow = coarsen_non_core(name, normalize_flow(u))
            l = lcs(canonical, u_flow)
            recall = l / max(1, len(canonical))
            p_flow = effective_precision_flow(canonical, source_union, u_flow)
            lp = lcs(canonical, p_flow)
            precision = lp / max(1, len(p_flow))
            score = min(recall, precision)
            extra = [t for t in significant_extras(canonical, u_flow) if t not in set(source_union)]
            if score > best_score:
                best_score = score
                best_recall = recall
                best_precision = precision
                best_u = u
                best_extra = extra

        assert best_u is not None
        u_flow = coarsen_non_core(name, normalize_flow(best_u))
        p_flow_best = effective_precision_flow(canonical, source_union, u_flow)
        if best_recall >= 0.90 and best_precision >= 0.90 and len(best_extra) == 0:
            verdict = "OK"
        elif best_recall >= 0.55 and best_precision >= 0.55:
            verdict = "内容NG"
        else:
            verdict = "NG"
        summary[verdict] += 1

        extra_note = "" if not best_extra else f" / extra={','.join(best_extra[:4])}"
        lines.append(
            f"| {name} | {gl_line} | {dx_line} | {best_u.line} | {verdict} | {best_recall:.2f} | {best_precision:.2f} | {flow_str(canonical)} | {flow_str(u_flow)} | recall={best_recall:.2f}, precision={best_precision:.2f}, pflow={flow_str(p_flow_best)}{extra_note} |"
        )

    return lines, summary


def build_urp_check_table(unity_text: str) -> Tuple[List[str], Dict[str, int]]:
    lines = [
        "### URP互換チェック",
        "| Check | 判定 | 根拠 |",
        "|---|---|---|",
    ]
    summary = {"OK": 0, "内容NG": 0, "NG": 0}

    for title, pattern in URP_REQUIRED_PATTERNS:
        m = re.search(pattern, unity_text)
        if m:
            line = line_of_index(unity_text, m.start())
            lines.append(f"| {title} | OK | 必須パターン検出: line {line} |")
            summary["OK"] += 1
        else:
            lines.append(f"| {title} | NG | 必須パターン未検出 |")
            summary["NG"] += 1

    for title, pattern in URP_FORBIDDEN_PATTERNS:
        m = re.search(pattern, unity_text)
        if m:
            line = line_of_index(unity_text, m.start())
            lines.append(f"| {title} | NG | 禁止パターン検出: line {line} |")
            summary["NG"] += 1
        else:
            lines.append(f"| {title} | OK | 禁止パターンなし |")
            summary["OK"] += 1

    return lines, summary


def main() -> None:
    gl_text = SRC_GL.read_text(encoding="utf-8", errors="ignore")
    dx_text = SRC_DX.read_text(encoding="utf-8", errors="ignore")
    un_text = SRC_UNITY.read_text(encoding="utf-8", errors="ignore")

    gl_types = extract_types(gl_text, "d")
    dx_types = extract_types(dx_text, "d")
    un_types = extract_types(un_text, "cs")

    gl_methods = extract_methods(SRC_GL, "d")
    dx_methods = extract_methods(SRC_DX, "d")
    un_methods = extract_methods(SRC_UNITY, "cs")

    method_lines, summary = build_method_comparison(gl_methods, dx_methods, un_methods)
    urp_lines, urp_summary = build_urp_check_table(un_text)
    for k, v in urp_summary.items():
        summary[k] += v

    out: List[str] = []
    out.append("# Unity Backend 互換性評価 (論理フロー比較)")
    out.append("")
    out.append("比較対象:")
    out.append("- OpenGL基準: `nijiv/source/opengl/opengl_backend.d`")
    out.append("- DirectX基準: `nijiv/source/directx/directx_backend.d`")
    out.append("- Unity実装: `nicxlive/unity_backend/unity_backend.cs`")
    out.append("")
    out.append("判定基準:")
    out.append("- `OK`: `recall >= 0.90` かつ `precision >= 0.90` かつ 重要な余剰ステップなし")
    out.append("- `内容NG`: 同名だが `recall/precision` のどちらかが不足")
    out.append("- `NG`: 同名なし or `recall/precision` が大きく不足")
    out.append("")
    out.append("canonicalフローは OpenGL/DX の共通部分(LCS)で構築。`gl*`, `SDL*`, `D3D*` などAPI固有呼び出しは正規化して比較。")
    out.append("")

    out.extend(render_class_table("OpenGL", gl_types, un_types)); out.append("")
    out.extend(render_class_table("DirectX", dx_types, un_types)); out.append("")
    out.extend(render_if_table("OpenGL", gl_types, un_types)); out.append("")
    out.extend(render_if_table("DirectX", dx_types, un_types)); out.append("")
    out.extend(method_lines); out.append("")
    out.extend(urp_lines); out.append("")
    out.append("## サマリー")
    out.append(f"- OK: {summary['OK']}")
    out.append(f"- 内容NG: {summary['内容NG']}")
    out.append(f"- NG: {summary['NG']}")

    OUT.write_text("\n".join(out), encoding="utf-8")
    print(f"wrote: {OUT}")
    print(f"summary OK={summary['OK']} content_ng={summary['内容NG']} NG={summary['NG']}")


if __name__ == "__main__":
    main()
