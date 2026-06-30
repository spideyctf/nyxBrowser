// IPC bridge between the dark UI and the C++ core, over the CEF message router.
// window.cefQuery is injected by CEF in the chrome render process.
(function () {
  var listeners = {};

  function call(action, params) {
    return new Promise(function (resolve, reject) {
      var payload = Object.assign({ action: action }, params || {});
      if (typeof window.cefQuery !== "function") {
        reject(new Error("cefQuery unavailable"));
        return;
      }
      window.cefQuery({
        request: JSON.stringify(payload),
        persistent: false,
        onSuccess: function (response) {
          try { resolve(response ? JSON.parse(response) : {}); }
          catch (e) { resolve({}); }
        },
        onFailure: function (code, message) { reject(new Error(message)); }
      });
    });
  }

  function on(event, cb) {
    (listeners[event] = listeners[event] || []).push(cb);
  }

  // Invoked from C++ via ExecuteJavaScript: window.nyx._emit(name, payload).
  function _emit(event, payload) {
    (listeners[event] || []).forEach(function (cb) {
      try { cb(payload); } catch (e) { console.error(e); }
    });
  }

  window.nyx = { call: call, on: on, _emit: _emit };
})();
