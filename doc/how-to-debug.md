# nijiv-based parity debug workflow (nijilive vs nicxlive)

## Goal
- Compare `nijilive` DLL and `nicxlive` DLL under exactly the same `nijiv` input and runtime settings.
- Drive fixes by: `observe -> confirm D-side behavior -> copy-based fix`.

## Non-negotiable rules
- Run commands strictly in sequence. No parallel build/run.
- Never run verification before build finishes.
- Change one hypothesis at a time.
- If a change causes crash/startup failure, manually revert that change immediately (no git rollback).

## Paths used in this repo
- Workspace: `C:\Users\siget\src\nijigenerate`
- Test model: `C:\Users\siget\OneDrive\Inochi2d\Aka-0.9.3.inx`
- Host app: `C:\Users\siget\src\nijigenerate\nijiv`

## 0) Shell / environment
- Use `VS2022 Developer PowerShell`.
- If `dub` is not on PATH, use `C:\opt\ldc-1.41\bin\dub.exe` through `VsDevCmd`.

## 1) Build phase (serial)
1. Build `nicxlive`.
```powershell
cd C:\Users\siget\src\nijigenerate\nicxlive
cmd /c _build_nicx.cmd
```
2. Build `nijiv` OpenGL target.
```powershell
cd C:\Users\siget\src\nijigenerate\nijiv
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && set PATH=C:\opt\ldc-1.41\bin;%PATH% && dub build -c opengl'
```
3. Do **not** copy `nicxlive.dll` to `nijiv`.
   - `nijiv` resolves `nicxlive` directly from `nicxlive/build/*`.
   - Windows: `Debug -> RelWithDebInfo -> Release -> build root`.
   - macOS/Linux: `build root -> Debug -> RelWithDebInfo -> Release`.
   - Keep a single source of truth: the just-built artifact.

## 2) Run phase (same args, two flavors)
1. Run with `nijilive`.
```powershell
cd C:\Users\siget\src\nijigenerate\nijiv
.\nijiv-opengl.exe --test "C:\Users\siget\OneDrive\Inochi2d\Aka-0.9.3.inx" --frames 120 --fixed-delta 0.0166667 --unity-dll nijilive --queue-dump "queue-nijilive-f120.log" *> "run-niji-f120.txt"
```
2. Run with `nicxlive`.
```powershell
.\nijiv-opengl.exe --test "C:\Users\siget\OneDrive\Inochi2d\Aka-0.9.3.inx" --frames 120 --fixed-delta 0.0166667 --unity-dll nicxlive --queue-dump "queue-nicxlive-f120.log" *> "run-nicx-f120.txt"
```
3. Verify the actually loaded library path in logs.
```powershell
rg -n "^\[unity\] flavor=nicxlive path=" run-nicx-f120.txt
```

## 3) Success criteria for each run
- Do not trust exit code alone.
- Treat run as valid when `run-*.txt` contains:
  - `Exit after ... frames (test)`
  - `emit out count=...`

## 4) First comparison checks
1. Per-frame hashes (`v/u/d`), especially deform hash `d`.
```powershell
rg -n "^FRAME |^HASH " queue-nijilive-f120.log
rg -n "^FRAME |^HASH " queue-nicxlive-f120.log
```
2. Broken texture handle probe (`th0=0`).
```powershell
rg -n "th0=0" queue-nicxlive-f120.log
```
3. Blend-sensitive commands (`bm=17` ClipToLower, `bm=18` SliceFromLower).
```powershell
rg -n "bm=17|bm=18" queue-nijilive-f120.log
rg -n "bm=17|bm=18" queue-nicxlive-f120.log
```
4. Dynamic composite ordering and pass handles.
```powershell
rg -n "BeginDynamicComposite|EndDynamicComposite" queue-nijilive-f120.log
rg -n "BeginDynamicComposite|EndDynamicComposite" queue-nicxlive-f120.log
```

## 5) Typical fault mapping
- `th0=0` appears:
  - inspect `core/unity_native.cpp` texture-handle resolution order
  - inspect `runtimeTextureHandles` vs `backendTextureHandles` mapping
- `nicxlive` deform hash `d` stays constant across frames:
  - inspect deform update path (`Deformable/Drawable -> shared_deform_buffer -> queue pack`)
  - verify `deformOffset` and `deformAtlasStride` correctness
- `bm=17` lines match but output is black:
  - prioritize texture/deform/dynamic-pass analysis before blend-mode analysis

## 6) Minimal re-test loop after each fix
- Fast structural check first (1 frame), then time-drift check (120 frames).
```powershell
.\nijiv-opengl.exe --test "C:\Users\siget\OneDrive\Inochi2d\Aka-0.9.3.inx" --frames 1 --fixed-delta 0.0166667 --unity-dll nicxlive --queue-dump "queue-nicxlive-f1.log" *> "run-nicx-f1.txt"
.\nijiv-opengl.exe --test "C:\Users\siget\OneDrive\Inochi2d\Aka-0.9.3.inx" --frames 120 --fixed-delta 0.0166667 --unity-dll nicxlive --queue-dump "queue-nicxlive-f120.log" *> "run-nicx-f120.txt"
```

## 7) Handoff template for next turn
- Changed files:
  - `...`
- D-source references used for copy-based fix:
  - `...`
- 1-frame result summary:
  - `...`
- 120-frame result summary:
  - `...`
- Remaining diffs in priority order:
  - `...`
