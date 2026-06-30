// Top-level window. Hosts the dark UI (chrome) browser view filling the window,
// and owns the TabManager whose content overlays render web pages on top of the
// UI's content slot. Pushes events to the UI by executing JS in the chrome frame.
#ifndef NYX_WINDOW_H_
#define NYX_WINDOW_H_

#include <memory>
#include <string>

#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "nlohmann/json.hpp"

#include "tab_manager.h"

namespace nyx {

class NyxClient;
class MessageBridge;

class NyxWindow : public CefWindowDelegate {
 public:
  NyxWindow();

  // Push an event into the UI: window.nyx._emit(event, payload).
  void EmitToUi(const std::string& event, const nlohmann::json& payload);

  // CefWindowDelegate
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  bool CanResize(CefRefPtr<CefWindow> window) override { return true; }
  bool CanMaximize(CefRefPtr<CefWindow> window) override { return true; }
  bool CanMinimize(CefRefPtr<CefWindow> window) override { return true; }
  bool CanClose(CefRefPtr<CefWindow> window) override;
  CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override;
  cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow> window) override {
    return CEF_SHOW_STATE_NORMAL;
  }
  bool IsFrameless(CefRefPtr<CefWindow> window) override { return false; }

 private:
  CefRefPtr<CefWindow> window_;
  CefRefPtr<NyxClient> chrome_client_;
  CefRefPtr<CefBrowserView> chrome_view_;
  std::unique_ptr<MessageBridge> bridge_;
  TabManager tabs_;

  IMPLEMENT_REFCOUNTING(NyxWindow);
  DISALLOW_COPY_AND_ASSIGN(NyxWindow);
};

}  // namespace nyx

#endif  // NYX_WINDOW_H_
