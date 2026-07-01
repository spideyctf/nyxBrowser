// Base64url encoding without padding. VirusTotal's URL endpoint identifies a
// URL by base64url(url) with trailing '=' stripped.
#ifndef NYX_INTEL_BASE64URL_H_
#define NYX_INTEL_BASE64URL_H_

#include <string>

namespace nyx {
namespace intel {

std::string Base64UrlNoPad(const std::string& input);

}  // namespace intel
}  // namespace nyx

#endif  // NYX_INTEL_BASE64URL_H_
