// Portable path resolution. Everything is relative to the executable folder.
#ifndef NYX_PATHS_H_
#define NYX_PATHS_H_

#include <string>

namespace nyx {

// Absolute directory that contains nyx.exe.
std::string ExeDir();

// <ExeDir>/data  (all mutable state lives here). Created if missing.
std::string DataDir();

// <ExeDir>/data/<sub>  (created if missing).
std::string DataSubDir(const std::string& sub);

// <ExeDir>/ui  (the dark UI assets).
std::string UiDir();

// <ExeDir>/config
std::string ConfigDir();

// Join two path fragments with the platform separator.
std::string Join(const std::string& a, const std::string& b);

}  // namespace nyx

#endif  // NYX_PATHS_H_
