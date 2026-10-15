#pragma once
#include <string>
#include <vector>
#include "downloads.h"
#include "logo_data.h"
#include "util.h"

namespace safari {

inline std::string getDownloadsPageHtml() {
    std::string html = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Downloads — Chronos</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI Variable Text", "SF Pro Text", system-ui, sans-serif;
    background: #1C1C1E;
    color: #F5F5F7;
    min-height: 100vh;
    padding: 40px 24px;
    display: flex;
    flex-direction: column;
    align-items: center;
  }
  .container {
    width: 100%;
    max-width: 720px;
  }
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding-bottom: 24px;
    border-bottom: 1px solid rgba(255, 255, 255, 0.1);
    margin-bottom: 24px;
  }
  .header-left {
    display: flex;
    align-items: center;
    gap: 14px;
  }
  .header-logo {
    width: 36px;
    height: 36px;
    object-fit: contain;
  }
  .header h1 {
    font-size: 24px;
    font-weight: 700;
    letter-spacing: -0.5px;
  }
  .download-list {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .download-item {
    display: flex;
    align-items: center;
    padding: 14px 18px;
    background: rgba(255, 255, 255, 0.04);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 12px;
    gap: 16px;
    transition: background 0.15s ease;
  }
  .download-item:hover {
    background: rgba(255, 255, 255, 0.07);
  }
  .file-icon {
    width: 38px;
    height: 44px;
    border-radius: 6px;
    background: rgba(10, 132, 255, 0.15);
    border: 1.5px solid #0A84FF;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 11px;
    font-weight: 700;
    color: #0A84FF;
    text-transform: uppercase;
    flex-shrink: 0;
  }
  .info {
    flex: 1;
    min-width: 0;
  }
  .filename {
    font-size: 14px;
    font-weight: 600;
    color: #FFFFFF;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    margin-bottom: 4px;
  }
  .meta {
    font-size: 12px;
    color: rgba(235, 235, 245, 0.6);
  }
  .empty-state {
    text-align: center;
    padding: 80px 20px;
    color: rgba(235, 235, 245, 0.45);
  }
  .empty-icon {
    font-size: 48px;
    margin-bottom: 16px;
  }
  .empty-title {
    font-size: 18px;
    font-weight: 600;
    color: rgba(235, 235, 245, 0.7);
    margin-bottom: 6px;
  }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="header-left">
      <img class="header-logo" src="data:image/png;base64,)HTML";
    html += getChronosLogoBase64();
    html += R"HTML(" alt="Chronos Logo">
      <h1>Downloads</h1>
    </div>
  </div>
  <div class="download-list">
)HTML";

    const auto& items = DownloadsManager::instance().items();
    if (items.empty()) {
        html += R"HTML(
    <div class="empty-state">
      <div class="empty-icon">&#10515;</div>
      <div class="empty-title">No Downloads</div>
      <div>Files you download in Chronos will appear here.</div>
    </div>
)HTML";
    } else {
        for (const auto& item : items) {
            std::string ext = "BIN";
            size_t dot = item.filename.find_last_of(L'.');
            if (dot != std::wstring::npos && dot + 1 < item.filename.size()) {
                std::wstring wext = item.filename.substr(dot + 1);
                ext = toUtf8(wext);
                if (ext.size() > 4) ext = ext.substr(0, 4);
                for (auto& c : ext) c = (char)::toupper(c);
            }

            char mbuf[64];
            if (item.state == DownloadState::Completed) {
                snprintf(mbuf, sizeof(mbuf), "Completed &bull; %.1f MB", item.totalBytes / 1048576.0);
            } else if (item.state == DownloadState::InProgress) {
                snprintf(mbuf, sizeof(mbuf), "Downloading&hellip; %.1f MB", item.receivedBytes / 1048576.0);
            } else {
                snprintf(mbuf, sizeof(mbuf), "Failed");
            }

            html += "    <div class=\"download-item\">\n";
            html += "      <div class=\"file-icon\">" + ext + "</div>\n";
            html += "      <div class=\"info\">\n";
            html += "        <div class=\"filename\">" + toUtf8(item.filename) + "</div>\n";
            html += "        <div class=\"meta\">" + std::string(mbuf) + "</div>\n";
            html += "      </div>\n";
            html += "    </div>\n";
        }
    }

    html += R"HTML(
  </div>
</div>
</body>
</html>
)HTML";
    return html;
}

} // namespace safari
