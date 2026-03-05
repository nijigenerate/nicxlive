param(
    [Parameter(Mandatory = $true)]
    [string]$UnityProjectPath,

    [ValidateSet("Debug", "RelWithDebInfo", "Release")]
    [string]$Config = "RelWithDebInfo",

    [switch]$SkipNativeBuild,

    [switch]$SkipUnitySetup,

    [string]$UnityExe = "",

    [string]$CMakeExe = "cmake"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-NativeDllPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,
        [Parameter(Mandatory = $true)]
        [string]$PreferredConfig
    )

    $candidates = @(
        (Join-Path $BuildDir "$PreferredConfig\nicxlive.dll"),
        (Join-Path $BuildDir "RelWithDebInfo\nicxlive.dll"),
        (Join-Path $BuildDir "Debug\nicxlive.dll"),
        (Join-Path $BuildDir "Release\nicxlive.dll")
    ) | Select-Object -Unique

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "nicxlive.dll not found under build directory: $BuildDir"
}

function Ensure-UnityProject {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    $assets = Join-Path $ProjectRoot "Assets"
    if (-not (Test-Path $assets)) {
        throw "Unity project path is invalid. 'Assets' directory not found: $ProjectRoot"
    }
}

function Resolve-UnityExePath {
    param(
        [Parameter(Mandatory = $false)]
        [string]$RequestedPath
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        if (Test-Path $RequestedPath) {
            return (Resolve-Path $RequestedPath).Path
        }
        throw "Unity executable not found: $RequestedPath"
    }

    $candidates = @()
    $hubEditors = Join-Path $env:ProgramFiles "Unity\Hub\Editor"
    if (Test-Path $hubEditors) {
        $candidates += Get-ChildItem -Path $hubEditors -Directory -ErrorAction SilentlyContinue |
            Sort-Object -Property Name -Descending |
            ForEach-Object { Join-Path $_.FullName "Editor\Unity.exe" }
    }

    $candidates += Join-Path $env:ProgramFiles "Unity\Editor\Unity.exe"

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

$scriptDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path
$buildDir = Join-Path $repoRoot "build"
$projectRoot = (Resolve-Path $UnityProjectPath).Path

Ensure-UnityProject -ProjectRoot $projectRoot

if (-not $SkipNativeBuild) {
    $cache = Join-Path $buildDir "CMakeCache.txt"
    if (-not (Test-Path $cache)) {
        Write-Host "[nicxlive] configuring CMake project..."
        & $CMakeExe -S $repoRoot -B $buildDir -DNICXLIVE_BUILD_SHARED=ON
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed."
        }
    }

    Write-Host "[nicxlive] building native shared library ($Config)..."
    & $CMakeExe --build $buildDir --config $Config --target nicxlive_shared
    if ($LASTEXITCODE -ne 0) {
        throw "Native build failed."
    }
}
else {
    Write-Host "[nicxlive] skipping native build (-SkipNativeBuild specified)."
}

$nativeDll = Resolve-NativeDllPath -BuildDir $buildDir -PreferredConfig $Config

$assetsDir = Join-Path $projectRoot "Assets"
$pluginsX64Dir = Join-Path $assetsDir "Plugins\x86_64"
$runtimeDir = Join-Path $assetsDir "Nicxlive\Runtime"
$editorDir = Join-Path $assetsDir "Nicxlive\Editor"

New-Item -ItemType Directory -Force -Path $pluginsX64Dir | Out-Null
New-Item -ItemType Directory -Force -Path $runtimeDir | Out-Null
New-Item -ItemType Directory -Force -Path $editorDir | Out-Null

$runtimeSource = Join-Path $scriptDir "unity_backend.cs"
$editorSource = Join-Path $scriptDir "NicxliveBehaviourEditor.cs"
$setupSource = Join-Path $scriptDir "NicxliveProjectSetup.cs"

if (-not (Test-Path $runtimeSource)) {
    throw "Managed runtime source not found: $runtimeSource"
}
if (-not (Test-Path $editorSource)) {
    throw "Managed editor source not found: $editorSource"
}
if (-not (Test-Path $setupSource)) {
    throw "Managed setup source not found: $setupSource"
}

$dllDest = Join-Path $pluginsX64Dir "nicxlive.dll"
$runtimeDest = Join-Path $runtimeDir "unity_backend.cs"
$editorDest = Join-Path $editorDir "NicxliveBehaviourEditor.cs"
$setupDest = Join-Path $editorDir "NicxliveProjectSetup.cs"

Copy-Item -Force $nativeDll $dllDest
Copy-Item -Force $runtimeSource $runtimeDest
Copy-Item -Force $editorSource $editorDest
Copy-Item -Force $setupSource $setupDest

if (-not $SkipUnitySetup) {
    $resolvedUnityExe = Resolve-UnityExePath -RequestedPath $UnityExe
    if ($null -eq $resolvedUnityExe) {
        Write-Warning "[nicxlive] Unity.exe was not found automatically. Skipping Unity settings setup."
        Write-Warning "[nicxlive] Re-run with -UnityExe <path> or run Tools/Nicxlive/Apply Project Setup in Unity."
    }
    else {
        $setupMethod = "Nicxlive.UnityBackend.EditorSetup.NicxliveProjectSetup.ApplyFromCommandLine"
        $setupLog = Join-Path $projectRoot "Logs\nicxlive-unity-setup.log"
        New-Item -ItemType Directory -Force -Path (Split-Path $setupLog) | Out-Null
        Write-Host "[nicxlive] applying Unity project settings..."
        & $resolvedUnityExe `
            -batchmode `
            -quit `
            -nographics `
            -projectPath $projectRoot `
            -executeMethod $setupMethod `
            -logFile $setupLog
        if ($LASTEXITCODE -ne 0) {
            throw "Unity setup failed. See log: $setupLog"
        }
        Write-Host "[nicxlive] Unity setup log: $setupLog"
    }
}
else {
    Write-Host "[nicxlive] skipping Unity settings setup (-SkipUnitySetup specified)."
}

Write-Host ""
Write-Host "[nicxlive] install complete"
Write-Host "  Unity project : $projectRoot"
Write-Host "  Native DLL    : $dllDest"
Write-Host "  Runtime code  : $runtimeDest"
Write-Host "  Editor code   : $editorDest"
Write-Host "  Setup code    : $setupDest"
Write-Host ""
Write-Host "Next in Unity:"
Write-Host "  1) Add NicxliveBehaviour to a GameObject"
Write-Host "  2) Set PartMaterial and MaskMaterial"
