// IOC (Indicator of Compromise) extraction from untrusted text.
// Dependency-free (uses only the C++ standard library). Refangs common
// obfuscation (hxxp, [.], (dot), [:]) before matching so defanged threat-intel
// text is handled correctly. Results are de-duplicated and order-stable.
#ifndef NYX_INTEL_IOC_EXTRACTOR_H_
#define NYX_INTEL_IOC_EXTRACTOR_H_

#include <string>
#include <vector>

namespace nyx {
namespace intel {

struct IocSet {
  std::vector<std::string> ipv4;
  std::vector<std::string> domains;
  std::vector<std::string> urls;
  std::vector<std::string> emails;
  std::vector<std::string> md5;
  std::vector<std::string> sha1;
  std::vector<std::string> sha256;
  std::vector<std::string> cves;

  bool Empty() const {
    return ipv4.empty() && domains.empty() && urls.empty() && emails.empty() &&
           md5.empty() && sha1.empty() && sha256.empty() && cves.empty();
  }
  size_t Count() const {
    return ipv4.size() + domains.size() + urls.size() + emails.size() +
           md5.size() + sha1.size() + sha256.size() + cves.size();
  }
};

// Undo common defanging so obfuscated indicators still match.
std::string Refang(const std::string& text);

// Extract all supported indicators from arbitrary text.
IocSet ExtractIocs(const std::string& text);

}  // namespace intel
}  // namespace nyx

#endif  // NYX_INTEL_IOC_EXTRACTOR_H_
