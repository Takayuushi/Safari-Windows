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
    padding: 36px 28px;
    display: flex;
    flex-direction: column;
    align-items: center;
    user-select: none;
  }
  .container {
    width: 100%;
    max-width: 860px;
  }
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding-bottom: 22px;
    border-bottom: 1px solid rgba(255, 255, 255, 0.1);
    margin-bottom: 24px;
  }
  .header-left {
    display: flex;
    align-items: center;
    gap: 16px;
  }
  .header-logo {
    width: 44px;
    height: 44px;
    object-fit: contain;
  }
  .header h1 {
    font-size: 24px;
    font-weight: 700;
    letter-spacing: -0.4px;
  }
  .header-actions {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .btn-top {
    background: rgba(255, 255, 255, 0.08);
    color: #f5f5f7;
    border: 1px solid rgba(255, 255, 255, 0.12);
    padding: 7px 14px;
    border-radius: 7px;
    font-size: 12.5px;
    font-weight: 500;
    text-decoration: none;
    cursor: pointer;
    transition: background 0.15s ease;
  }
  .btn-top:hover {
    background: rgba(255, 255, 255, 0.14);
  }

  .download-list {
    display: flex;
    flex-direction: column;
    gap: 12px;
  }
  .download-item {
    display: flex;
    align-items: center;
    padding: 16px 20px;
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
    width: 44px;
    height: 50px;
    border-radius: 8px;
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
    font-size: 14.5px;
    font-weight: 600;
    color: #FFFFFF;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    margin-bottom: 5px;
  }
  .meta {
    font-size: 12.5px;
    color: rgba(235, 235, 245, 0.65);
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .progress-wrap {
    width: 100%;
    height: 6px;
    background: rgba(255, 255, 255, 0.1);
    border-radius: 3px;
    overflow: hidden;
    margin: 8px 0 4px;
  }
  .progress-bar {
    height: 100%;
    background: #0A84FF;
    border-radius: 3px;
    transition: width 0.2s ease;
  }
  .progress-bar.paused {
    background: #FF9F0A;
  }

  .actions {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-shrink: 0;
  }
  .btn-action {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    background: rgba(255, 255, 255, 0.09);
    color: #F5F5F7;
    border: 1px solid rgba(255, 255, 255, 0.12);
    padding: 6px 13px;
    border-radius: 7px;
    font-size: 12px;
    font-weight: 500;
    text-decoration: none;
    cursor: pointer;
    transition: all 0.15s ease;
  }
  .btn-action:hover {
    background: rgba(255, 255, 255, 0.16);
  }
  .btn-primary {
    background: #0A84FF;
    border-color: #0A84FF;
    color: #FFFFFF;
  }
  .btn-primary:hover {
    background: #0077ED;
  }
  .btn-danger {
    background: rgba(255, 69, 58, 0.15);
    border-color: rgba(255, 69, 58, 0.3);
    color: #FF453A;
  }
  .btn-danger:hover {
    background: #FF453A;
    color: #FFFFFF;
  }

  .empty-state {
    text-align: center;
    padding: 90px 20px;
    color: rgba(235, 235, 245, 0.45);
  }
  .empty-icon {
    font-size: 54px;
    margin-bottom: 16px;
  }
  .empty-title {
    font-size: 19px;
    font-weight: 600;
    color: rgba(235, 235, 245, 0.75);
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
    <div class="header-actions">
      <a href="chronos:reveal_folder" class="btn-top">&#128194; Open Downloads Folder</a>
      <a href="chronos:clear" class="btn-top">Clear Completed</a>
    </div>
  </div>
  <div class="download-list">
)HTML";

    const auto& items = DownloadsManager::instance().items();
    bool hasActive = false;

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

            char idStr[32];
            snprintf(idStr, sizeof(idStr), "%llu", (unsigned long long)item.id);

            html += "    <div class=\"download-item\">\n";
            html += "      <div class=\"file-icon\">" + ext + "</div>\n";
            html += "      <div class=\"info\">\n";
            html += "        <div class=\"filename\">" + toUtf8(item.filename) + "</div>\n";

            if (item.state == DownloadState::Completed) {
                char mbuf[64];
                snprintf(mbuf, sizeof(mbuf), "Completed &bull; %.1f MB", (item.totalBytes > 0 ? item.totalBytes : item.receivedBytes) / 1048576.0);
                html += "        <div class=\"meta\">" + std::string(mbuf) + "</div>\n";
                html += "      </div>\n";
                html += "      <div class=\"actions\">\n";
                html += "        <a href=\"chronos:reveal?id=" + std::string(idStr) + "\" class=\"btn-action btn-primary\">Show in Folder</a>\n";
                html += "        <a href=\"chronos:remove?id=" + std::string(idStr) + "\" class=\"btn-action\">Remove</a>\n";
                html += "      </div>\n";
            } else if (item.state == DownloadState::InProgress) {
                hasActive = true;
                double pct = 0.0;
                if (item.totalBytes > 0) {
                    pct = (double)item.receivedBytes / (double)item.totalBytes * 100.0;
                    if (pct > 100.0) pct = 100.0;
                }
                char mbuf[128];
                if (item.totalBytes > 0) {
                    snprintf(mbuf, sizeof(mbuf), "Downloading&hellip; %.1f MB of %.1f MB (%.0f%%)", item.receivedBytes / 1048576.0, item.totalBytes / 1048576.0, pct);
                } else {
                    snprintf(mbuf, sizeof(mbuf), "Downloading&hellip; %.1f MB", item.receivedBytes / 1048576.0);
                }
                char pctBuf[32];
                snprintf(pctBuf, sizeof(pctBuf), "%.1f%%", pct);

                html += "        <div class=\"progress-wrap\"><div class=\"progress-bar\" style=\"width: " + std::string(pctBuf) + ";\"></div></div>\n";
                html += "        <div class=\"meta\">" + std::string(mbuf) + "</div>\n";
                html += "      </div>\n";
                html += "      <div class=\"actions\">\n";
                html += "        <a href=\"chronos:pause?id=" + std::string(idStr) + "\" class=\"btn-action\">Pause</a>\n";
                html += "        <a href=\"chronos:cancel?id=" + std::string(idStr) + "\" class=\"btn-action btn-danger\">Cancel</a>\n";
                html += "      </div>\n";
            } else if (item.state == DownloadState::Paused) {
                double pct = 0.0;
                if (item.totalBytes > 0) {
                    pct = (double)item.receivedBytes / (double)item.totalBytes * 100.0;
                    if (pct > 100.0) pct = 100.0;
                }
                char mbuf[128];
                if (item.totalBytes > 0) {
                    snprintf(mbuf, sizeof(mbuf), "Paused &bull; %.1f MB of %.1f MB (%.0f%%)", item.receivedBytes / 1048576.0, item.totalBytes / 1048576.0, pct);
                } else {
                    snprintf(mbuf, sizeof(mbuf), "Paused &bull; %.1f MB", item.receivedBytes / 1048576.0);
                }
                char pctBuf[32];
                snprintf(pctBuf, sizeof(pctBuf), "%.1f%%", pct);

                html += "        <div class=\"progress-wrap\"><div class=\"progress-bar paused\" style=\"width: " + std::string(pctBuf) + ";\"></div></div>\n";
                html += "        <div class=\"meta\">" + std::string(mbuf) + "</div>\n";
                html += "      </div>\n";
                html += "      <div class=\"actions\">\n";
                html += "        <a href=\"chronos:resume?id=" + std::string(idStr) + "\" class=\"btn-action btn-primary\">Resume</a>\n";
                html += "        <a href=\"chronos:cancel?id=" + std::string(idStr) + "\" class=\"btn-action btn-danger\">Cancel</a>\n";
                html += "      </div>\n";
            } else if (item.state == DownloadState::Cancelled) {
                html += "        <div class=\"meta\">Cancelled</div>\n";
                html += "      </div>\n";
                html += "      <div class=\"actions\">\n";
                html += "        <a href=\"chronos:remove?id=" + std::string(idStr) + "\" class=\"btn-action\">Remove</a>\n";
                html += "      </div>\n";
            } else {
                html += "        <div class=\"meta\">Failed</div>\n";
                html += "      </div>\n";
                html += "      <div class=\"actions\">\n";
                html += "        <a href=\"chronos:remove?id=" + std::string(idStr) + "\" class=\"btn-action\">Remove</a>\n";
                html += "      </div>\n";
            }

            html += "    </div>\n";
        }
    }

    if (hasActive) {
        html += R"HTML(
<script>
  setTimeout(function() {
    location.reload();
  }, 1000);
</script>
)HTML";
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
