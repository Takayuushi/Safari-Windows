#pragma once

#include <string>

namespace safari {

inline std::string getStartPageHtml() {
    return R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Compass — Chronos Compass</title>
<style>
  *{box-sizing:border-box}
  html,body{margin:0;min-height:100%;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI Variable Text","Segoe UI",system-ui,sans-serif;color:#f5f5f7}
  body{
    background:
      radial-gradient(1100px 640px at 18% -8%,rgba(76,201,176,.2),transparent 62%),
      radial-gradient(900px 560px at 92% 0%,rgba(110,120,230,.18),transparent 58%),
      linear-gradient(180deg,#242428,#161619);
    background-attachment:fixed;
    display:flex;flex-direction:column;align-items:center;padding:50px 24px;
  }
  .container{max-width:820px;width:100%}
  .app-header{display:flex;flex-direction:column;align-items:center;gap:8px;margin-bottom:34px}
  .compass-icon{width:64px;height:64px;border-radius:18px;background:linear-gradient(135deg,#007aff,#0040dd);display:grid;place-items:center;box-shadow:0 6px 18px rgba(0,122,255,0.35)}
  .compass-icon svg{width:38px;height:38px;fill:#fff}
  .app-title{font-size:24px;font-weight:700;letter-spacing:-.02em;color:#f5f5f7;margin:0}
  .app-sub{font-size:12px;color:#8e8e93;letter-spacing:.04em;text-transform:uppercase}

  h2{font-size:17px;font-weight:700;letter-spacing:-.01em;margin:0 0 20px;color:#f5f5f7}
  .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(96px,1fr));gap:24px 16px}
  .fav{display:flex;flex-direction:column;align-items:center;gap:9px;text-decoration:none;color:#f5f5f7;transition:transform .15s ease}
  .fav:hover{transform:translateY(-2px)}
  .icon{width:68px;height:68px;border-radius:16px;display:grid;place-items:center;color:#fff;font-size:28px;font-weight:600;
        box-shadow:0 3px 12px rgba(0,0,0,.35),inset 0 1px 1px rgba(255,255,255,.2)}
  .label{max-width:96px;text-align:center;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:12px;color:#e1e1e6}
  .privacy{margin-top:48px;background:rgba(255,255,255,.05);border:1px solid rgba(255,255,255,.08);border-radius:14px;padding:18px 22px;display:flex;align-items:center;gap:16px}
  .shield{width:32px;height:32px;fill:#2997ff}
  .priv-title{font-size:14px;font-weight:600;margin:0 0 3px}
  .priv-sub{font-size:12px;color:#8e8e93;margin:0}
  .engine{margin-top:36px;text-align:center;color:#6e6e73;font-size:11px;letter-spacing:.15em;text-transform:uppercase}
</style>
</head>
<body>
<div class="container">
  <div class="app-header">
    <div class="compass-icon">
      <!-- Compass Star SVG -->
      <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8zm-1-14l2.5 5.5L19 11l-5.5 2.5L11 19l-2.5-5.5L3 11l5.5-2.5L11 6z"/></svg>
    </div>
    <h1 class="app-title">Chronos Compass</h1>
    <div class="app-sub">Compass &bull; Apple WebKit</div>
  </div>

  <h2>Favorites</h2>
  <div class="grid">
    <a class="fav" href="https://www.apple.com"><div class="icon" style="background:linear-gradient(135deg,#3a88e9,#1d63bf)">A</div><div class="label">Apple</div></a>
    <a class="fav" href="https://www.google.com"><div class="icon" style="background:linear-gradient(135deg,#ea4335,#c5221f)">G</div><div class="label">Google</div></a>
    <a class="fav" href="https://www.youtube.com"><div class="icon" style="background:linear-gradient(135deg,#ff0000,#cc0000)">Y</div><div class="label">YouTube</div></a>
    <a class="fav" href="https://github.com"><div class="icon" style="background:linear-gradient(135deg,#6e5494,#403259)">G</div><div class="label">GitHub</div></a>
    <a class="fav" href="https://www.wikipedia.org"><div class="icon" style="background:linear-gradient(135deg,#636e72,#2d3436)">W</div><div class="label">Wikipedia</div></a>
    <a class="fav" href="https://www.reddit.com"><div class="icon" style="background:linear-gradient(135deg,#ff4500,#cc3700)">R</div><div class="label">Reddit</div></a>
    <a class="fav" href="https://www.linkedin.com"><div class="icon" style="background:linear-gradient(135deg,#0077b5,#005582)">L</div><div class="label">LinkedIn</div></a>
    <a class="fav" href="https://www.netflix.com"><div class="icon" style="background:linear-gradient(135deg,#e50914,#b20710)">N</div><div class="label">Netflix</div></a>
  </div>

  <div class="privacy">
    <svg class="shield" viewBox="0 0 24 24"><path d="M12 1L3 5v6c0 5.55 3.84 10.74 9 12 5.16-1.26 9-6.45 9-12V5l-9-4zm-1 6h2v2h-2V7zm0 4h2v6h-2v-6z"/></svg>
    <div>
      <div class="priv-title">Privacy Report</div>
      <div class="priv-sub">WebKit Intelligent Tracking Prevention is actively protecting your browsing privacy.</div>
    </div>
  </div>

  <div class="engine">Chronos Compass &bull; Apple WebKit Engine</div>
</div>
</body>
</html>)HTML";
}

} // namespace safari
