// Windows entry point for nyx Browser.
#include <windows.h>

#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"

#include "nyx_app.h"
#include "paths.h"

// Multi-process: this same binary is launched by CEF for the render/gpu/utility
// sub-processes. CefExecuteProcess handles those and returns >= 0 for them.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
  // High DPI is handled by the application manifest (nyx.exe.manifest).

  CefMainArgs main_args(hInstance);
  CefRefPtr<nyx::NyxApp> app(new nyx::NyxApp());

  // Sub-process execution (renderer, gpu, etc.).
  int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
  if (exit_code >= 0) return exit_code;

  // Browser process settings.
  CefSettings settings;
  settings.no_sandbox = true;  // sandbox hardening is a future phase
  settings.multi_threaded_message_loop = false;
  CefString(&settings.cache_path) = nyx::DataSubDir("cache");
  CefString(&settings.root_cache_path) = nyx::DataDir();
  CefString(&settings.log_file) = nyx::Join(nyx::DataSubDir("logs"), "nyx.log");
  settings.log_severity = LOGSEVERITY_WARNING;

  CefInitialize(main_args, settings, app.get(), nullptr);
  CefRunMessageLoop();
  CefShutdown();
  return 0;
}
