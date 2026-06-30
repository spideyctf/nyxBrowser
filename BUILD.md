# Building nyx Browser

## 1. Environment

- Visual Studio 2022 with the "Desktop development with C++" workload.
- Windows SDK 10.0.22621 or newer.
- CMake 3.21+ (the VS installer ships one; or install standalone).
- PowerShell.

## 2. Fetch dependencies

```powershell
./tools/fetch_cef.ps1     # downloads + extracts CEF into third_party/cef
./tools/fetch_deps.ps1    # downloads nlohmann/json single header
```

The CEF version is pinned in `tools/fetch_cef.ps1` (`$CefVersion`). To change
the engine version, edit that variable; the build picks it up automatically.

## 3. Build

```powershell
./tools/build.ps1                 # Release
./tools/build.ps1 -Config Debug   # Debug
```

This runs CMake configure + build. Output lands in `build/`.

## 4. Package portable distribution

```powershell
./tools/package.ps1
```

Produces `dist/NyxBrowser/` containing:

```
nyx.exe
libcef.dll, chrome_elf.dll, *.bin, *.pak, locales/, ...   (CEF runtime)
ui/            (dark UI assets)
config/        (default settings)
data/          (created at first run; all state lives here)
```

Zip `dist/NyxBrowser/` and it runs on any Windows x64 machine — portable.

## Troubleshooting

- **CMake can't find CEF**: ensure `./tools/fetch_cef.ps1` completed and
  `third_party/cef/cmake/cef_variables.cmake` exists.
- **Missing DLLs at runtime**: re-run `./tools/package.ps1`; it copies the CEF
  runtime next to `nyx.exe`.
- **Blank window**: confirm `ui/` is next to `nyx.exe` (the packager copies it).
