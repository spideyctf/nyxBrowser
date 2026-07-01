// Daily cybersecurity feed aggregator. Fetches a curated set of security RSS/
// Atom feeds through Chromium's network stack (CefURLRequest, no CORS limits),
// caches the raw payloads to data/feeds_cache.json, refreshes automatically once
// per day (and on startup if stale), and emits "feeds.updated" to the UI.
//
// Raw feed XML is handed to the dashboard, which parses it with the browser's
// own DOMParser - robust, and no XML dependency in C++.
#ifndef NYX_INTEL_FEED_SERVICE_H_
#define NYX_INTEL_FEED_SERVICE_H_

#include <functional>
#include <string>

#include "nlohmann/json.hpp"

namespace nyx {
namespace intel {

class FeedService {
 public:
  using Emitter =
      std::function<void(const std::string& event, const nlohmann::json&)>;

  static FeedService& Get();

  void SetEmitter(Emitter emitter) { emit_ = std::move(emitter); }

  // Load cache, refresh if older than a day, then schedule a daily refresh.
  void Start();
  // Force an immediate refresh (async; emits "feeds.updated" when done).
  void Refresh();
  // Cached payload: { "lastFetch": <epoch>, "feeds": [ {name,url,xml,ts}, ... ] }.
  nlohmann::json Cached();

  // Called internally by request callbacks.
  void OnFeedDone(const std::string& name, const std::string& url,
                  const std::string& xml);
  void ScheduleDaily();

 private:
  FeedService();

  void FetchAll();
  bool Stale() const;
  void LoadCache();
  void SaveCache();

  Emitter emit_;
  nlohmann::json cache_;
  nlohmann::json fetching_;   // accumulates during an in-flight refresh
  int pending_ = 0;
  bool loaded_ = false;
  bool started_ = false;
};

}  // namespace intel
}  // namespace nyx

#endif  // NYX_INTEL_FEED_SERVICE_H_
