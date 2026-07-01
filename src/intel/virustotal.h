// VirusTotal API v3 client. Uses Chromium's own network stack via CefURLRequest
// (no extra HTTP dependency, and it honors the browser's proxy/DoH config).
// All lookups are asynchronous; results are delivered on the CEF UI thread.
#ifndef NYX_INTEL_VIRUSTOTAL_H_
#define NYX_INTEL_VIRUSTOTAL_H_

#include <functional>
#include <string>

namespace nyx {
namespace intel {

struct VtStats {
  int malicious = 0;
  int suspicious = 0;
  int harmless = 0;
  int undetected = 0;
  int timeout = 0;
  int Total() const { return malicious + suspicious + harmless + undetected + timeout; }
};

struct VtResult {
  bool ok = false;            // true when VT returned analysis stats
  std::string kind;           // "file" | "url" | "domain"
  std::string resource;       // hash / url / domain queried
  VtStats stats;
  int http_status = 0;
  std::string error;          // populated when ok == false
};

using VtCallback = std::function<void(const VtResult&)>;

class VtClient {
 public:
  void SetApiKey(const std::string& key) { api_key_ = key; }
  bool HasKey() const { return !api_key_.empty(); }

  void LookupFile(const std::string& sha256, VtCallback cb);
  void LookupUrl(const std::string& url, VtCallback cb);
  void LookupDomain(const std::string& domain, VtCallback cb);

 private:
  void Send(const std::string& endpoint,
            const std::string& kind,
            const std::string& resource,
            VtCallback cb);

  std::string api_key_;
};

}  // namespace intel
}  // namespace nyx

#endif  // NYX_INTEL_VIRUSTOTAL_H_
