// Portable JSON-backed storage for bookmarks, history, downloads, settings,
// and session. All files live under <ExeDir>/data. Accessed on the UI thread.
#ifndef NYX_STORAGE_H_
#define NYX_STORAGE_H_

#include <string>

#include "nlohmann/json.hpp"

namespace nyx {

class Storage {
 public:
  static Storage& Get();

  // ---- Bookmarks ----
  nlohmann::json Bookmarks();
  void AddBookmark(const std::string& url, const std::string& title);
  void RemoveBookmark(const std::string& url);

  // ---- History ----
  nlohmann::json History();                 // newest first
  void AddHistory(const std::string& url, const std::string& title);
  void ClearHistory();

  // ---- Downloads ----
  nlohmann::json Downloads();
  void UpsertDownload(const nlohmann::json& item);  // keyed by "id"

  // ---- Settings ----
  nlohmann::json Settings();
  void SetSetting(const std::string& key, const nlohmann::json& value);

  // ---- Session (open tabs) ----
  nlohmann::json Session();
  void SaveSession(const nlohmann::json& tabs);

 private:
  Storage() = default;

  nlohmann::json Load(const std::string& file, const nlohmann::json& fallback);
  void Save(const std::string& file, const nlohmann::json& data);
  std::string PathFor(const std::string& file);
};

}  // namespace nyx

#endif  // NYX_STORAGE_H_
