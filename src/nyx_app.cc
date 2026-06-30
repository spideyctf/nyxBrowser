#include "nyx_app.h"

#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include "nyx_client.h"
#include "nyx_window.h"
#include "scheme_handler.h"

namespace nyx {

void NyxApp::OnRegisterCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {
  RegisterCustomSchemes(registrar);
}

void NyxApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  // Privacy / footprint defaults applied to the engine.
  if (process_type.empty()) {
    command_line->AppendSwitch("disable-breakpad");
    command_line->AppendSwitch("disable-component-update");
    command_line->AppendSwitchWithValue("force-dark-mode", "1");
  }
}

void NyxApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();
  RegisterSchemeHandlerFactory();
  NyxClient::RefreshPrivacy();

  // Create the top-level window; it builds the UI and tabs.
  CefWindow::CreateTopLevelWindow(new NyxWindow());
}

void NyxApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefV8Context> context) {
  if (!render_router_) {
    CefMessageRouterConfig config;  // defaults match the browser side
    render_router_ = CefMessageRouterRendererSide::Create(config);
  }
  render_router_->OnContextCreated(browser, frame, context);
}

void NyxApp::OnContextReleased(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               CefRefPtr<CefV8Context> context) {
  if (render_router_) render_router_->OnContextReleased(browser, frame, context);
}

bool NyxApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefProcessId source_process,
                                      CefRefPtr<CefProcessMessage> message) {
  if (render_router_) {
    return render_router_->OnProcessMessageReceived(browser, frame,
                                                    source_process, message);
  }
  return false;
}

}  // namespace nyx
