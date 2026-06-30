<#
  Downloads the pinned nlohmann/json single-header into third_party/json/include.
#>
param([string]$JsonVersion = "v3.11.3")

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Dir  = Join-Path $Root "third_party/json/include/nlohmann"
New-Item -ItemType Directory -Force -Path $Dir | Out-Null

$Out = Join-Path $Dir "json.hpp"
if (Test-Path $Out) { Write-Host "json.hpp already present - skipping."; exit 0 }

$Url = "https://github.com/nlohmann/json/releases/download/$JsonVersion/json.hpp"
Write-Host "Downloading nlohmann/json $JsonVersion ..."
Invoke-WebRequest -Uri $Url -OutFile $Out
Write-Host "json.hpp ready at $Out"
