#include "intel/ioc_extractor.h"

#include <algorithm>
#include <regex>
#include <set>
#include <unordered_set>

namespace nyx {
namespace intel {

namespace {

// Replace every occurrence of `from` with `to` (case-sensitive).
void ReplaceAll(std::string& s, const std::string& from, const std::string& to) {
  if (from.empty()) return;
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

// Collect unique matches (preserving first-seen order) for a pattern.
std::vector<std::string> MatchAll(const std::string& text,
                                  const std::regex& re,
                                  bool lower_for_dedup) {
  std::vector<std::string> out;
  std::unordered_set<std::string> seen;
  auto begin = std::sregex_iterator(text.begin(), text.end(), re);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    std::string m = it->str();
    std::string key = lower_for_dedup ? ToLower(m) : m;
    if (seen.insert(key).second) out.push_back(m);
  }
  return out;
}

// Common non-IOC domains we don't want to flag on every page load.
bool IsNoiseDomain(const std::string& d) {
  static const std::set<std::string> tlds_only = {};
  std::string dl = ToLower(d);
  // Filenames like "index.html" or "app.js" match the domain pattern; drop
  // anything whose final label is a known file extension.
  static const std::set<std::string> ext = {
      "html", "htm", "js", "css", "png", "jpg", "jpeg", "gif", "svg", "json",
      "php", "asp", "aspx", "xml", "txt", "woff", "woff2", "ico", "map", "ts",
      "md", "py", "exe", "dll", "zip"};
  size_t dot = dl.find_last_of('.');
  if (dot != std::string::npos && ext.count(dl.substr(dot + 1))) return true;
  return false;
}

}  // namespace

std::string Refang(const std::string& in) {
  std::string s = in;
  ReplaceAll(s, "[.]", ".");
  ReplaceAll(s, "(.)", ".");
  ReplaceAll(s, "{.}", ".");
  ReplaceAll(s, "[dot]", ".");
  ReplaceAll(s, "(dot)", ".");
  ReplaceAll(s, " dot ", ".");
  ReplaceAll(s, "[:]", ":");
  ReplaceAll(s, "[://]", "://");
  ReplaceAll(s, "[@]", "@");
  ReplaceAll(s, "[at]", "@");
  ReplaceAll(s, "(at)", "@");
  ReplaceAll(s, "hxxps", "https");
  ReplaceAll(s, "hxxp", "http");
  ReplaceAll(s, "hXXp", "http");
  return s;
}

IocSet ExtractIocs(const std::string& raw) {
  const std::string text = Refang(raw);
  IocSet out;

  static const std::regex re_ipv4(
      R"(\b(?:25[0-5]|2[0-4]\d|[01]?\d?\d)(?:\.(?:25[0-5]|2[0-4]\d|[01]?\d?\d)){3}\b)");
  static const std::regex re_url(R"((https?|ftp)://[^\s"'<>\)\]]+)",
                                 std::regex::icase);
  static const std::regex re_email(
      R"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})");
  static const std::regex re_sha256(R"(\b[a-fA-F0-9]{64}\b)");
  static const std::regex re_sha1(R"(\b[a-fA-F0-9]{40}\b)");
  static const std::regex re_md5(R"(\b[a-fA-F0-9]{32}\b)");
  static const std::regex re_cve(R"(CVE-\d{4}-\d{4,7})", std::regex::icase);
  static const std::regex re_domain(
      R"(\b(?:[a-zA-Z0-9](?:[a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}\b)");

  out.ipv4 = MatchAll(text, re_ipv4, false);
  out.urls = MatchAll(text, re_url, true);
  out.emails = MatchAll(text, re_email, true);
  out.sha256 = MatchAll(text, re_sha256, true);
  out.sha1 = MatchAll(text, re_sha1, true);
  out.md5 = MatchAll(text, re_md5, true);
  out.cves = MatchAll(text, re_cve, true);

  // Domains: filter obvious file names and any host already inside an IPv4.
  std::unordered_set<std::string> ipset(out.ipv4.begin(), out.ipv4.end());
  for (const auto& d : MatchAll(text, re_domain, true)) {
    if (ipset.count(d)) continue;
    if (IsNoiseDomain(d)) continue;
    out.domains.push_back(d);
  }
  return out;
}

}  // namespace intel
}  // namespace nyx
