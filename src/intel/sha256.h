// Minimal, dependency-free SHA-256 (public-domain style implementation).
// Used to hash downloaded files for VirusTotal lookups.
#ifndef NYX_INTEL_SHA256_H_
#define NYX_INTEL_SHA256_H_

#include <cstdint>
#include <string>

namespace nyx {
namespace intel {

// Streaming SHA-256.
class Sha256 {
 public:
  Sha256();
  void Update(const void* data, size_t len);
  std::string HexDigest();  // finalizes; lowercase 64-char hex

  // Convenience: hash an entire file. Returns "" on read failure.
  static std::string HashFile(const std::string& path);
  // Convenience: hash a buffer.
  static std::string HashBytes(const void* data, size_t len);

 private:
  void Transform(const uint8_t* chunk);

  uint32_t state_[8];
  uint64_t bitlen_ = 0;
  uint8_t buffer_[64];
  size_t buflen_ = 0;
};

}  // namespace intel
}  // namespace nyx

#endif  // NYX_INTEL_SHA256_H_
