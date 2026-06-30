// Browser-side IPC handler. Receives JSON requests from the UI via the CEF
// message router (window.cefQuery) and dispatches them to TabManager + Storage.
#ifndef NYX_MESSAGE_BRIDGE_H_
#define NYX_MESSAGE_BRIDGE_H_

#include "include/wrapper/cef_message_router.h"

namespace nyx {

class TabManager;

class MessageBridge : public CefMessageRouterBrowserSide::Handler {
 public:
  explicit MessageBridge(TabManager* tabs) : tabs_(tabs) {}

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override;

 private:
  TabManager* tabs_;
};

}  // namespace nyx

#endif  // NYX_MESSAGE_BRIDGE_H_
