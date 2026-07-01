#include "intel/feed_service.h"

#include "include/cef_request.h"
#include "include/cef_response.h"
#include "include/cef_task.h"
#include "include/cef_urlrequest.h"
#include "include/wrapper/cef_helpers.h"

#include <ctime>
#include <fstream>
#include <utility>
#include <vector>

#include "paths.h"

namespace nyx {
namespace intel {

namespace {

constexpr long kDaySeconds = 86400;

// Curated, public cybersecurity RSS/Atom feeds.
struct Feed { const char* name; const char* url; };
const Feed kFeeds[] = {
    {"The Hacker News", "https://feeds.feedburner.com/TheHackersNews"},
    {"BleepingComputer", "https://www.bleepingcomputer.com/feed/"},
    {"Krebs on Security", "https://krebsonsecurity.com/feed/"},
    {"CISA Advisories", "https://www.cisa.gov/cybersecurity-advisories/all.xml"},
    {"Dark Reading", "https://www.darkreading.com/rss.xml"},
};

std::string CacheFile() { return Join(DataDir(), "feeds_cache.json"); }

class FuncTask : public CefTask {
 public:
  explicit FuncTask(std::function<void()> fn) : fn_(std::move(fn)) {}
  void Execute() override { if (fn_) fn_(); }
 private:
  std::function<void()> fn_;
  IMPLEMENT_REFCOUNTING(FuncTask);
  DISALLOW_COPY_AND_ASSIGN(FuncTask);
};

// Fetches one feed and reports the raw body back to FeedService.
class FeedRequestClient : public CefURLRequestClient {
 public:
  FeedRequestClient(std::string name, std::string url)
      : name_(std::move(name)), url_(std::move(url)) {}

  void OnRequestComplete(CefRefPtr<CefURLRequest> request) override {
    CEF_REQUIRE_UI_THREAD();
    CefRefPtr<CefResponse> resp = request->GetResponse();
    const int status = resp ? resp->GetStatus() : 0;
    const bool ok =
        request->GetRequestStatus() == UR_SUCCESS && status == 200;
    FeedService::Get().OnFeedDone(name_, url_, ok ? body_ : std::string());
  }

  void OnDownloadData(CefRefPtr<CefURLRequest> request, const void* data,
                      size_t data_length) override {
    body_.append(static_cast<const char*>(data), data_length);
  }
  void OnUploadProgress(CefRefPtr<CefURLRequest>, int64_t, int64_t) override {}
  void OnDownloadProgress(CefRefPtr<CefURLRequest>, int64_t, int64_t) override {}
  bool GetAuthCredentials(bool, const CefString&, int, const CefString&,
                          const CefString&,
                          CefRefPtr<CefAuthCallback>) override {
    return false;
  }

 private:
  std::string name_, url_, body_;
  IMPLEMENT_REFCOUNTING(FeedRequestClient);
  DISALLOW_COPY_AND_ASSIGN(FeedRequestClient);
};

}  // namespace

FeedService::FeedService() = default;

FeedService& FeedService::Get() {
  static FeedService instance;
  return instance;
}

void FeedService::LoadCache() {
  loaded_ = true;
  std::ifstream in(CacheFile(), std::ios::binary);
  if (!in.good()) { cache_ = {{"lastFetch", 0}, {"feeds", nlohmann::json::array()}}; return; }
  auto j = nlohmann::json::parse(in, nullptr, false);
  cache_ = j.is_discarded()
               ? nlohmann::json{{"lastFetch", 0}, {"feeds", nlohmann::json::array()}}
               : j;
}

void FeedService::SaveCache() {
  std::ofstream out(CacheFile(), std::ios::binary | std::ios::trunc);
  if (out.good()) out << cache_.dump();
}

bool FeedService::Stale() const {
  long last = cache_.value("lastFetch", 0L);
  return std::time(nullptr) - last > kDaySeconds;
}

nlohmann::json FeedService::Cached() {
  if (!loaded_) LoadCache();
  return cache_;
}

void FeedService::Start() {
  if (started_) return;
  started_ = true;
  if (!loaded_) LoadCache();
  if (Stale()) FetchAll();
  ScheduleDaily();
}

void FeedService::ScheduleDaily() {
  // Re-check once per day while the browser is running.
  CefPostDelayedTask(TID_UI, new FuncTask([]() {
    FeedService& fs = FeedService::Get();
    fs.Refresh();
    fs.ScheduleDaily();
  }), static_cast<int64_t>(kDaySeconds) * 1000);
}

void FeedService::Refresh() {
  if (pending_ > 0) return;  // a refresh is already in flight
  FetchAll();
}

void FeedService::FetchAll() {
  fetching_ = nlohmann::json::array();
  pending_ = static_cast<int>(sizeof(kFeeds) / sizeof(kFeeds[0]));
  for (const auto& f : kFeeds) {
    CefRefPtr<CefRequest> request = CefRequest::Create();
    request->SetURL(f.url);
    request->SetMethod("GET");
    request->SetFlags(UR_FLAG_SKIP_CACHE);
    CefRequest::HeaderMap headers;
    headers.insert(std::make_pair("Accept",
                                  "application/rss+xml, application/xml, text/xml"));
    request->SetHeaderMap(headers);
    CefRefPtr<FeedRequestClient> client = new FeedRequestClient(f.name, f.url);
    CefURLRequest::Create(request, client.get(), nullptr);
  }
}

void FeedService::OnFeedDone(const std::string& name, const std::string& url,
                             const std::string& xml) {
  fetching_.push_back({{"name", name}, {"url", url}, {"xml", xml},
                       {"ts", static_cast<long>(std::time(nullptr))}});
  if (--pending_ > 0) return;

  // All feeds returned - commit the batch.
  cache_ = {{"lastFetch", static_cast<long>(std::time(nullptr))},
            {"feeds", fetching_}};
  SaveCache();
  if (emit_) emit_("feeds.updated", {{"lastFetch", cache_["lastFetch"]}});
}

}  // namespace intel
}  // namespace nyx
