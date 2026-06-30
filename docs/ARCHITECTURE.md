# nyx Browser - Implementation Architecture

This maps the approved architecture onto the shipped code.

## Process model (CEF multi-process)

- **Browser process** - `wWinMain` (`src/main.cc`) initializes CEF and runs the
  message loop. Owns the window, tabs, network policy, storage, IPC.
- **Render processes** - one per site, sandboxed by Chromium. The chrome UI
  render process hosts the renderer-side message router (`NyxApp`).
- The same `nyx.exe` is re-launched for render/gpu/utility sub-processes;
  `CefExecuteProcess` short-circuits those.

## Components

| File | Role |
|------|------|
| `main.cc` | Entry point, CEF init, portable cache paths, message loop. |
| `nyx_app.*` | `CefApp`: registers `nyx` scheme, creates the window, hosts the renderer-side IPC router. |
| `nyx_window.*` | `CefWindowDelegate`: builds the UI browser view (fills window), owns `TabManager`, pushes events to the UI. |
| `nyx_client.*` | `CefClient` for chrome + content roles: lifecycle, load/display, downloads, privacy (DNT, 3rd-party cookie filter), browser-side IPC router. |
| `tab_manager.*` | Tabs as `CefBrowserView` overlays docked into the UI content slot; create/close/activate/navigate, session snapshots. |
| `message_bridge.*` | Browser-side IPC handler: turns UI JSON requests into core actions. |
| `storage.*` | Portable JSON persistence (bookmarks, history, downloads, settings, session). |
| `scheme_handler.*` | Serves the dark UI over `nyx://ui/...` from `<exe>/ui`. |
| `paths.*` | Resolves all paths relative to the executable (portability). |
| `ui/` | The dark UI: one HTML/CSS/JS app rendered in a CEF browser view. |

## Data flow

```
UI (omnibox/buttons)
   -> window.nyx.call(action, params)        [ui/js/bridge.js]
   -> window.cefQuery (CEF message router)
   -> NyxClient (browser-side router)
   -> MessageBridge::OnQuery                  [src/message_bridge.cc]
   -> TabManager / Storage                    [actions + persistence]
   -> CEF content browser view                [renders the page]

Core events (load/title/url/download)
   -> NyxClient handlers
   -> TabManager
   -> NyxWindow::EmitToUi  (ExecuteJavaScript: window.nyx._emit)
   -> UI updates tabs / toolbar / panels
```

## State

- **UI state** - in `app.js` memory only (tabs map, active id, open panel).
- **Authoritative state** - the C++ core; persisted by `Storage` to `data/*.json`.
- **Engine state** - Chromium cache/cookies under `data/cache`.
- The UI holds a projection synced via IPC events; the core is the source of truth.

## Portability

`paths.cc` resolves everything from `GetModuleFileName`. `main.cc` points
`root_cache_path` / `cache_path` into `data/`. No writes to `%APPDATA%` or the
registry. Copy the folder = move the browser.

## Privacy

- `DNT: 1` header injection (`OnBeforeResourceLoad`).
- Third-party cookie blocking via `CefCookieAccessFilter` (registrable-domain
  comparison against the top-level frame).
- `disable-breakpad`, `disable-component-update` engine switches.

## Future modules (designed-for, not yet wired)

`nyx_client.cc`'s `OnBeforeResourceLoad` and `OnDownloadUpdated` are the
designed tap points for Threat Intelligence / VirusTotal / IOC extraction: every
request and download already flows through the core, so an intel service can be
attached there without touching the UI or engine.
