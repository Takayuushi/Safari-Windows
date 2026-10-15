#pragma once

#include <string>
#include <cmath>
#include "logo_data.h"
#include "util.h"

namespace safari {

struct BrowserSettings {
    bool showFullUrl = false;
    std::wstring searchEngine = L"Google";     // Google, DuckDuckGo, Bing, Yahoo, Ecosia
    std::wstring newTabSetting = L"Start Page"; // Start Page, Homepage, Empty Page
    std::wstring homepage = L"chronos:start";
    double pageZoom = 1.0;
    bool devToolsEnabled = true;
};

inline std::string getSettingsPageHtml(const BrowserSettings& s = BrowserSettings{}) {
    std::string html = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Settings — Chronos Compass</title>
<style>
  *{box-sizing:border-box}
  html,body{margin:0;min-height:100%;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI Variable Text","Segoe UI",system-ui,sans-serif;color:#f5f5f7;background:#1e1e22;user-select:none}
  
  .window{display:flex;flex-direction:column;max-width:980px;margin:32px auto 48px;background:#25252a;border-radius:16px;border:1px solid rgba(255,255,255,0.08);box-shadow:0 24px 60px rgba(0,0,0,0.5);overflow:hidden}
  
  /* App Header Banner */
  .app-banner{display:flex;align-items:center;gap:18px;padding:22px 34px;background:linear-gradient(180deg,#2b2b32,#232328);border-bottom:1px solid rgba(255,255,255,0.07)}
  .app-banner img{width:52px;height:52px;object-fit:contain}
  .app-banner-info h2{margin:0 0 3px;font-size:19px;font-weight:700;color:#fff;letter-spacing:-0.2px}
  .app-banner-info p{margin:0;font-size:13px;color:#8e8e93}

  /* macOS Segmented Settings Toolbar */
  .toolbar{display:flex;align-items:center;justify-content:center;gap:4px;background:#202024;padding:10px 16px;border-bottom:1px solid rgba(255,255,255,0.06);overflow-x:auto}
  .tab-btn{display:flex;flex-direction:column;align-items:center;gap:5px;background:transparent;border:0;color:#8e8e93;padding:8px 14px;border-radius:9px;cursor:pointer;font-size:12px;font-weight:500;transition:all .15s ease;white-space:nowrap}
  .tab-btn svg{width:20px;height:20px;fill:currentColor}
  .tab-btn:hover{color:#f5f5f7;background:rgba(255,255,255,0.05)}
  .tab-btn.active{color:#fff;background:#383840;box-shadow:0 1px 3px rgba(0,0,0,0.3)}
  .tab-btn.active svg{fill:#2997ff}

  /* Content Area */
  .content{padding:30px 38px 42px;display:none;animation:fadeIn .15s ease}
  .content.active{display:block}
  @keyframes fadeIn{from{opacity:0;transform:translateY(3px)}to{opacity:1;transform:translateY(0)}}

  h3{font-size:18px;font-weight:600;margin:0 0 20px;color:#f5f5f7}
  .group{background:rgba(255,255,255,0.04);border:1px solid rgba(255,255,255,0.07);border-radius:12px;margin-bottom:24px;overflow:hidden}
  .row{display:flex;align-items:center;justify-content:space-between;padding:14px 20px;border-bottom:1px solid rgba(255,255,255,0.04)}
  .row:last-child{border-bottom:0}
  
  .label-group{display:flex;flex-direction:column;gap:3px;max-width:560px}
  .label{font-size:14px;font-weight:500;color:#f5f5f7}
  .desc{font-size:12.5px;color:#8e8e93;line-height:1.4}

  select {
    -webkit-appearance: none;
    -moz-appearance: none;
    appearance: none;
    background-color: #2c2c30 !important;
    color: #f5f5f7 !important;
    border: 1px solid rgba(255, 255, 255, 0.16) !important;
    border-radius: 8px !important;
    padding: 7px 34px 7px 14px !important;
    font-size: 13px !important;
    font-family: inherit !important;
    outline: none !important;
    cursor: pointer !important;
    background-image: url("data:image/svg+xml;charset=UTF-8,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='%238e8e93'%3e%3cpath d='M7 10l5 5 5-5z'/%3e%3c/svg%3e") !important;
    background-repeat: no-repeat !important;
    background-position: right 8px center !important;
    background-size: 16px !important;
    min-width: 190px;
    box-shadow: 0 1px 2px rgba(0,0,0,0.2) !important;
  }
  select option {
    background-color: #252529 !important;
    color: #f5f5f7 !important;
    padding: 8px 12px;
  }
  select:hover {
    background-color: #38383e !important;
    border-color: rgba(255, 255, 255, 0.28) !important;
  }
  select:focus {
    border-color: #2997ff !important;
    box-shadow: 0 0 0 2px rgba(41, 151, 255, 0.3) !important;
  }

  input[type="text"] {
    -webkit-appearance: none;
    appearance: none;
    background-color: #1a1a1d !important;
    color: #f5f5f7 !important;
    border: 1px solid rgba(255, 255, 255, 0.14) !important;
    border-radius: 8px !important;
    padding: 7px 12px !important;
    font-size: 13px !important;
    outline: none !important;
  }
  input[type="text"]:focus {
    border-color: #2997ff !important;
    box-shadow: 0 0 0 2px rgba(41, 151, 255, 0.3) !important;
  }

  /* macOS Switch */
  .switch{position:relative;display:inline-block;width:42px;height:24px;flex-shrink:0}
  .switch input{opacity:0;width:0;height:0}
  .slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#3a3a3e;transition:.2s;border-radius:24px}
  .slider:before{position:absolute;content:"";height:20px;width:20px;left:2px;bottom:2px;background-color:white;transition:.2s;border-radius:50%;box-shadow:0 1px 3px rgba(0,0,0,0.3)}
  input:checked + .slider{background-color:#34c759}
  input:checked + .slider:before{transform:translateX(18px)}

  .btn-action{background:#383840;color:#f5f5f7;border:1px solid rgba(255,255,255,0.1);padding:7px 16px;border-radius:7px;font-size:13px;cursor:pointer;font-weight:500}
  .btn-action:hover{background:#484852}
  .btn-danger{background:#d70015;color:#fff;border:0}
  .btn-danger:hover{background:#b80012}

  .badge{display:inline-flex;align-items:center;gap:6px;background:rgba(41,151,255,0.12);color:#2997ff;padding:5px 12px;border-radius:7px;font-size:12px;font-weight:600}
</style>
</head>
<body>

<div class="window">
  <!-- Brand Header -->
  <div class="app-banner">
    <img src="data:image/png;base64,)HTML";
    html += getChronosLogoBase64();
    html += R"HTML(" alt="Logo">
    <div class="app-banner-info">
      <h2>Chronos Compass</h2>
      <p>Apple WebKit WinCairo &bull; Version 18.0 (Build 2026.10)</p>
    </div>
  </div>

  <!-- Segmented Toolbar -->
  <div class="toolbar">
    <button class="tab-btn active" onclick="showTab('general', this)">
      <svg viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
      General
    </button>
    <button class="tab-btn" onclick="showTab('tabs', this)">
      <svg viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm-5 14H7v-2h7v2zm3-4H7v-2h10v2zm0-4H7V7h10v2z"/></svg>
      Tabs
    </button>
    <button class="tab-btn" onclick="showTab('autofill', this)">
      <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 3c1.66 0 3 1.34 3 3s-1.34 3-3 3-3-1.34-3-3 1.34-3 3-3zm0 14.2c-2.5 0-4.71-1.28-6-3.22.03-1.99 4-3.08 6-3.08 1.99 0 5.97 1.09 6 3.08-1.29 1.94-3.5 3.22-6 3.22z"/></svg>
      AutoFill
    </button>
    <button class="tab-btn" onclick="showTab('passwords', this)">
      <svg viewBox="0 0 24 24"><path d="M18 8h-1V6c0-2.76-2.24-5-5-5S7 3.24 7 6v2H6c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V10c0-1.1-.9-2-2-2zm-6 9c-1.1 0-2-.9-2-2s.9-2 2-2 2 .9 2 2-.9 2-2 2zm3.1-9H8.9V6c0-1.71 1.39-3.1 3.1-3.1 1.71 0 3.1 1.39 3.1 3.1v2z"/></svg>
      Passwords
    </button>
    <button class="tab-btn" onclick="showTab('search', this)">
      <svg viewBox="0 0 24 24"><path d="M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/></svg>
      Search
    </button>
    <button class="tab-btn" onclick="showTab('security', this)">
      <svg viewBox="0 0 24 24"><path d="M12 1L3 5v6c0 5.55 3.84 10.74 9 12 5.16-1.26 9-6.45 9-12V5l-9-4zm-1 6h2v2h-2V7zm0 4h2v6h-2v-6z"/></svg>
      Security
    </button>
    <button class="tab-btn" onclick="showTab('privacy', this)">
      <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8zm-5-9h10v2H7z"/></svg>
      Privacy
    </button>
    <button class="tab-btn" onclick="showTab('websites', this)">
      <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-1 17.93c-3.95-.49-7-3.85-7-7.93 0-.62.08-1.21.21-1.79L9 15v1c0 1.1.9 2 2 2v1.93zm6.9-2.54c-.26-.81-1-1.39-1.9-1.39h-1v-3c0-.55-.45-1-1-1H8v-2h2c.55 0 1-.45 1-1V7h2c1.1 0 2-.9 2-2v-.41c2.93 1.19 5 4.06 5 7.41 0 2.08-.8 3.97-2.1 5.39z"/></svg>
      Websites
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
          <div class="label">Chronos opens with</div>
          <div class="desc">Window state restored when starting the browser</div>
        </div>
        <select>
          <option selected>A new window</option>
          <option>All windows from last session</option>
          <option>All non-private windows from last session</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">New windows open with</div>
          <div class="desc">Page to display when opening a new window</div>
        </div>
        <select>
          <option selected>Start Page</option>
          <option>Homepage</option>
          <option>Empty Page</option>
          <option>Same Page</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">New tabs open with</div>
          <div class="desc">Page to display when clicking + in the tab bar or opening new tabs</div>
        </div>
        <select onchange="saveSetting('newTab', this.value)">
          <option )HTML" + (s.newTabSetting == L"Start Page" ? std::string("selected") : "") + R"HTML(>Start Page</option>
          <option )HTML" + (s.newTabSetting == L"Homepage" ? std::string("selected") : "") + R"HTML(>Homepage</option>
          <option )HTML" + (s.newTabSetting == L"Empty Page" ? std::string("selected") : "") + R"HTML(>Empty Page</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Homepage</div>
          <div class="desc">Default website URL for new browser sessions</div>
        </div>
        <input type="text" id="homepageInput" value=")HTML" + toUtf8(s.homepage) + R"HTML(" style="width:240px" onchange="saveSetting('homepage', this.value)">
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Remove history items</div>
          <div class="desc">Automatically prune browsing history after specified duration</div>
        </div>
        <select>
          <option>After one day</option>
          <option>After one week</option>
          <option>After one month</option>
          <option selected>After one year</option>
          <option>Manually</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">File download location</div>
          <div class="desc">Files downloaded from web pages will be saved directly here</div>
        </div>
        <button class="btn-action" onclick="location.href='chronos:open_downloads_folder'">Downloads Folder</button>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Open "safe" files after downloading</div>
          <div class="desc">Safe files include pictures, PDF documents, and plain text files</div>
        </div>
        <label class="switch"><input type="checkbox"><span class="slider"></span></label>
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
          <div class="desc">Show tabs below the unified toolbar (macOS Safari layout)</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Show website icons in tabs</div>
          <div class="desc">Display site favicons on tab cards</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Open links in tabs instead of windows</div>
          <div class="desc">Command-clicking or middle-clicking links opens in background tabs</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Make new tabs active immediately</div>
          <div class="desc">Automatically focus newly created tabs</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Automatically close tabs</div>
          <div class="desc">Close inactive tabs after period of time</div>
        </div>
        <select>
          <option selected>Manually</option>
          <option>After one day</option>
          <option>After one week</option>
          <option>After one month</option>
        </select>
      </div>
    </div>
  </div>

  <!-- AutoFill Tab -->
  <div id="autofill" class="content">
    <h3>AutoFill Preferences</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Using info from my contacts</div>
          <div class="desc">Auto-fill name, address, email, and phone numbers in web forms</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">User names and passwords</div>
          <div class="desc">Remember account credentials securely for quick sign-in</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Credit and debit cards</div>
          <div class="desc">Store card numbers and expiration dates encrypted</div>
        </div>
        <label class="switch"><input type="checkbox"><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Other web forms</div>
          <div class="desc">Auto-complete frequent search queries and miscellaneous fields</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
    </div>
  </div>

  <!-- Passwords Tab -->
  <div id="passwords" class="content">
    <h3>Passwords & Passkeys</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">AutoFill Passwords and Passkeys</div>
          <div class="desc">Allow filling stored credentials automatically with Windows Hello</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Detect Compromised Passwords</div>
          <div class="desc">Alert when stored credentials appear in known public data leaks</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Vault Encryption</div>
          <div class="desc">Hardware-backed cryptographic protection</div>
        </div>
        <span class="badge">Windows DPAPI &bull; AES-256</span>
      </div>
    </div>
  </div>

  <!-- Search Tab -->
  <div id="search" class="content">
    <h3>Search Preferences</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Default Search Engine</div>
          <div class="desc">Used for web searches entered into the Smart Search field</div>
        </div>
        <select onchange="saveSetting('searchEngine', this.value)">
          <option )HTML" + (s.searchEngine == L"Google" ? std::string("selected") : "") + R"HTML(>Google</option>
          <option )HTML" + (s.searchEngine == L"DuckDuckGo" ? std::string("selected") : "") + R"HTML(>DuckDuckGo</option>
          <option )HTML" + (s.searchEngine == L"Bing" ? std::string("selected") : "") + R"HTML(>Bing</option>
          <option )HTML" + (s.searchEngine == L"Yahoo" ? std::string("selected") : "") + R"HTML(>Yahoo</option>
          <option )HTML" + (s.searchEngine == L"Ecosia" ? std::string("selected") : "") + R"HTML(>Ecosia</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Include search engine suggestions</div>
          <div class="desc">Fetch predicted queries from search provider as you type</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Include Safari Suggestions</div>
          <div class="desc">Show news, Wikipedia summaries, and weather directly in dropdown</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Enable Quick Website Search</div>
          <div class="desc">Jump directly to searches on previously visited websites</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Preload Top Hit in background</div>
          <div class="desc">Accelerate navigation by speculative pre-connection</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
    </div>
  </div>

  <!-- Security Tab -->
  <div id="security" class="content">
    <h3>Security</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Warn when visiting fraudulent websites</div>
          <div class="desc">Inspect URLs against known anti-phishing databases before loading</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Enable JavaScript</div>
          <div class="desc">Allow websites to run interactive JavaScript programs</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Allow WebGL Hardware Acceleration</div>
          <div class="desc">Enable 3D graphics rendering via GPU shaders</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Block pop-up windows</div>
          <div class="desc">Prevent unprompted intrusive new window spawns</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
    </div>
  </div>

  <!-- Privacy Tab -->
  <div id="privacy" class="content">
    <h3>Privacy</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Prevent cross-site tracking</div>
          <div class="desc">Apple WebKit Intelligent Tracking Prevention (ITP) suppresses trackers</div>
        </div>
        <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Hide IP address from known trackers</div>
          <div class="desc">Protect your geographic location via iCloud Private Relay</div>
        </div>
        <div style="display:flex;align-items:center;gap:10px">
          <span class="badge" style="background:rgba(255,255,255,0.08);color:#8e8e93;font-size:11px">Apple Ecosystem</span>
          <label class="switch"><input type="checkbox" checked><span class="slider"></span></label>
        </div>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Block all cookies</div>
          <div class="desc">Disallow all websites from reading or storing cookies (may break login)</div>
        </div>
        <label class="switch"><input type="checkbox"><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Manage Website Data</div>
          <div class="desc">View and prune stored cache, databases, and local storage</div>
        </div>
        <button class="btn-action btn-danger" onclick="if(confirm('Are you sure you want to clear browsing history, download logs, and cached data?')){ location.href='chronos:clear_all_data'; }">Clear All Data…</button>
      </div>
    </div>
  </div>

  <!-- Websites Tab -->
  <div id="websites" class="content">
    <h3>Websites Permissions</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Camera</div>
          <div class="desc">When websites request webcam access</div>
        </div>
        <select>
          <option selected>Ask</option>
          <option>Deny</option>
          <option>Allow</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Microphone</div>
          <div class="desc">When websites request audio recording access</div>
        </div>
        <select>
          <option selected>Ask</option>
          <option>Deny</option>
          <option>Allow</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Location</div>
          <div class="desc">When websites request precise GPS coordinates</div>
        </div>
        <select>
          <option selected>Ask</option>
          <option>Deny</option>
          <option>Allow</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Notifications</div>
          <div class="desc">Web push notification permission prompts</div>
        </div>
        <select>
          <option selected>Ask</option>
          <option>Deny</option>
          <option>Allow</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Auto-Play</div>
          <div class="desc">Control media playback behavior across websites</div>
        </div>
        <select>
          <option selected>Stop Media with Sound</option>
          <option>Never Auto-Play</option>
          <option>Allow All Auto-Play</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Default Page Zoom</div>
          <div class="desc">Global magnification level applied to all web content</div>
        </div>
        <select onchange="saveSetting('pageZoom', this.value)">
          <option value="1.0" )HTML" + (std::abs(s.pageZoom - 1.0) < 0.05 ? std::string("selected") : "") + R"HTML(>100%</option>
          <option value="1.15" )HTML" + (std::abs(s.pageZoom - 1.15) < 0.05 ? std::string("selected") : "") + R"HTML(>115%</option>
          <option value="1.25" )HTML" + (std::abs(s.pageZoom - 1.25) < 0.05 ? std::string("selected") : "") + R"HTML(>125%</option>
          <option value="1.5" )HTML" + (std::abs(s.pageZoom - 1.5) < 0.05 ? std::string("selected") : "") + R"HTML(>150%</option>
        </select>
      </div>
    </div>
  </div>

  <!-- Advanced Tab -->
  <div id="advanced" class="content">
    <h3>Advanced</h3>
    <div class="group">
      <div class="row">
        <div class="label-group">
          <div class="label">Show full website address</div>
          <div class="desc">Always display the complete URL instead of domain only in address bar</div>
        </div>
        <label class="switch"><input type="checkbox" )HTML" + (s.showFullUrl ? std::string("checked") : "") + R"HTML( onchange="saveSetting('showFullUrl', this.checked ? 'true' : 'false')"><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Minimum font size</div>
          <div class="desc">Never use font sizes smaller than specified threshold</div>
        </div>
        <select>
          <option selected>None</option>
          <option>9 pt</option>
          <option>10 pt</option>
          <option>12 pt</option>
        </select>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">WebKit Web Inspector (Developer Tools)</div>
          <div class="desc">Enable inspecting DOM elements, JavaScript console, and network logs</div>
        </div>
        <label class="switch"><input type="checkbox" )HTML" + (s.devToolsEnabled ? std::string("checked") : "") + R"HTML( onchange="saveSetting('devTools', this.checked ? 'true' : 'false')"><span class="slider"></span></label>
      </div>
      <div class="row">
        <div class="label-group">
          <div class="label">Rendering Engine Architecture</div>
          <div class="desc">Core browser pipeline implementation</div>
        </div>
        <span class="badge">Apple WebKit2 WinCairo C++20</span>
      </div>
    </div>
  </div>
</div>

<script>
function showTab(id, btn) {
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.querySelectorAll('.content').forEach(c => c.classList.remove('active'));
  if (btn) btn.classList.add('active');
  var target = document.getElementById(id);
  if (target) target.classList.add('active');
  location.hash = id;
}

function saveSetting(key, val) {
  var activeTab = location.hash || '#general';
  location.href = 'chronos:setting?key=' + encodeURIComponent(key) + '&val=' + encodeURIComponent(val) + activeTab;
}

window.addEventListener('DOMContentLoaded', function() {
  var h = (location.hash || '').replace('#', '');
  if (h) {
    var btn = document.querySelector(".tab-btn[onclick*='" + h + "']");
    if (btn) showTab(h, btn);
  }
});
</script>
</body>
</html>)HTML";
    return html;
}

} // namespace safari
