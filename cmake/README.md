# cmake/

nyx relies on the CMake helpers shipped *inside* the CEF binary distribution
(`third_party/cef/cmake/`), which `find_package(CEF)` loads. That distribution
provides:

- `cef_variables.cmake` - compiler/linker flags, `CEF_STANDARD_LIBS`,
  `CEF_BINARY_FILES`, `CEF_RESOURCE_FILES`.
- `cef_macros.cmake` - `SET_EXECUTABLE_TARGET_PROPERTIES`, `COPY_FILES`, etc.
- `FindCEF.cmake` - the package entry point.

No project-local CMake modules are required. This directory is a placeholder for
future custom modules (e.g. code-signing, packaging targets).
