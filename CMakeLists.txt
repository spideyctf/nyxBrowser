# nyx Browser - top-level build
cmake_minimum_required(VERSION 3.21)

project(nyx_browser CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Locate the CEF binary distribution (fetched by tools/fetch_cef.ps1).
# ---------------------------------------------------------------------------
if(NOT DEFINED CEF_ROOT)
  set(CEF_ROOT "${CMAKE_SOURCE_DIR}/third_party/cef")
endif()

if(NOT EXISTS "${CEF_ROOT}/cmake/cef_variables.cmake")
  message(FATAL_ERROR
    "CEF not found at ${CEF_ROOT}. Run tools/fetch_cef.ps1 first.")
endif()

# CEF ships its own CMake helpers (sets compiler flags, defines macros,
# provides the libcef_dll_wrapper target, copy/binary helpers).
set(CMAKE_MODULE_PATH "${CEF_ROOT}/cmake" ${CMAKE_MODULE_PATH})
find_package(CEF REQUIRED)

# ---------------------------------------------------------------------------
# Fix SDK compatibility: the installed Windows SDK is 10.0.19041.0 which
# predates NTDDI_WIN10_FE. Replace with a value the SDK actually knows.
# ---------------------------------------------------------------------------
list(REMOVE_ITEM CEF_COMPILER_DEFINES "NTDDI_VERSION=NTDDI_WIN10_FE")
list(APPEND CEF_COMPILER_DEFINES "NTDDI_VERSION=0x0A000008")

# Build the bundled C++ wrapper that ships with CEF.
add_subdirectory("${CEF_ROOT}/libcef_dll" libcef_dll_wrapper)

# Create the imported target for the CEF library itself.
ADD_LOGICAL_TARGET(libcef_lib "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

# ---------------------------------------------------------------------------
# Third-party header-only deps (fetched by tools/fetch_deps.ps1).
# ---------------------------------------------------------------------------
set(JSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/json/include")

# ---------------------------------------------------------------------------
# nyx executable
# ---------------------------------------------------------------------------
set(NYX_SRCS
  src/main.cc
  src/nyx_app.cc
  src/nyx_app.h
  src/nyx_client.cc
  src/nyx_client.h
  src/nyx_window.cc
  src/nyx_window.h
  src/tab_manager.cc
  src/tab_manager.h
  src/message_bridge.cc
  src/message_bridge.h
  src/storage.cc
  src/storage.h
  src/scheme_handler.cc
  src/scheme_handler.h
  src/paths.cc
  src/paths.h
  src/resource.h
  src/intel/ioc_extractor.cc
  src/intel/ioc_extractor.h
  src/intel/sha256.cc
  src/intel/sha256.h
  src/intel/base64url.cc
  src/intel/base64url.h
  src/intel/virustotal.cc
  src/intel/virustotal.h
  src/intel/intel_service.cc
  src/intel/intel_service.h
  src/intel/feed_service.cc
  src/intel/feed_service.h
  src/win/nyx.rc
)

add_executable(nyx WIN32 ${NYX_SRCS})

SET_EXECUTABLE_TARGET_PROPERTIES(nyx)

# nlohmann/json requires exceptions; re-enable /EHsc for nyx sources.
target_compile_options(nyx PRIVATE /EHsc)

add_dependencies(nyx libcef_dll_wrapper)

target_include_directories(nyx PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${JSON_INCLUDE_DIR}"
  "${CEF_ROOT}"
)

target_link_libraries(nyx PRIVATE
  libcef_lib
  libcef_dll_wrapper
  ${CEF_STANDARD_LIBS}
)

# Use the embedded application manifest (DPI awareness, common controls).
target_sources(nyx PRIVATE src/win/nyx.exe.manifest)

# ---------------------------------------------------------------------------
# Copy the CEF runtime (DLLs, .pak, .bin, locales) next to nyx.exe.
# ---------------------------------------------------------------------------
COPY_FILES(nyx "${CEF_BINARY_FILES}"   "${CEF_BINARY_DIR}"   "$<TARGET_FILE_DIR:nyx>")
COPY_FILES(nyx "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:nyx>")

# Copy the UI assets and default config next to the executable for dev runs.
add_custom_command(TARGET nyx POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/ui"     "$<TARGET_FILE_DIR:nyx>/ui"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/config" "$<TARGET_FILE_DIR:nyx>/config"
  VERBATIM)
