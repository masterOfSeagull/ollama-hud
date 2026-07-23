param(
    [switch]$Run,
    [switch]$Verify,
    [switch]$Tests,
    [switch]$HotReload,
    [string]$Configuration = "Release",
    [string]$QtVersion = "6.9.2",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArgs
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ($HotReload) {
    $BuildDir = Join-Path $ProjectRoot "build\hotreload"
    if (-not $PSBoundParameters.ContainsKey("Configuration")) {
        $Configuration = "Debug"
    }
    $hotReloadValue = "ON"
} else {
    $BuildDir = Join-Path $ProjectRoot "build\native"
    $hotReloadValue = "OFF"
}

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\17\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\17\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }
    throw "CMake was not found on PATH or in Visual Studio's bundled CMake location."
}

function Find-QtPrefix {
    $envCandidates = @($env:Qt6_ROOT, $env:QT_ROOT, $env:QTDIR, $env:QT_DIR) | Where-Object { $_ }
    foreach ($path in $envCandidates) {
        if (Test-Path (Join-Path $path "lib\cmake\Qt6\Qt6Config.cmake")) { return $path }
    }

    $roots = Get-ChildItem -Path "C:\Qt" -Directory -ErrorAction SilentlyContinue
    $kits = foreach ($root in $roots) {
        Get-ChildItem -Path $root.FullName -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match "msvc.*_64" -and (Test-Path (Join-Path $_.FullName "lib\cmake\Qt6\Qt6Config.cmake")) }
    }
    $preferred = $kits | Sort-Object FullName -Descending | Select-Object -First 1
    if ($preferred) { return $preferred.FullName }
    return $null
}

function Install-Qt {
    param([string]$Version)

    Write-Host "Qt 6 msvc2022_64 was not found under C:\Qt. Installing Qt $Version with aqtinstall..."
    python -m pip install --upgrade aqtinstall
    $aqtArgs = @("install-qt", "windows", "desktop", $Version, "win64_msvc2022_64", "-O", "C:\Qt", "-m", "qt5compat")
    $process = Start-Process -FilePath "python" -ArgumentList (@("-m", "aqt") + $aqtArgs) -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) {
        Write-Host "Exact Qt $Version install failed; trying latest available Qt 6 desktop msvc2022_64."
        $fallback = Start-Process -FilePath "python" -ArgumentList @("-m", "aqt", "install-qt", "windows", "desktop", "6.9.3", "win64_msvc2022_64", "-O", "C:\Qt", "-m", "qt5compat") -Wait -PassThru -NoNewWindow
        if ($fallback.ExitCode -ne 0) {
            throw "aqtinstall could not install Qt. Install Qt 6.9+ msvc2022_64 manually or set QTDIR."
        }
    }
}

function Find-VsDevCmd {
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\17\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\17\Professional\Common7\Tools\VsDevCmd.bat"
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }
    return $null
}

$cmake = Find-CMake
$qtPrefix = Find-QtPrefix
if (-not $qtPrefix) {
    Install-Qt -Version $QtVersion
    $qtPrefix = Find-QtPrefix
}
if (-not $qtPrefix) { throw "Qt installation was not found after install attempt." }

New-Item -ItemType Directory -Force $BuildDir | Out-Null

$configureArgs = @(
    "-S", $ProjectRoot,
    "-B", $BuildDir,
    "-G", "Visual Studio 18 2026",
    "-A", "x64",
    "-DCMAKE_PREFIX_PATH=$qtPrefix",
    "-DQt6_DIR=$(Join-Path $qtPrefix 'lib\cmake\Qt6')",
    "-DOLLAMA_HUD_BUILD_TESTS=ON",
    "-DOLLAMA_HUD_HOT_RELOAD=$hotReloadValue"
)

try {
    & $cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for Visual Studio 18 2026." }
} catch {
    $configureArgs[5] = "Visual Studio 17 2022"
    & $cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for Visual Studio 17 2022." }
}

& $cmake --build $BuildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Native build failed." }

$exe = Join-Path $BuildDir "$Configuration\OllamaHud.exe"
if (-not (Test-Path $exe)) {
    $exe = Join-Path $BuildDir "OllamaHud.exe"
}
if (-not (Test-Path $exe)) {
    throw "Build succeeded but OllamaHud.exe was not found in $BuildDir."
}

$buildRuntimeDir = Split-Path -Parent $exe
$env:PATH = "$(Join-Path $qtPrefix 'bin');$buildRuntimeDir;$env:PATH"

if ($Tests) {
    & $cmake --build $BuildDir --config $Configuration --target ollama_hud_native_tests --parallel
    if ($LASTEXITCODE -ne 0) { throw "Native test build failed." }
    $testExe = Join-Path $BuildDir "$Configuration\ollama_hud_native_tests.exe"
    if (-not (Test-Path $testExe)) {
        $testExe = Join-Path $BuildDir "ollama_hud_native_tests.exe"
    }
    if (-not (Test-Path $testExe)) {
        throw "Native test executable was not found."
    }
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw "Native tests failed." }
}

if ($Verify) {
    & $exe --verify
}

if ($Run) {
    $argsToForward = @()
    if ($RemainingArgs) {
        $argsToForward = $RemainingArgs | Where-Object { $_ -ne "--" }
    }
    & $exe @argsToForward
}
