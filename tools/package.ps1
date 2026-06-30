param([ValidateSet("Release","Debug")][string]$Config = "Release")

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Bin  = Join-Path $Root "build"
$Dist = Join-Path $Root "dist/NyxBrowser"

if (-not (Test-Path (Join-Path $Bin "nyx.exe"))) {
  throw "nyx.exe not found in $Bin. Run ./tools/build.ps1 first."
}

if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
New-Item -ItemType Directory -Force -Path $Dist | Out-Null

# Copy the full build output (nyx.exe + CEF runtime that the build step staged).
Copy-Item (Join-Path $Bin "*") $Dist -Recurse -Force

# Ensure UI + default config travel with the portable build.
Copy-Item (Join-Path $Root "ui")     $Dist -Recurse -Force
Copy-Item (Join-Path $Root "config") $Dist -Recurse -Force

Write-Host "Portable build ready at $Dist"
Write-Host "Run: $Dist\nyx.exe"
