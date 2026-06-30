#include "scheme_handler.h"

#include <algorithm>
#include <fstream>
#include <string>

#include "include/cef_parser.h"
#include "include/cef_resource_handler.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_helpers.h"

#include "paths.h"

namespace nyx {

namespace {

const char kScheme[] = "nyx";

std::string MimeForExt(const std::string& path) {
  auto ends = [&](const char* s) {
    const std::string suf(s);
    return path.size() >= suf.size() &&
           path.compare(path.size() - suf.size(), suf.size(), suf) == 0;
  };
  if (ends(".html") || ends(".htm")) return "text/html";
  if (ends(".css")) return "text/css";
  if (ends(".js")) return "application/javascript";
  if (ends(".json")) return "application/json";
  if (ends(".svg")) return "image/svg+xml";
  if (ends(".png")) return "image/png";
  if (ends(".woff2")) return "font/woff2";
  if (ends(".ico")) return "image/x-icon";
  return "application/octet-stream";
}

// Maps a nyx://ui/... URL to a file under <ExeDir>/ui and serves its bytes.
class UiResourceHandler : public CefResourceHandler {
 public:
  UiResourceHandler() = default;

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    handle_request = true;  // handled synchronously from local disk

    CefURLParts parts;
    CefParseURL(request->GetURL(), parts);
    std::string path = CefString(&parts.path).ToString();  // e.g. "/index.html"
    if (path.empty() || path == "/") path = "/index.html";

    // Prevent directory traversal.
    if (path.find("..") != std::string::npos) return false;

    std::string file = UiDir();
    for (char c : path) file.push_back(c == '/' ? '\\' : c);

    std::ifstream in(file, std::ios::binary);
    if (!in.good()) return false;

    data_.assign((std::istreambuf_iterator<char>(in)),
                 std::istreambuf_iterator<char>());
    mime_ = MimeForExt(path);
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    response->SetMimeType(mime_);
    response->SetStatus(200);
    CefResponse::HeaderMap headers;
    headers.insert(std::make_pair("Cache-Control", "no-cache"));
    response->SetHeaderMap(headers);
    response_length = static_cast<int64_t>(data_.size());
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    bytes_read = 0;
    if (offset_ >= data_.size()) return false;  // done
    const size_t remaining = data_.size() - offset_;
    const size_t n =
        std::min(static_cast<size_t>(bytes_to_read), remaining);
    memcpy(data_out, data_.data() + offset_, n);
    offset_ += n;
    bytes_read = static_cast<int>(n);
    return true;
  }

  void Cancel() override {}

 private:
  std::string data_;
  std::string mime_;
  size_t offset_ = 0;

  IMPLEMENT_REFCOUNTING(UiResourceHandler);
  DISALLOW_COPY_AND_ASSIGN(UiResourceHandler);
};

class UiSchemeFactory : public CefSchemeHandlerFactory {
 public:
  UiSchemeFactory() = default;

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    return new UiResourceHandler();
  }

  IMPLEMENT_REFCOUNTING(UiSchemeFactory);
  DISALLOW_COPY_AND_ASSIGN(UiSchemeFactory);
};

}  // namespace

void RegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
  // standard + secure + CORS-enabled so the UI behaves like a normal origin.
  registrar->AddCustomScheme(
      kScheme, CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE |
                   CEF_SCHEME_OPTION_CORS_ENABLED |
                   CEF_SCHEME_OPTION_FETCH_ENABLED);
}

void RegisterSchemeHandlerFactory() {
  CefRegisterSchemeHandlerFactory(kScheme, "ui", new UiSchemeFactory());
}

}  // namespace nyx
