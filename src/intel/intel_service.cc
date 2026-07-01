#include "intel/intel_service.h"

#include "include/cef_task.h"
#include "include/wrapper/cef_helpers.h"

#include <fstream>

#include "intel/ioc_extractor.h"
#include "intel/sha256.h"
#include "paths.h"

namespace nyx {
namespace intel {

namespace {

constexpr int kRateMax = 4;         // requests
constexpr int kRateWindow = 60;     // seconds
constexpr long kCacheTtl = 86400;   // 24h

// Run an arbitrary function as a CefTask (used for rate-limit deferral).
class FuncTask : public CefTask {
 public:
  explicit FuncTask(std::function<void()> fn) : fn_(std::move(fn)) {}
  void Execute() override { if (fn_) fn_(); }
 private:
  std::function<void()> fn_;
  IMPLEMENT_REFCOUNTING(FuncTask);
  DISALLOW_COPY_AND_ASSIGN(FuncTask);
};

std::string HostOf(const std::string& url) {
  size_t p = url.find("://");
  size_t start = (p == std::string::npos) ? 0 : p + 3;
  size_t end = url.find_first_of("/:?#", start);
  std::string host = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
  // Strip credentials if present (user:pass@host).
  size_t at = host.find('@');
  if (at != std::string::npos) host = host.substr(at + 1);
  return host;
}

nlohmann::json IocsToJson(const IocSet& s) {
  return {{"ipv4", s.ipv4},   {"domains", s.domains}, {"urls", s.urls},
          {"emails", s.emails}, {"md5", s.md5},       {"sha1", s.sha1},
          {"sha256", s.sha256}, {"cves", s.cves},     {"count", s.Count()}};
}

}  // namespace

IntelService::IntelService() = default;

IntelService& IntelService::Get() {
  static IntelService instance;
  return instance;
}

void IntelService::Configure(const std::string& api_key, bool enabled) {
  vt_.SetApiKey(api_key);
  enabled_ = enabled;
  if (!cache_loaded_) LoadCache();
}

void IntelService::Emit(const std::string& event, const nlohmann::json& p) {
  if (emit_) emit_(event, p);
}

// ---------------- tap points ----------------
void IntelService::ScanNavigation(const std::string& url) {
  if (!enabled_ || url.empty() || url.rfind("nyx://", 0) == 0) return;
  IocSet iocs = ExtractIocs(url);
  Emit("intel.iocs", {{"source", url}, {"iocs", IocsToJson(iocs)}});

  std::string host = HostOf(url);
  if (!host.empty()) QueryDomain(host, url);
}

void IntelService::ScanText(const std::string& text, const std::string& source) {
  if (!enabled_ || text.empty()) return;
  IocSet iocs = ExtractIocs(text);
  if (iocs.Empty()) return;
  Emit("intel.iocs", {{"source", source}, {"iocs", IocsToJson(iocs)}});
  // Opportunistically check any file hashes found (rate permitting).
  for (const auto& h : iocs.sha256) QueryFile(h, source);
}

void IntelService::ScanDownload(const std::string& file_path,
                                const std::string& url) {
  if (!enabled_) return;
  const std::string sha = Sha256::HashFile(file_path);
  if (sha.empty()) return;
  Emit("intel.iocs",
       {{"source", url}, {"iocs", {{"sha256", {sha}}, {"count", 1}}}});
  QueryFile(sha, file_path);
}

// ---------------- VT queries (cached + rate-limited) ----------------
void IntelService::QueryDomain(const std::string& domain,
                               const std::string& source) {
  const std::string key = "domain:" + domain;
  VtStats cached;
  if (CacheGet(key, &cached)) {
    VtResult r; r.ok = true; r.kind = "domain"; r.resource = domain; r.stats = cached;
    EmitVerdict(r, source);
    return;
  }
  if (!vt_.HasKey()) return;
  if (!RateAllow()) {
    CefPostDelayedTask(TID_UI, new FuncTask([this, domain, source]() {
      QueryDomain(domain, source);
    }), 2000);
    return;
  }
  vt_.LookupDomain(domain, [this, key, source](const VtResult& r) {
    if (r.ok) CachePut(key, r.stats);
    EmitVerdict(r, source);
  });
}

void IntelService::QueryFile(const std::string& sha256,
                             const std::string& filename) {
  const std::string key = "file:" + sha256;
  VtStats cached;
  if (CacheGet(key, &cached)) {
    VtResult r; r.ok = true; r.kind = "file"; r.resource = sha256; r.stats = cached;
    EmitVerdict(r, filename);
    return;
  }
  if (!vt_.HasKey()) return;
  if (!RateAllow()) {
    CefPostDelayedTask(TID_UI, new FuncTask([this, sha256, filename]() {
      QueryFile(sha256, filename);
    }), 2000);
    return;
  }
  vt_.LookupFile(sha256, [this, key, filename](const VtResult& r) {
    if (r.ok) CachePut(key, r.stats);
    EmitVerdict(r, filename);
  });
}

// ---------------- rate limit ----------------
bool IntelService::RateAllow() {
  std::time_t now = std::time(nullptr);
  while (!hits_.empty() && now - hits_.front() >= kRateWindow) hits_.pop_front();
  if (static_cast<int>(hits_.size()) >= kRateMax) return false;
  hits_.push_back(now);
  return true;
}

// ---------------- cache ----------------
std::string CachePath() { return Join(DataDir(), "intel_cache.json"); }

void IntelService::LoadCache() {
  cache_loaded_ = true;
  std::ifstream in(CachePath(), std::ios::binary);
  if (!in.good()) { cache_ = nlohmann::json::object(); return; }
  auto j = nlohmann::json::parse(in, nullptr, false);
  cache_ = j.is_discarded() ? nlohmann::json::object() : j;
}

void IntelService::SaveCache() {
  std::ofstream out(CachePath(), std::ios::binary | std::ios::trunc);
  if (out.good()) out << cache_.dump();
}

bool IntelService::CacheGet(const std::string& key, VtStats* out) {
  if (!cache_loaded_) LoadCache();
  if (!cache_.contains(key)) return false;
  const auto& e = cache_[key];
  long ts = e.value("ts", 0L);
  if (std::time(nullptr) - ts > kCacheTtl) return false;  // stale
  out->malicious = e.value("m", 0);
  out->suspicious = e.value("s", 0);
  out->harmless = e.value("h", 0);
  out->undetected = e.value("u", 0);
  out->timeout = e.value("t", 0);
  return true;
}

void IntelService::CachePut(const std::string& key, const VtStats& s) {
  if (!cache_loaded_) LoadCache();
  cache_[key] = {{"m", s.malicious}, {"s", s.suspicious}, {"h", s.harmless},
                 {"u", s.undetected}, {"t", s.timeout},
                 {"ts", static_cast<long>(std::time(nullptr))}};
  SaveCache();
}

// ---------------- verdict ----------------
std::string IntelService::Verdict(const VtStats& s) {
  if (s.malicious >= 1) return "malicious";
  if (s.suspicious >= 1) return "suspicious";
  if (s.harmless + s.undetected > 0) return "clean";
  return "unknown";
}

void IntelService::EmitVerdict(const VtResult& r, const std::string& source) {
  nlohmann::json payload = {
      {"kind", r.kind},
      {"resource", r.resource},
      {"source", source},
      {"ok", r.ok},
      {"verdict", r.ok ? Verdict(r.stats) : "unknown"},
      {"error", r.error},
      {"stats", {{"malicious", r.stats.malicious},
                 {"suspicious", r.stats.suspicious},
                 {"harmless", r.stats.harmless},
                 {"undetected", r.stats.undetected},
                 {"timeout", r.stats.timeout}}}};
  Emit("intel.verdict", payload);
}

}  // namespace intel
}  // namespace nyx
