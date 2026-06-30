#include "tab_manager.h"

#include "include/wrapper/cef_helpers.h"

#include "nyx_client.h"
#include "storage.h"

namespace nyx {

TabManager::TabManager() = default;
TabManager::~TabManager() = default;

void TabManager::Init(CefRefPtr<CefWindow> window, EmitFn emit) {
  window_ = window;
  emit_ = std::move(emit);
  content_client_ = new NyxClient(NyxClient::kContent);
  content_client_->SetTabManager(this);
}

TabManager::Tab* TabManager::Find(int id) {
  for (auto& t : tabs_)
    if (t.id == id) return &t;
  return nullptr;
}

TabManager::Tab* TabManager::FindByBrowser(CefRefPtr<CefBrowser> browser) {
  if (!browser) return nullptr;
  for (auto& t : tabs_) {
    CefRefPtr<CefBrowser> b = t.view ? t.view->GetBrowser() : nullptr;
    if (b && b->IsSame(browser)) return &t;
  }
  return nullptr;
}

CefRefPtr<CefBrowser> TabManager::BrowserFor(int id) {
  Tab* t = Find(id);
  return (t && t->view) ? t->view->GetBrowser() : nullptr;
}

int TabManager::NewTab(const std::string& url, bool activate) {
  CEF_REQUIRE_UI_THREAD();
  Tab tab;
  tab.id = next_id_++;
  tab.url = url;

  CefBrowserSettings settings;
  tab.view = CefBrowserView::CreateBrowserView(
      content_client_, url, settings, nullptr, nullptr, nullptr);

  // Add as a custom-docked overlay so we control its exact rectangle.
  tab.overlay = window_->AddOverlayView(
      tab.view, CEF_DOCKING_MODE_CUSTOM, /*can_activate=*/true);
  tab.overlay->SetBounds(content_bounds_);

  tabs_.push_back(tab);
  emit_("tab.created",
        {{"id", tab.id}, {"url", url}, {"title", url}, {"active", activate}});

  if (activate || active_id_ == 0) ActivateTab(tab.id);
  SaveSession();
  return tab.id;
}

void TabManager::CloseTab(int id) {
  CEF_REQUIRE_UI_THREAD();
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].id != id) continue;
    CefRefPtr<CefBrowser> b = tabs_[i].view ? tabs_[i].view->GetBrowser() : nullptr;
    if (tabs_[i].overlay) tabs_[i].overlay->Destroy();
    if (b) b->GetHost()->CloseBrowser(/*force_close=*/true);
    tabs_.erase(tabs_.begin() + i);
    emit_("tab.closed", {{"id", id}});

    if (active_id_ == id) {
      active_id_ = 0;
      if (!tabs_.empty()) {
        int neighbor = tabs_[i < tabs_.size() ? i : tabs_.size() - 1].id;
        ActivateTab(neighbor);
      }
    }
    SaveSession();
    return;
  }
}

void TabManager::ActivateTab(int id) {
  CEF_REQUIRE_UI_THREAD();
  if (!Find(id)) return;
  active_id_ = id;
  ApplyVisibility();
  emit_("tab.activated", {{"id", id}});
  if (CefRefPtr<CefBrowser> b = BrowserFor(id))
    if (b->GetHost()) b->GetHost()->SetFocus(true);
}

void TabManager::ApplyVisibility() {
  for (auto& t : tabs_) {
    if (!t.overlay) continue;
    const bool active = (t.id == active_id_);
    if (active) t.overlay->SetBounds(content_bounds_);
    t.overlay->SetVisible(active);
  }
}

void TabManager::Navigate(int id, const std::string& url) {
  if (CefRefPtr<CefBrowser> b = BrowserFor(id))
    b->GetMainFrame()->LoadURL(url);
}

void TabManager::Back(int id) {
  if (CefRefPtr<CefBrowser> b = BrowserFor(id)) b->GoBack();
}
void TabManager::Forward(int id) {
  if (CefRefPtr<CefBrowser> b = BrowserFor(id)) b->GoForward();
}
void TabManager::Reload(int id) {
  if (CefRefPtr<CefBrowser> b = BrowserFor(id)) b->Reload();
}
void TabManager::Stop(int id) {
  if (CefRefPtr<CefBrowser> b = BrowserFor(id)) b->StopLoad();
}

void TabManager::SetContentBounds(const CefRect& bounds) {
  content_bounds_ = bounds;
  if (Tab* t = Find(active_id_))
    if (t->overlay) t->overlay->SetBounds(content_bounds_);
}

nlohmann::json TabManager::ListTabs() {
  nlohmann::json arr = nlohmann::json::array();
  for (auto& t : tabs_) {
    arr.push_back({{"id", t.id},
                   {"url", t.url},
                   {"title", t.title.empty() ? t.url : t.title},
                   {"loading", t.loading},
                   {"canBack", t.can_back},
                   {"canForward", t.can_forward},
                   {"active", t.id == active_id_}});
  }
  return arr;
}

nlohmann::json TabManager::SessionSnapshot() {
  nlohmann::json arr = nlohmann::json::array();
  for (auto& t : tabs_) {
    if (t.url.rfind("nyx://", 0) == 0) continue;
    arr.push_back({{"url", t.url}, {"title", t.title}});
  }
  return arr;
}

void TabManager::SaveSession() { Storage::Get().SaveSession(SessionSnapshot()); }

void TabManager::EmitTabUpdate(const Tab& tab) {
  emit_("tab.updated", {{"id", tab.id},
                        {"url", tab.url},
                        {"title", tab.title.empty() ? tab.url : tab.title},
                        {"loading", tab.loading},
                        {"canBack", tab.can_back},
                        {"canForward", tab.can_forward},
                        {"active", tab.id == active_id_}});
}

// ---------------- content client callbacks ----------------
void TabManager::OnContentCreated(CefRefPtr<CefBrowser> browser) {
  // Browser object is now live; make sure the active overlay is correctly sized.
  ApplyVisibility();
}

void TabManager::OnContentClosed(CefRefPtr<CefBrowser> browser) {
  // Tab removal is driven by CloseTab(); nothing extra required here.
}

void TabManager::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                      bool loading, bool can_back,
                                      bool can_forward) {
  if (Tab* t = FindByBrowser(browser)) {
    t->loading = loading;
    t->can_back = can_back;
    t->can_forward = can_forward;
    EmitTabUpdate(*t);
  }
}

void TabManager::OnTitleChange(CefRefPtr<CefBrowser> browser,
                               const std::string& title) {
  if (Tab* t = FindByBrowser(browser)) {
    t->title = title;
    EmitTabUpdate(*t);
    if (!t->url.empty()) Storage::Get().AddHistory(t->url, title);
  }
}

void TabManager::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                 const std::string& url) {
  if (Tab* t = FindByBrowser(browser)) {
    t->url = url;
    EmitTabUpdate(*t);
    SaveSession();
  }
}

void TabManager::OnDownloadUpdated(const nlohmann::json& item) {
  Storage::Get().UpsertDownload(item);
  emit_("download.updated", item);
}

}  // namespace nyx
