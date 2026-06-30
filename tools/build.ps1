param([ValidateSet("Release","Debug")][string]$Config = "Release")

$ErrorActionPreference = "Stop"
$Root  = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "build"

if (-not (Test-Path (Join-Path $Root "third_party/cef/cmake/cef_variables.cmake"))) {
  throw "CEF missing. Run ./tools/fetch_cef.ps1 first."
}
if (-not (Test-Path (Join-Path $Root "third_party/json/include/nlohmann/json.hpp"))) {
  throw "json.hpp missing. Run ./tools/fetch_deps.ps1 first."
}

New-Item -ItemType Directory -Force -Path $Build | Out-Null

# Use cmd to initialize vcvars64 environment and then run cmake with Ninja
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$cmakeExe = "C:\Program Files\CMake\bin\cmake.exe"

if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }
if (-not (Test-Path $cmakeExe)) { throw "cmake.exe not found at $cmakeExe" }

$buildCmd = @"
call "$vcvars" && ^
"$cmakeExe" -G Ninja -DCMAKE_BUILD_TYPE=$Config -S "$Root" -B "$Build" && ^
"$cmakeExe" --build "$Build" --config $Config
"@

cmd /c $buildCmd
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
Write-Host "Build complete: $Build"
