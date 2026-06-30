// nyx Browser - dark UI controller.
(function () {
  var nyx = window.nyx;

  var state = {
    tabs: {},          // id -> {id,title,url,active,loading,canBack,canForward}
    order: [],         // tab ids in display order
    activeId: 0,
    settings: {},
    bookmarks: [],
    panel: null,       // open panel name or null
    sidebarCollapsed: false
  };

  // ---------- element helpers ----------
  function $(id) { return document.getElementById(id); }
  function el(tag, cls, text) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text != null) e.textContent = text;
    return e;
  }
  function host(url) {
    try { return new URL(url).host || url; } catch (e) { return url; }
  }

  // ---------- content overlay bounds ----------
  function pushBounds() {
    var slot = $("content-slot").getBoundingClientRect();
    if (state.panel) {
      // Hide the native content overlay behind the HTML panel.
      nyx.call("content.bounds", { x: -30000, y: 0, w: 10, h: 10 });
    } else {
      nyx.call("content.bounds", {
        x: Math.round(slot.left), y: Math.round(slot.top),
        w: Math.round(slot.width), h: Math.round(slot.height)
      });
    }
  }
  window.addEventListener("resize", pushBounds);

  // ---------- tab strip ----------
  function renderTabs() {
    var strip = $("tabstrip");
    strip.innerHTML = "";
    state.order.forEach(function (id) {
      var t = state.tabs[id];
      if (!t) return;
      var tab = el("div", "tab" + (t.active ? " active" : ""));
      if (t.loading) tab.appendChild(el("span", "spin"));
      var title = el("span", "tab-title", t.title || host(t.url) || "New tab");
      tab.appendChild(title);
      var close = el("span", "tab-close", "\u2715");
      close.addEventListener("click", function (ev) {
        ev.stopPropagation();
        nyx.call("tab.close", { id: id });
      });
      tab.appendChild(close);
      tab.addEventListener("click", function () {
        nyx.call("tab.activate", { id: id });
      });
      strip.appendChild(tab);
    });
  }

  function active() { return state.tabs[state.activeId]; }

  function syncToolbar() {
    var t = active();
    $("btn-back").disabled = !t || !t.canBack;
    $("btn-forward").disabled = !t || !t.canForward;
    if (t && document.activeElement !== $("omnibox")) {
      $("omnibox").value = (t.url && t.url.indexOf("nyx://") === 0) ? "" : (t.url || "");
    }
    var bm = t && state.bookmarks.some(function (b) { return b.url === t.url; });
    $("btn-bookmark").classList.toggle("on", !!bm);
    $("btn-bookmark").innerHTML = bm ? "\u2605" : "\u2606";
  }

  // ---------- omnibox ----------
  function normalize(input) {
    var s = input.trim();
    if (!s) return null;
    if (/^[a-z]+:\/\//i.test(s) || s.indexOf("nyx://") === 0) return s;
    var looksUrl = /^[^\s]+\.[^\s]{2,}(\/.*)?$/.test(s) || s.indexOf("localhost") === 0;
    if (looksUrl) return "https://" + s;
    var eng = state.settings.searchEngine || "https://www.google.com/search?q=";
    return eng + encodeURIComponent(s);
  }

  $("omnibox").addEventListener("keydown", function (e) {
    if (e.key === "Enter") {
      var url = normalize(this.value);
      if (url) nyx.call("nav.go", { id: state.activeId, url: url });
      this.blur();
    }
  });
  $("omnibox").addEventListener("focus", function () { this.select(); });

  // ---------- toolbar buttons ----------
  $("btn-back").addEventListener("click", function () { nyx.call("nav.back", { id: state.activeId }); });
  $("btn-forward").addEventListener("click", function () { nyx.call("nav.forward", { id: state.activeId }); });
  $("btn-reload").addEventListener("click", function () { nyx.call("nav.reload", { id: state.activeId }); });
  $("btn-newtab").addEventListener("click", function () { nyx.call("tab.new", {}); });
  $("btn-sidebar").addEventListener("click", function () {
    state.sidebarCollapsed = !state.sidebarCollapsed;
    $("sidebar").classList.toggle("collapsed", state.sidebarCollapsed);
    setTimeout(pushBounds, 140);
  });
  $("btn-bookmark").addEventListener("click", function () {
    var t = active(); if (!t || !t.url) return;
    var has = state.bookmarks.some(function (b) { return b.url === t.url; });
    var action = has ? "bookmark.remove" : "bookmark.add";
    nyx.call(action, { url: t.url, title: t.title || t.url }).then(function (r) {
      state.bookmarks = r.bookmarks || [];
      syncToolbar();
    });
  });

  // ---------- panels ----------
  function openPanel(name) {
    state.panel = name;
    document.querySelectorAll(".side-btn").forEach(function (b) {
      b.classList.toggle("active", b.dataset.panel === name);
    });
    $("panel").classList.remove("hidden");
    pushBounds();
    if (name === "bookmarks") renderBookmarks();
    else if (name === "history") renderHistory();
    else if (name === "downloads") renderDownloads();
    else if (name === "settings") renderSettings();
  }
  function closePanel() {
    state.panel = null;
    document.querySelectorAll(".side-btn").forEach(function (b) { b.classList.remove("active"); });
    $("panel").classList.add("hidden");
    pushBounds();
  }
  $("panel-close").addEventListener("click", closePanel);
  document.querySelectorAll(".side-btn").forEach(function (b) {
    b.addEventListener("click", function () {
      if (state.panel === b.dataset.panel) closePanel();
      else openPanel(b.dataset.panel);
    });
  });

  function setPanel(title, actionsNode) {
    $("panel-title").textContent = title;
    var a = $("panel-actions"); a.innerHTML = "";
    if (actionsNode) a.appendChild(actionsNode);
    return $("panel-body");
  }
  function openInTab(url) {
    nyx.call("nav.go", { id: state.activeId, url: url });
    closePanel();
  }

  function renderBookmarks() {
    var body = setPanel("Bookmarks", null);
    nyx.call("bookmark.list", {}).then(function (r) {
      state.bookmarks = r.bookmarks || [];
      body.innerHTML = "";
      if (!state.bookmarks.length) { body.appendChild(el("div", "empty", "No bookmarks yet.")); return; }
      state.bookmarks.forEach(function (b) {
        var row = el("div", "list-row");
        var main = el("div", "row-main");
        main.appendChild(el("div", "row-title", b.title || host(b.url)));
        main.appendChild(el("div", "row-sub", b.url));
        main.addEventListener("click", function () { openInTab(b.url); });
        row.appendChild(main);
        var del = el("span", "icon-btn row-del", "\u2715");
        del.addEventListener("click", function () {
          nyx.call("bookmark.remove", { url: b.url }).then(function (rr) {
            state.bookmarks = rr.bookmarks || []; renderBookmarks(); syncToolbar();
          });
        });
        row.appendChild(del);
        body.appendChild(row);
      });
    });
  }

  function renderHistory() {
    var clear = el("button", "btn danger", "Clear history");
    clear.addEventListener("click", function () {
      nyx.call("history.clear", {}).then(renderHistory);
    });
    var body = setPanel("History", clear);
    nyx.call("history.list", {}).then(function (r) {
      var items = r.history || [];
      body.innerHTML = "";
      if (!items.length) { body.appendChild(el("div", "empty", "No history.")); return; }
      items.slice(0, 500).forEach(function (h) {
        var row = el("div", "list-row");
        var main = el("div", "row-main");
        main.appendChild(el("div", "row-title", h.title || host(h.url)));
        main.appendChild(el("div", "row-sub", h.url));
        main.addEventListener("click", function () { openInTab(h.url); });
        row.appendChild(main);
        body.appendChild(row);
      });
    });
  }

  function renderDownloads() {
    var body = setPanel("Downloads", null);
    nyx.call("downloads.list", {}).then(function (r) {
      drawDownloads(body, r.downloads || []);
    });
  }
  function drawDownloads(body, items) {
    body.innerHTML = "";
    if (!items.length) { body.appendChild(el("div", "empty", "No downloads.")); return; }
    items.forEach(function (d) {
      var row = el("div", "list-row");
      var main = el("div", "row-main");
      main.appendChild(el("div", "row-title", d.filename || host(d.url)));
      var sub = d.complete ? "Completed" : (d.canceled ? "Canceled" : (d.percent + "%"));
      main.appendChild(el("div", "row-sub", sub + "  -  " + (d.url || "")));
      if (d.inProgress && !d.complete) {
        var bar = el("div", "bar"); var i = el("i");
        i.style.width = (d.percent || 0) + "%"; bar.appendChild(i); main.appendChild(bar);
      }
      row.appendChild(main);
      body.appendChild(row);
    });
  }

  function makeSwitch(on, onChange) {
    var s = el("div", "switch" + (on ? " on" : ""));
    s.appendChild(el("i"));
    s.addEventListener("click", function () {
      var nv = !s.classList.contains("on");
      s.classList.toggle("on", nv); onChange(nv);
    });
    return s;
  }
  function settingRow(label, desc, control) {
    var row = el("div", "setting");
    var left = el("div");
    left.appendChild(el("div", "s-label", label));
    if (desc) left.appendChild(el("div", "s-desc", desc));
    row.appendChild(left); row.appendChild(control);
    return row;
  }
  function saveSetting(key, value) {
    nyx.call("settings.set", { key: key, value: value }).then(function (r) {
      state.settings = r.settings || state.settings;
    });
  }
  function renderSettings() {
    var body = setPanel("Settings", null);
    var s = state.settings;
    body.innerHTML = "";

    var home = el("input"); home.type = "text"; home.value = s.homepage || "";
    home.addEventListener("change", function () { saveSetting("homepage", home.value); });
    body.appendChild(settingRow("Homepage", "Opened when you start a new session", home));

    var eng = el("input"); eng.type = "text"; eng.value = s.searchEngine || "";
    eng.addEventListener("change", function () { saveSetting("searchEngine", eng.value); });
    body.appendChild(settingRow("Search engine", "Query is appended to this URL", eng));

    body.appendChild(settingRow("Restore session", "Reopen tabs on startup",
      makeSwitch(!!s.restoreSession, function (v) { saveSetting("restoreSession", v); })));
    body.appendChild(settingRow("Do Not Track", "Send the DNT request header",
      makeSwitch(!!s.doNotTrack, function (v) { saveSetting("doNotTrack", v); })));
    body.appendChild(settingRow("Block third-party cookies", "Reject cross-site cookies",
      makeSwitch(!!s.blockThirdPartyCookies, function (v) { saveSetting("blockThirdPartyCookies", v); })));
  }

  // ---------- events from core ----------
  nyx.on("tab.created", function (t) {
    state.tabs[t.id] = { id: t.id, title: t.title, url: t.url, active: !!t.active,
      loading: true, canBack: false, canForward: false };
    if (state.order.indexOf(t.id) < 0) state.order.push(t.id);
    renderTabs();
  });
  nyx.on("tab.updated", function (t) {
    var cur = state.tabs[t.id] || {};
    state.tabs[t.id] = Object.assign(cur, t);
    if (t.active) state.activeId = t.id;
    renderTabs(); syncToolbar();
  });
  nyx.on("tab.activated", function (p) {
    state.activeId = p.id;
    state.order.forEach(function (id) { if (state.tabs[id]) state.tabs[id].active = (id === p.id); });
    renderTabs(); syncToolbar(); pushBounds();
  });
  nyx.on("tab.closed", function (p) {
    delete state.tabs[p.id];
    state.order = state.order.filter(function (id) { return id !== p.id; });
    renderTabs(); syncToolbar();
  });
  nyx.on("download.updated", function (item) {
    if (state.panel === "downloads") renderDownloads();
  });

  // ---------- boot ----------
  function boot() {
    nyx.call("ui.ready", {}).then(function (r) {
      state.settings = r.settings || {};
      return nyx.call("bookmark.list", {});
    }).then(function (r) {
      state.bookmarks = r.bookmarks || [];
      syncToolbar();
      pushBounds();
    }).catch(function (e) { console.error("boot failed", e); });
  }

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", boot);
  else boot();
})();
