# nyx Browser

A fast, portable, privacy-focused Chromium browser for Windows, built on
**CEF (Chromium Embedded Framework)** with a custom dark HTML UI.

Everything runs from a single folder. Nothing is written to `%APPDATA%`,
`%LOCALAPPDATA%`, or the registry. All state lives next to the executable
under `data/`.

## Features

- Full Chromium engine (browses YouTube, GitHub, ChatGPT, VirusTotal, anything)
- Tabs (new / close / switch / reorder-ready)
- Address bar (omnibox) with smart URL/search detection
- Back / Forward / Refresh / Stop
- Downloads (tracked, persisted, opens to `data/downloads/`)
- Bookmarks (add / remove / persisted)
- History (recorded / searchable / clearable)
- Session restore (reopens your tabs)
- Dark modern UI (custom HTML/CSS chrome)
- Collapsible sidebar with panels
- Settings (persisted, portable)
- Privacy defaults (DNT, referrer trimming, third-party cookie policy)

## Architecture

See `docs` / the approved architecture. In short:

- **C++20 core** hosts CEF, manages the window, tabs, network, storage, IPC.
- **Custom dark UI** is one HTML/CSS/JS app rendered in a CEF browser view and
  served over the internal `nyx://ui/` scheme.
- **Web content** is rendered in separate CEF browser views (one per tab),
  composited as overlays inside the UI's content slot.
- **Storage** is portable JSON under `data/` (bookmarks, history, downloads,
  settings, session).

## Build (Windows)

Prerequisites:

- Windows 10/11 x64
- Visual Studio 2022 (Desktop C++ workload) + Windows 10/11 SDK
- CMake >= 3.21
- PowerShell 5+

Steps:

```powershell
# 1. Fetch the pinned CEF binary distribution (~250 MB) and third-party headers
./tools/fetch_cef.ps1
./tools/fetch_deps.ps1

# 2. Configure + build (Release)
./tools/build.ps1

# 3. Package the portable folder into dist/NyxBrowser/
./tools/package.ps1
```

Run `dist/NyxBrowser/nyx.exe`.

## Why fetched, not vendored

The CEF binary distribution is ~250 MB and the `nlohmann/json` header is large.
No Chromium-derived project commits these to source control. The fetch scripts
pin exact versions for reproducibility.
