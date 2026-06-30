#include "storage.h"

#include <fstream>
#include <mutex>

#include "paths.h"

namespace nyx {

namespace {
std::mutex g_mutex;
constexpr size_t kMaxHistory = 5000;
}  // namespace

Storage& Storage::Get() {
  static Storage instance;
  return instance;
}

std::string Storage::PathFor(const std::string& file) {
  return Join(DataDir(), file);
}

nlohmann::json Storage::Load(const std::string& file,
                             const nlohmann::json& fallback) {
  std::ifstream in(PathFor(file), std::ios::binary);
  if (!in.good()) return fallback;
  try {
    nlohmann::json j;
    in >> j;
    return j;
  } catch (...) {
    return fallback;
  }
}

void Storage::Save(const std::string& file, const nlohmann::json& data) {
  std::ofstream out(PathFor(file), std::ios::binary | std::ios::trunc);
  if (out.good()) out << data.dump(2);
}

// ---------------- Bookmarks ----------------
nlohmann::json Storage::Bookmarks() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return Load("bookmarks.json", nlohmann::json::array());
}

void Storage::AddBookmark(const std::string& url, const std::string& title) {
  std::lock_guard<std::mutex> lock(g_mutex);
  nlohmann::json arr = Load("bookmarks.json", nlohmann::json::array());
  for (const auto& b : arr) {
    if (b.value("url", "") == url) return;  // already bookmarked
  }
  arr.push_back({{"url", url}, {"title", title}});
  Save("bookmarks.json", arr);
}

void Storage::RemoveBookmark(const std::string& url) {
  std::lock_guard<std::mutex> lock(g_mutex);
  nlohmann::json arr = Load("bookmarks.json", nlohmann::json::array());
  nlohmann::json out = nlohmann::json::array();
  for (const auto& b : arr) {
    if (b.value("url", "") != url) out.push_back(b);
  }
  Save("bookmarks.json", out);
}

// ---------------- History ----------------
nlohmann::json Storage::History() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return Load("history.json", nlohmann::json::array());
}

void Storage::AddHistory(const std::string& url, const std::string& title) {
  if (url.empty() || url.rfind("nyx://", 0) == 0) return;
  std::lock_guard<std::mutex> lock(g_mutex);
  nlohmann::json arr = Load("history.json", nlohmann::json::array());
  nlohmann::json entry = {{"url", url}, {"title", title},
                          {"ts", static_cast<long long>(time(nullptr))}};
  arr.insert(arr.begin(), entry);  // newest first
  if (arr.size() > kMaxHistory) arr.erase(arr.begin() + kMaxHistory, arr.end());
  Save("history.json", arr);
}

void Storage::ClearHistory() {
  std::lock_guard<std::mutex> lock(g_mutex);
  Save("history.json", nlohmann::json::array());
}

// ---------------- Downloads ----------------
nlohmann::json Storage::Downloads() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return Load("downloads.json", nlohmann::json::array());
}

void Storage::UpsertDownload(const nlohmann::json& item) {
  std::lock_guard<std::mutex> lock(g_mutex);
  nlohmann::json arr = Load("downloads.json", nlohmann::json::array());
  const auto id = item.value("id", 0);
  bool found = false;
  for (auto& d : arr) {
    if (d.value("id", -1) == id) { d = item; found = true; break; }
  }
  if (!found) arr.insert(arr.begin(), item);
  Save("downloads.json", arr);
}

// ---------------- Settings ----------------
nlohmann::json Storage::Settings() {
  std::lock_guard<std::mutex> lock(g_mutex);
  nlohmann::json defaults = {
      {"homepage", "https://github.com"},
      {"searchEngine", "https://www.google.com/search?q="},
      {"restoreSession", true},
      {"doNotTrack", true},
      {"blockThirdPartyCookies", true},
      {"trimReferrer", true},
      {"theme", "dark"}};
  nlohmann::json saved = Load("settings.json", nlohmann::json::object());
  for (auto it = saved.begin(); it != saved.end(); ++it) {
    defaults[it.key()] = it.value();
  }
  return defaults;
}

void Storage::SetSetting(const std::string& key, const nlohmann::json& value) {
  std::lock_guard<std::mutex> lock(g_mutex);
  nlohmann::json saved = Load("settings.json", nlohmann::json::object());
  saved[key] = value;
  Save("settings.json", saved);
}

// ---------------- Session ----------------
nlohmann::json Storage::Session() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return Load("session.json", nlohmann::json::array());
}

void Storage::SaveSession(const nlohmann::json& tabs) {
  std::lock_guard<std::mutex> lock(g_mutex);
  Save("session.json", tabs);
}

}  // namespace nyx
