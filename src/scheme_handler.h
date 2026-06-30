// Serves the dark UI over the internal "nyx" scheme:
//   nyx://ui/index.html, nyx://ui/css/theme.css, nyx://ui/js/app.js, ...
// Files are read from <ExeDir>/ui. The scheme is registered as standard +
// secure so it behaves like https for cookies, fetch, and module scripts.
#ifndef NYX_SCHEME_HANDLER_H_
#define NYX_SCHEME_HANDLER_H_

#include "include/cef_app.h"

namespace nyx {

// Call from CefApp::OnRegisterCustomSchemes (all processes).
void RegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar);

// Call once after CefInitialize (browser process) to install the factory.
void RegisterSchemeHandlerFactory();

}  // namespace nyx

#endif  // NYX_SCHEME_HANDLER_H_
