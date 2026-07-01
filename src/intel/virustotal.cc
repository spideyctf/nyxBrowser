#include "intel/virustotal.h"

#include "include/cef_request.h"
#include "include/cef_response.h"
#include "include/cef_urlrequest.h"
#include "include/wrapper/cef_helpers.h"
#include "nlohmann/json.hpp"

#include "intel/base64url.h"

namespace nyx {
namespace intel {

namespace {

const char kApiBase[] = "https://www.virustotal.com/api/v3/";

// One instance per in-flight request; CEF keeps it alive for the request's
// lifetime. Accumulates the response body and parses last_analysis_stats.
class VtRequestClient : public CefURLRequestClient {
 public:
  VtRequestClient(const std::string& kind,
                  const std::string& resource,
                  VtCallback cb)
      : kind_(kind), resource_(resource), cb_(std::move(cb)) {}

  void OnRequestComplete(CefRefPtr<CefURLRequest> request) override {
    CEF_REQUIRE_UI_THREAD();
    VtResult result;
    result.kind = kind_;
    result.resource = resource_;

    CefRefPtr<CefResponse> response = request->GetResponse();
    result.http_status = response ? response->GetStatus() : 0;

    if (request->GetRequestStatus() != UR_SUCCESS) {
      result.error = "network_error";
      if (cb_) cb_(result);
      return;
    }
    if (result.http_status == 404) {
      result.error = "not_found";  // no VT record for this resource
      if (cb_) cb_(result);
      return;
    }
    if (result.http_status != 200) {
      result.error = "http_" + std::to_string(result.http_status);
      if (cb_) cb_(result);
      return;
    }

    auto j = nlohmann::json::parse(body_, nullptr, /*allow_exceptions=*/false);
    if (!j.is_discarded() && j.contains("data")) {
      const auto& attr = j["data"].value("attributes", nlohmann::json::object());
      const auto& s =
          attr.value("last_analysis_stats", nlohmann::json::object());
      result.stats.malicious = s.value("malicious", 0);
      result.stats.suspicious = s.value("suspicious", 0);
      result.stats.harmless = s.value("harmless", 0);
      result.stats.undetected = s.value("undetected", 0);
      result.stats.timeout = s.value("timeout", 0);
      result.ok = true;
    } else {
      result.error = "parse_error";
    }
    if (cb_) cb_(result);
  }

  void OnDownloadData(CefRefPtr<CefURLRequest> request,
                      const void* data,
                      size_t data_length) override {
    body_.append(static_cast<const char*>(data), data_length);
  }

  void OnUploadProgress(CefRefPtr<CefURLRequest>, int64_t, int64_t) override {}
  void OnDownloadProgress(CefRefPtr<CefURLRequest>, int64_t, int64_t) override {}
  bool GetAuthCredentials(bool, const CefString&, int, const CefString&,
                          const CefString&,
                          CefRefPtr<CefAuthCallback>) override {
    return false;
  }

 private:
  std::string kind_;
  std::string resource_;
  VtCallback cb_;
  std::string body_;

  IMPLEMENT_REFCOUNTING(VtRequestClient);
  DISALLOW_COPY_AND_ASSIGN(VtRequestClient);
};

}  // namespace

void VtClient::Send(const std::string& endpoint,
                    const std::string& kind,
                    const std::string& resource,
                    VtCallback cb) {
  if (!HasKey()) {
    VtResult r;
    r.kind = kind;
    r.resource = resource;
    r.error = "no_api_key";
    if (cb) cb(r);
    return;
  }

  CefRefPtr<CefRequest> request = CefRequest::Create();
  request->SetURL(endpoint);
  request->SetMethod("GET");
  CefRequest::HeaderMap headers;
  headers.insert(std::make_pair("x-apikey", api_key_));
  headers.insert(std::make_pair("Accept", "application/json"));
  request->SetHeaderMap(headers);
  // Do not send cookies / cache this API traffic.
  request->SetFlags(UR_FLAG_NO_RETRY_ON_5XX | UR_FLAG_SKIP_CACHE);

  CefRefPtr<VtRequestClient> client =
      new VtRequestClient(kind, resource, std::move(cb));
  CefURLRequest::Create(request, client.get(), nullptr);
}

void VtClient::LookupFile(const std::string& sha256, VtCallback cb) {
  Send(std::string(kApiBase) + "files/" + sha256, "file", sha256, std::move(cb));
}

void VtClient::LookupDomain(const std::string& domain, VtCallback cb) {
  Send(std::string(kApiBase) + "domains/" + domain, "domain", domain,
       std::move(cb));
}

void VtClient::LookupUrl(const std::string& url, VtCallback cb) {
  Send(std::string(kApiBase) + "urls/" + Base64UrlNoPad(url), "url", url,
       std::move(cb));
}

}  // namespace intel
}  // namespace nyx
