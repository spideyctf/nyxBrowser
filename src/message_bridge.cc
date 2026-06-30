#include "message_bridge.h"

#include <string>

#include "nlohmann/json.hpp"

#include "nyx_client.h"
#include "storage.h"
#include "tab_manager.h"

namespace nyx {

using nlohmann::json;

bool MessageBridge::OnQuery(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            int64_t query_id,
                            const CefString& request,
                            bool persistent,
                            CefRefPtr<Callback> callback) {
  json req;
  try {
    req = json::parse(request.ToString());
  } catch (...) {
    return false;  // not our message
  }

  const std::string action = req.value("action", "");
  Storage& store = Storage::Get();

  auto ok = [&](const json& data = json::object()) {
    callback->Success(data.dump());
  };

  // ---------------- lifecycle ----------------
  if (action == "ui.ready") {
    NyxClient::RefreshPrivacy();
    json settings = store.Settings();

    // Session restore (or open homepage).
    json session = store.Session();
    bool restored = false;
    if (settings.value("restoreSession", true) && session.is_array() &&
        !session.empty()) {
      for (const auto& t : session) {
        std::string url = t.value("url", "");
        if (!url.empty()) tabs_->NewTab(url, false);
      }
      restored = true;
    }
    if (tabs_->ListTabs().empty()) {
      tabs_->NewTab(settings.value("homepage", "https://github.com"), true);
    } else {
      // Activate the first restored tab.
      auto list = tabs_->ListTabs();
      tabs_->ActivateTab(list[0].value("id", 0));
    }
    ok({{"settings", settings}, {"restored", restored}});
    return true;
  }

  // ---------------- tabs ----------------
  if (action == "tab.new") {
    json s = store.Settings();
    std::string url = req.value("url", s.value("homepage", "https://github.com"));
    int id = tabs_->NewTab(url, true);
    ok({{"id", id}});
    return true;
  }
  if (action == "tab.close") {
    tabs_->CloseTab(req.value("id", 0));
    ok();
    return true;
  }
  if (action == "tab.activate") {
    tabs_->ActivateTab(req.value("id", 0));
    ok();
    return true;
  }
  if (action == "tab.list") {
    ok({{"tabs", tabs_->ListTabs()}});
    return true;
  }

  // ---------------- navigation ----------------
  if (action == "nav.go") {
    int id = req.value("id", tabs_->active_id());
    tabs_->Navigate(id, req.value("url", ""));
    ok();
    return true;
  }
  if (action == "nav.back") { tabs_->Back(req.value("id", tabs_->active_id())); ok(); return true; }
  if (action == "nav.forward") { tabs_->Forward(req.value("id", tabs_->active_id())); ok(); return true; }
  if (action == "nav.reload") { tabs_->Reload(req.value("id", tabs_->active_id())); ok(); return true; }
  if (action == "nav.stop") { tabs_->Stop(req.value("id", tabs_->active_id())); ok(); return true; }

  // ---------------- content overlay bounds ----------------
  if (action == "content.bounds") {
    CefRect r(req.value("x", 0), req.value("y", 88),
              req.value("w", 1280), req.value("h", 712));
    tabs_->SetContentBounds(r);
    ok();
    return true;
  }

  // ---------------- bookmarks ----------------
  if (action == "bookmark.add") {
    store.AddBookmark(req.value("url", ""), req.value("title", ""));
    ok({{"bookmarks", store.Bookmarks()}});
    return true;
  }
  if (action == "bookmark.remove") {
    store.RemoveBookmark(req.value("url", ""));
    ok({{"bookmarks", store.Bookmarks()}});
    return true;
  }
  if (action == "bookmark.list") {
    ok({{"bookmarks", store.Bookmarks()}});
    return true;
  }

  // ---------------- history ----------------
  if (action == "history.list") { ok({{"history", store.History()}}); return true; }
  if (action == "history.clear") { store.ClearHistory(); ok({{"history", store.History()}}); return true; }

  // ---------------- downloads ----------------
  if (action == "downloads.list") { ok({{"downloads", store.Downloads()}}); return true; }

  // ---------------- settings ----------------
  if (action == "settings.get") { ok({{"settings", store.Settings()}}); return true; }
  if (action == "settings.set") {
    store.SetSetting(req.value("key", ""), req.value("value", json()));
    NyxClient::RefreshPrivacy();
    ok({{"settings", store.Settings()}});
    return true;
  }

  callback->Failure(404, "unknown action: " + action);
  return true;
}

}  // namespace nyx
