<#
  Downloads and extracts the pinned CEF binary distribution into third_party/cef.
  CEF is ~250 MB; this is the standard fetch step for every CEF-based project.
#>
param(
  [string]$CefVersion = "124.3.9+g9bd638f+chromium-124.0.6367.207",
  [string]$Platform   = "windows64"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Dest = Join-Path $Root "third_party/cef"

if (Test-Path (Join-Path $Dest "cmake/cef_variables.cmake")) {
  Write-Host "CEF already present at $Dest - skipping."
  exit 0
}

# CEF distributions are indexed at https://cef-builds.spotifycdn.com/index.html
$Encoded = [uri]::EscapeDataString($CefVersion)
$File    = "cef_binary_${Encoded}_${Platform}_minimal.tar.bz2"
$Url     = "https://cef-builds.spotifycdn.com/$File"

$Tmp = Join-Path $env:TEMP "nyx_cef"
New-Item -ItemType Directory -Force -Path $Tmp | Out-Null
$Archive = Join-Path $Tmp $File

Write-Host "Downloading CEF $CefVersion ..."
Invoke-WebRequest -Uri $Url -OutFile $Archive

Write-Host "Extracting ..."
# tar is built into Windows 10/11 and handles .tar.bz2
tar -xjf $Archive -C $Tmp

$Extracted = Get-ChildItem -Path $Tmp -Directory |
  Where-Object { $_.Name -like "cef_binary_*" } | Select-Object -First 1

New-Item -ItemType Directory -Force -Path (Split-Path $Dest) | Out-Null
if (Test-Path $Dest) { Remove-Item -Recurse -Force $Dest }
Move-Item $Extracted.FullName $Dest

Write-Host "CEF ready at $Dest"
