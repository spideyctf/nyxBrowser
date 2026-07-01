#include "nyx_window.h"

#include "include/cef_app.h"
#include "include/views/cef_fill_layout.h"
#include "include/wrapper/cef_helpers.h"

#include "intel/feed_service.h"
#include "intel/intel_service.h"
#include "message_bridge.h"
#include "nyx_client.h"
#include "storage.h"

namespace nyx {

NyxWindow::NyxWindow() = default;

void NyxWindow::OnWindowCreated(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  window_ = window;
  window_->SetTitle("nyx Browser");

  chrome_client_ = new NyxClient(NyxClient::kChrome);
  bridge_ = std::make_unique<MessageBridge>(&tabs_);
  chrome_client_->AttachBridge(bridge_.get());

  CefBrowserSettings settings;
  chrome_view_ = CefBrowserView::CreateBrowserView(
      chrome_client_, "nyx://ui/index.html", settings, nullptr, nullptr,
      nullptr);

  // The chrome view fills the entire window; content tabs are overlays.
  window_->SetToFillLayout();
  window_->AddChildView(chrome_view_);

  tabs_.Init(window_, [this](const std::string& e, const nlohmann::json& p) {
    EmitToUi(e, p);
  });

  // Wire the intelligence + feed services to push results into the UI, then
  // start the daily cybersecurity feed refresh. IOC extraction and feeds work
  // without any API key; VirusTotal verdicts require one (see Settings).
  auto emit = [this](const std::string& e, const nlohmann::json& p) {
    EmitToUi(e, p);
  };
  nlohmann::json settings_json = Storage::Get().Settings();
  intel::IntelService::Get().SetEmitter(emit);
  intel::IntelService::Get().Configure(
      settings_json.value("virusTotalApiKey", std::string()),
      settings_json.value("intelEnabled", false));
  intel::FeedService::Get().SetEmitter(emit);
  intel::FeedService::Get().Start();

  window_->CenterWindow(CefSize(1280, 820));
  window_->Show();
  chrome_view_->RequestFocus();
}

void NyxWindow::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  window_ = nullptr;
  chrome_view_ = nullptr;
  // Last window closed -> exit the message loop.
  CefQuitMessageLoop();
}

bool NyxWindow::CanClose(CefRefPtr<CefWindow> window) {
  // Allow the chrome browser to close cleanly.
  CefRefPtr<CefBrowser> browser =
      chrome_view_ ? chrome_view_->GetBrowser() : nullptr;
  if (browser) return browser->GetHost()->TryCloseBrowser();
  return true;
}

CefRect NyxWindow::GetInitialBounds(CefRefPtr<CefWindow> window) {
  return CefRect(0, 0, 1280, 820);
}

void NyxWindow::EmitToUi(const std::string& event,
                         const nlohmann::json& payload) {
  if (!chrome_view_) return;
  CefRefPtr<CefBrowser> browser = chrome_view_->GetBrowser();
  if (!browser) return;
  CefRefPtr<CefFrame> frame = browser->GetMainFrame();
  if (!frame) return;

  const std::string js = "window.nyx && window.nyx._emit(" +
                         nlohmann::json(event).dump() + "," + payload.dump() +
                         ");";
  frame->ExecuteJavaScript(js, frame->GetURL(), 0);
}

}  // namespace nyx
