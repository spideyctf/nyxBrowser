#include "nyx_client.h"

#include <string>

#include "include/cef_parser.h"
#include "include/wrapper/cef_helpers.h"
#include "nlohmann/json.hpp"

#include "message_bridge.h"
#include "paths.h"
#include "storage.h"
#include "tab_manager.h"

namespace nyx {

namespace {

std::atomic<bool> g_dnt{true};
std::atomic<bool> g_block_third_party{true};

std::string HostOf(const CefString& url) {
  CefURLParts parts;
  if (!CefParseURL(url, parts)) return std::string();
  return CefString(&parts.host).ToString();
}

// Approximate registrable domain: last two labels (e.g. "github.com").
std::string RegistrableDomain(const std::string& host) {
  size_t last = host.find_last_of('.');
  if (last == std::string::npos) return host;
  size_t prev = host.find_last_of('.', last - 1);
  if (prev == std::string::npos) return host;
  return host.substr(prev + 1);
}

bool SameSite(const CefString& a, const CefString& b) {
  std::string ha = HostOf(a), hb = HostOf(b);
  if (ha.empty() || hb.empty()) return true;  // be permissive if unknown
  return RegistrableDomain(ha) == RegistrableDomain(hb);
}

}  // namespace

NyxClient::NyxClient(Role role) : role_(role) {
  if (role_ == kChrome) {
    CefMessageRouterConfig config;  // defaults: cefQuery / cefQueryCancel
    message_router_ = CefMessageRouterBrowserSide::Create(config);
  }
}

void NyxClient::AttachBridge(MessageBridge* bridge) {
  if (message_router_) message_router_->AddHandler(bridge, /*first=*/false);
}

void NyxClient::RefreshPrivacy() {
  nlohmann::json s = Storage::Get().Settings();
  g_dnt = s.value("doNotTrack", true);
  g_block_third_party = s.value("blockThirdPartyCookies", true);
}

// ---------------- CefClient ----------------
bool NyxClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefProcessId source_process,
                                         CefRefPtr<CefProcessMessage> message) {
  if (role_ == kChrome && message_router_) {
    return message_router_->OnProcessMessageReceived(browser, frame,
                                                     source_process, message);
  }
  return false;
}

// ---------------- CefLifeSpanHandler ----------------
void NyxClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  if (role_ == kContent && tab_manager_) tab_manager_->OnContentCreated(browser);
}

void NyxClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  if (role_ == kChrome && message_router_) message_router_->OnBeforeClose(browser);
  if (role_ == kContent && tab_manager_) tab_manager_->OnContentClosed(browser);
}

bool NyxClient::OnBeforePopup(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& target_url,
    const CefString& target_frame_name,
    cef_window_open_disposition_t target_disposition,
    bool user_gesture,
    const CefPopupFeatures& popupFeatures,
    CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client,
    CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue>& extra_info,
    bool* no_javascript_access) {
  // Open target="_blank" / window.open in a new tab instead of an OS popup.
  if (role_ == kContent && tab_manager_ && !target_url.empty()) {
    tab_manager_->NewTab(target_url.ToString(), /*activate=*/true);
  }
  return true;  // block the default popup window
}

// ---------------- CefLoadHandler ----------------
void NyxClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                     bool isLoading,
                                     bool canGoBack,
                                     bool canGoForward) {
  if (role_ == kContent && tab_manager_) {
    tab_manager_->OnLoadingStateChange(browser, isLoading, canGoBack,
                                       canGoForward);
  }
}

// ---------------- CefDisplayHandler ----------------
void NyxClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                              const CefString& title) {
  if (role_ == kContent && tab_manager_) {
    tab_manager_->OnTitleChange(browser, title.ToString());
  }
}

void NyxClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                const CefString& url) {
  if (role_ == kContent && tab_manager_ && frame && frame->IsMain()) {
    tab_manager_->OnAddressChange(browser, url.ToString());
  }
}

// ---------------- CefDownloadHandler ----------------
void NyxClient::OnBeforeDownload(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    const CefString& suggested_name,
    CefRefPtr<CefBeforeDownloadCallback> callback) {
  const std::string dir = DataSubDir("downloads");
  const std::string full = Join(dir, suggested_name.ToString());
  callback->Continue(full, /*show_dialog=*/false);
}

void NyxClient::OnDownloadUpdated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    CefRefPtr<CefDownloadItemCallback> callback) {
  if (!(role_ == kContent && tab_manager_)) return;
  nlohmann::json item = {
      {"id", download_item->GetId()},
      {"url", download_item->GetURL().ToString()},
      {"filename", download_item->GetSuggestedFileName().ToString()},
      {"path", download_item->GetFullPath().ToString()},
      {"received", static_cast<long long>(download_item->GetReceivedBytes())},
      {"total", static_cast<long long>(download_item->GetTotalBytes())},
      {"percent", download_item->GetPercentComplete()},
      {"inProgress", download_item->IsInProgress()},
      {"complete", download_item->IsComplete()},
      {"canceled", download_item->IsCanceled()}};
  tab_manager_->OnDownloadUpdated(item);
}

// ---------------- CefRequestHandler ----------------
void NyxClient::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                          TerminationStatus status,
                                          int error_code,
                                          const CefString& error_string) {
  if (role_ == kChrome && message_router_) {
    message_router_->OnRenderProcessTerminated(browser);
  }
}

CefRefPtr<CefResourceRequestHandler> NyxClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    bool is_navigation,
    bool is_download,
    const CefString& request_initiator,
    bool& disable_default_handling) {
  return this;  // enforce privacy defaults on content network traffic
}

// ---------------- CefResourceRequestHandler ----------------
CefRefPtr<CefCookieAccessFilter> NyxClient::GetCookieAccessFilter(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request) {
  return g_block_third_party ? this : nullptr;
}

cef_return_value_t NyxClient::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> callback) {
  if (g_dnt) request->SetHeaderByName("DNT", "1", /*overwrite=*/true);
  return RV_CONTINUE;
}

// ---------------- CefCookieAccessFilter ----------------
bool NyxClient::CanSendCookie(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              const CefCookie& cookie) {
  if (!browser || !browser->GetMainFrame()) return true;
  return SameSite(request->GetURL(), browser->GetMainFrame()->GetURL());
}

bool NyxClient::CanSaveCookie(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefResponse> response,
                              const CefCookie& cookie) {
  if (!browser || !browser->GetMainFrame()) return true;
  return SameSite(request->GetURL(), browser->GetMainFrame()->GetURL());
}

}  // namespace nyx
