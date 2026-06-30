// Owns all content browser views (tabs). Each tab is a CefBrowserView added to
// the window as a custom-docked overlay sized to the UI's content slot. Only the
// active tab's overlay is visible. Reports state changes to the UI via emit_.
#ifndef NYX_TAB_MANAGER_H_
#define NYX_TAB_MANAGER_H_

#include <functional>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_overlay_controller.h"
#include "include/views/cef_window.h"
#include "nlohmann/json.hpp"

namespace nyx {

class NyxClient;

class TabManager {
 public:
  using EmitFn = std::function<void(const std::string&, const nlohmann::json&)>;

  TabManager();
  ~TabManager();

  void Init(CefRefPtr<CefWindow> window, EmitFn emit);

  // Commands (called from the IPC bridge / UI).
  int NewTab(const std::string& url, bool activate);
  void CloseTab(int id);
  void ActivateTab(int id);
  void Navigate(int id, const std::string& url);
  void Back(int id);
  void Forward(int id);
  void Reload(int id);
  void Stop(int id);
  void SetContentBounds(const CefRect& bounds);
  nlohmann::json ListTabs();
  nlohmann::json SessionSnapshot();
  int active_id() const { return active_id_; }

  // Content client callbacks.
  void OnContentCreated(CefRefPtr<CefBrowser> browser);
  void OnContentClosed(CefRefPtr<CefBrowser> browser);
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool loading, bool can_back, bool can_forward);
  void OnTitleChange(CefRefPtr<CefBrowser> browser, const std::string& title);
  void OnAddressChange(CefRefPtr<CefBrowser> browser, const std::string& url);
  void OnDownloadUpdated(const nlohmann::json& item);

 private:
  struct Tab {
    int id = 0;
    CefRefPtr<CefBrowserView> view;
    CefRefPtr<CefOverlayController> overlay;
    std::string url;
    std::string title;
    bool loading = false;
    bool can_back = false;
    bool can_forward = false;
  };

  Tab* Find(int id);
  Tab* FindByBrowser(CefRefPtr<CefBrowser> browser);
  void EmitTabUpdate(const Tab& tab);
  void ApplyVisibility();
  void SaveSession();
  CefRefPtr<CefBrowser> BrowserFor(int id);

  CefRefPtr<CefWindow> window_;
  CefRefPtr<NyxClient> content_client_;
  EmitFn emit_;
  std::vector<Tab> tabs_;
  int next_id_ = 1;
  int active_id_ = 0;
  CefRect content_bounds_{0, 88, 1280, 712};
};

}  // namespace nyx

#endif  // NYX_TAB_MANAGER_H_
