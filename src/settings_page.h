#pragma once

#include <string>

namespace safari {

inline std::string getSettingsPageHtml() {
    return R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Settings — Chronos Compass</title>
<style>
  *{box-sizing:border-box}
  html,body{margin:0;height:100%;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI Variable Text","Segoe UI",system-ui,sans-serif;color:#f5f5f7;background:#1e1e22;user-select:none}
  
  .window{display:flex;flex-direction:column;max-width:740px;margin:32px auto;background:#25252a;border-radius:14px;border:1px solid rgba(255,255,255,0.08);box-shadow:0 18px 45px rgba(0,0,0,0.45);overflow:hidden}
  
  /* macOS Segmented Settings Toolbar */
  .toolbar{display:flex;align-items:center;justify-content:center;gap:6px;background:#202024;padding:12px;border-bottom:1px solid rgba(255,255,255,0.06)}
  .tab-btn{display:flex;flex-direction:column;align-items:center;gap:4px;background:transparent;border:0;color:#8e8e93;padding:6px 14px;border-radius:8px;cursor:pointer;font-size:11px;font-weight:500;transition:all .15s ease}
  .tab-btn svg{width:20px;height:20px;fill:currentColor}
  .tab-btn:hover{color:#f5f5f7;background:rgba(255,255,255,0.05)}
  .tab-btn.active{color:#fff;background:#383840;box-shadow:0 1px 3px rgba(0,0,0,0.3)}
  .tab-btn.active svg{fill:#2997ff}

  /* Content Area */
  .content{padding:26px 32px 36px;display:none;animation:fadeIn .15s ease}
  .content.active{display:block}
  @keyframes fadeIn{from{opacity:0;transform:translateY(3px)}to{opacity:1;transform:translateY(0)}}

  h3{font-size:16px;font-weight:600;margin:0 0 16px;color:#f5f5f7}
  .group{background:rgba(255,255,255,0.04);border:1px solid rgba(255,255,255,0.07);border-radius:10px;margin-bottom:20px;overflow:hidden}
  .row{display:flex;align-items:center;justify-content:space-between;padding:12px 16px;border-bottom:1px solid rgba(255,255,255,0.04)}
  .row:last-child{border-bottom:0}
  
  .label-group{display:flex;flex-direction:column;gap:2px}
  .label{font-size:13px;font-weight:500;color:#f5f5f7}
  .desc{font-size:11.5px;color:#8e8e93}

  select,input[type="text"]{background:#1a1a1d;color:#f5f5f7;border:1px solid rgba(255,255,255,0.12);border-radius:6px;padding:6px 10px;font-size:12.5px;outline:none}
  select:focus,input[type="text"]:focus{border-color:#2997ff}

  /* macOS Switch */
  .switch{position:relative;display:inline-block;width:38px;height:22px}
  .switch input{opacity:0;width:0;height:0}
  .slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#3a3a3e;transition:.2s;border-radius:22px}
  .slider:before{position:absolute;content:"";height:18px;width:18px;left:2px;bottom:2px;background-color:white;transition:.2s;border-radius:50%;box-shadow:0 1px 3px rgba(0,0,0,0.3)}
  input:checked + .slider{background-color:#34c759}
  input:checked + .slider:before{transform:translateX(16px)}

  .btn-action{background:#383840;color:#f5f5f7;border:1px solid rgba(255,255,255,0.1);padding:6px 14px;border-radius:6px;font-size:12px;cursor:pointer}
  .btn-action:hover{background:#484852}
  .btn-danger{background:#d70015;color:#fff;border:0}
  .btn-danger:hover{background:#b80012}

  .badge{display:inline-flex;align-items:center;gap:6px;background:rgba(41,151,255,0.12);color:#2997ff;padding:4px 10px;border-radius:6px;font-size:11.5px;font-weight:600}
</style>
</head>
<body>

<div class="window">
  <!-- Toolbar -->
  <div class="toolbar">
    <button class="tab-btn active" onclick="showTab('general', this)">
      <svg viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
      General
    </button>
    <button class="tab-btn" onclick="showTab('tabs', this)">
      <svg viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm-5 14H7v-2h7v2zm3-4H7v-2h10v2zm0-4H7V7h10v2z"/></svg>
      Tabs
    </button>
    <button class="tab-btn" onclick="showTab('privacy', this)">
      <svg viewBox="0 0 24 24"><path d="M12 1L3 5v6c0 5.55 3.84 10.74 9 12 5.16-1.26 9-6.45 9-12V5l-9-4zm-1 6h2v2h-2V7zm0 4h2v6h-2v-6z"/></svg>
      Privacy
    </button>
    <button class="tab-btn" onclick="showTab('advanced', this)">
      <svg viewBox="0 0 24 24"><path d="M9.4 16.6L4.8 12l4.6-4.6L8 6l-6 6 6 6 1.4-1.4zm5.2 0l4.6-4.6-4.6-4.6L16 6l6 6-6 6-1.4-1.4z"/></svg>
      Advanced
    </button>
  </div>

  <!-- General Tab -->
  <div id="general" class="content active">
    <h3>General Preferences</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Default Search Engine</div>
          <div class="desc">Used for searches in the Smart Search field</div>
        </div>
        <select id="selSearch">
          <option value="google" selected>Google</option>
          <option value="duckduckgo">DuckDuckGo</option>
          <option value="bing">Bing</option>
          <option value="ecosia">Ecosia</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">New Windows Open With</div>
          <div class="desc">Page to display when opening a new window</div>
        </div>
        <select>
          <option selected>Start Page</option>
          <option>Homepage</option>
          <option>Empty Page</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Homepage</div>
          <div class="desc">URL for your default start page</div>
        </div>
        <input type="text" value="chronos:start" style="width:200px">
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">File Download Location</div>
          <div class="desc">Files downloaded from the web will be saved here</div>
        </div>
        <button class="btn-action">Downloads Folder</button>
      </div>
    </div>
  </div>

  <!-- Tabs Tab -->
  <div id="tabs" class="content">
    <h3>Tab Preferences</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Separate Tab Bar</div>
          <div class="desc">Show tabs below the unified toolbar (macOS Safari style)</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Open links in tabs instead of windows</div>
          <div class="desc">Command-click opens links in new background tabs</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
    </div>
  </div>

  <!-- Privacy Tab -->
  <div id="privacy" class="content">
    <h3>Privacy & Security</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Intelligent Tracking Prevention</div>
          <div class="desc">Prevent cross-site tracking cookies and fingerprinting</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Fraudulent Website Warning</div>
          <div class="desc">Warn when visiting suspected phishing or malicious websites</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Website Data</div>
          <div class="desc">Manage cookies, local storage, and cached assets</div>
        </div>
        <button class="btn-action btn-danger" onclick="alert('Website data cleared!')">Clear All Data…</button>
      </div>
    </div>
  </div>

  <!-- Advanced Tab -->
  <div id="advanced" class="content">
    <h3>Advanced</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Show Full Website Address</div>
          <div class="desc">Always display the complete URL instead of the domain name</div>
        </div>
        <label class="switch"><input type="checkbox"><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">WebKit Web Inspector (Developer Tools)</div>
          <div class="desc">Enable inspecting elements, console, and network requests</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Rendering Engine</div>
          <div class="desc">Core browser rendering architecture</div>
        </div>
        <span class="badge">Apple WebKit2 WinCairo</span>
      </div>
    </div>
  </div>
</div>

<script>
function showTab(id, btn) {
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.querySelectorAll('.content').forEach(c => c.classList.remove('active'));
  btn.classList.add('active');
  document.getElementById(id).classList.add('active');
}
</script>
</body>
</html>)HTML";
}

} // namespace safari
