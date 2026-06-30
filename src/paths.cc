#include "paths.h"

#include <windows.h>

#include <direct.h>
#include <sys/stat.h>

namespace nyx {

namespace {

std::string DirNameOf(const std::string& path) {
  const size_t pos = path.find_last_of("\\/");
  return (pos == std::string::npos) ? std::string() : path.substr(0, pos);
}

void EnsureDir(const std::string& path) {
  // Create each component of the path if it does not yet exist.
  std::string acc;
  for (size_t i = 0; i < path.size(); ++i) {
    acc.push_back(path[i]);
    if (path[i] == '\\' || path[i] == '/' || i + 1 == path.size()) {
      if (acc.size() > 2) {  // skip drive root like "C:"
        _mkdir(acc.c_str());
      }
    }
  }
}

}  // namespace

std::string Join(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  char last = a[a.size() - 1];
  if (last == '\\' || last == '/') return a + b;
  return a + "\\" + b;
}

std::string ExeDir() {
  char buffer[MAX_PATH] = {0};
  GetModuleFileNameA(nullptr, buffer, MAX_PATH);
  return DirNameOf(std::string(buffer));
}

std::string DataDir() {
  std::string dir = Join(ExeDir(), "data");
  EnsureDir(dir);
  return dir;
}

std::string DataSubDir(const std::string& sub) {
  std::string dir = Join(DataDir(), sub);
  EnsureDir(dir);
  return dir;
}

std::string UiDir() { return Join(ExeDir(), "ui"); }

std::string ConfigDir() { return Join(ExeDir(), "config"); }

}  // namespace nyx
