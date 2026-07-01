// Threat-intelligence orchestrator. Coordinates IOC extraction and VirusTotal
// lookups, caches verdicts with a TTL, respects VT's free-tier rate limit, and
// emits results to the UI through a caller-supplied emitter.
//
// Designed to be driven from the network/download tap points in nyx_client.cc:
//   IntelService::Get().ScanNavigation(url);
//   IntelService::Get().ScanDownload(path, url);
//   IntelService::Get().ScanText(bodyText, sourceUrl);
// It is inert until Configure(apiKey, enabled=true) is called.
#ifndef NYX_INTEL_INTEL_SERVICE_H_
#define NYX_INTEL_INTEL_SERVICE_H_

#include <ctime>
#include <deque>
#include <functional>
#include <string>

#include "nlohmann/json.hpp"

#include "intel/virustotal.h"

namespace nyx {
namespace intel {

class IntelService {
 public:
  using Emitter =
      std::function<void(const std::string& event, const nlohmann::json&)>;

  static IntelService& Get();

  // Enable/disable and set the VT API key. Safe to call repeatedly.
  void Configure(const std::string& api_key, bool enabled);
  // Where verdicts/IOCs are pushed (e.g. the UI). Optional.
  void SetEmitter(Emitter emitter) { emit_ = std::move(emitter); }
  bool enabled() const { return enabled_; }

  // Tap points.
  void ScanNavigation(const std::string& url);
  void ScanText(const std::string& text, const std::string& source);
  void ScanDownload(const std::string& file_path, const std::string& url);

 private:
  IntelService();

  void QueryDomain(const std::string& domain, const std::string& source);
  void QueryFile(const std::string& sha256, const std::string& filename);

  bool RateAllow();               // VT free tier: 4 requests / 60s
  bool CacheGet(const std::string& key, VtStats* out);
  void CachePut(const std::string& key, const VtStats& stats);
  void LoadCache();
  void SaveCache();

  static std::string Verdict(const VtStats& s);
  void EmitVerdict(const VtResult& r, const std::string& source);
  void Emit(const std::string& event, const nlohmann::json& payload);

  VtClient vt_;
  bool enabled_ = false;
  Emitter emit_;
  nlohmann::json cache_;          // key -> {m,s,h,u,t,ts}
  std::deque<std::time_t> hits_;  // rate-limit timestamps
  bool cache_loaded_ = false;
};

}  // namespace intel
}  // namespace nyx

#endif  // NYX_INTEL_INTEL_SERVICE_H_
