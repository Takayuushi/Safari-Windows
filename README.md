# Safari for Windows (Apple WebKit)

An authentic, ultra-lightweight recreation of Apple Safari for Windows, powered by **Apple WebKit (WebKit2 WinCairo C++)**.

> **No Chromium. No Blink. No Electron. Pure native WebKit.**

---

## ✨ Features

- **Apple WebKit Engine**: Powered by WebKit2 with modern CSS, ES6, and intelligent tracking prevention.
- **macOS Glassmorphic Chrome**:
  - Interactive macOS traffic light buttons (Close, Minimize, Zoom).
  - Unified Smart Search Field (domain centering on idle, SSL lock, reader button, reload/stop).
  - Apple Safari Separate Tab Bar with rounded pill cards, site favicons, circular hover close buttons, and `+` new tab button.
- **Authentic 3-Segment macOS Sidebar**:
  - 📖 **Bookmarks**: Seeded with Apple, Google, YouTube, GitHub, Wikipedia, Reddit, etc., with squircle icons.
  - 👓 **Reading List**: Save and browse articles with live snippets.
  - 🕒 **History**: Timestamped browsing history (`12:56 PM`) with one-click **"Clear History…"** action.
  - **Live Filter**: Instant search box to search bookmarks, reading list, or history.
- **Floating Downloads Popover**:
  - Clicking the toolbar download button opens a floating macOS-style mini panel right beneath the icon.
  - Shows file icon, truncated filename, progress bar, byte counts, and a magnifier button to **"Show in File Explorer"**.
- **Safari 26 Start Page**:
  - Built-in Favorites grid with glassmorphic cards and Privacy Report.

---

## 🏗️ Architecture

- **Language:** C++20
- **Engine:** WebKit2 (WinCairo port) via `WKViewCreate` and `WKPageRef`
- **UI & Graphics:** Native Win32 + GDI+ high-DPI rendering
- **Build System:** CMake + MSVC

---

## 📦 Building with GitHub Actions

Every commit to `main` triggers an automated GitHub Actions build:
1. Downloads the WebKit WinCairo SDK.
2. Compiles `Safari.exe` with MSVC C++20.
3. Bundles `Safari.exe` with all WebKit runtime DLLs into `Safari-Windows-x64.zip`.
4. Uploads the release artifact for immediate download.
