# Chronos Compass for Windows (Apple WebKit)

An authentic, ultra-lightweight recreation of Apple Safari for Windows, powered by **Apple WebKit (WebKit2 WinCairo C++)**.

> **No Chromium. No Blink. No Electron. Pure native WebKit.**

---

## ✨ Features

- **Apple WebKit Engine**: Powered by WebKit2 with modern CSS, ES6, and intelligent tracking prevention.
- **macOS Glassmorphic Chrome**:
  - Borderless window with DWM glass integration and no Windows titlebar.
  - Interactive macOS traffic light buttons (Close, Minimize, Zoom).
  - Unified Smart Search Field (domain centering on idle, SSL lock, reader button, reload/stop).
  - Apple Safari Separate Tab Bar with rounded pill cards, site favicons, circular hover close buttons, and `+` new tab button.
- **Authentic macOS Sidebar & Settings**:
  - 📖 **Bookmarks**: Seeded with Apple, Google, YouTube, GitHub, Wikipedia, Reddit, etc., with squircle icons.
  - 👓 **Reading List**: Save and browse articles with live snippets.
  - 🕒 **History**: Timestamped browsing history (`12:56 PM`) with one-click **"Clear History…"** action.
  - ⚙️ **Settings (`chronos:settings`)**: Integrated Settings button at the bottom of the sidebar with authentic macOS Safari multi-tab preferences (General, Tabs, Privacy, Advanced).
- **Floating Downloads Popover**:
  - Clicking the toolbar download button opens a floating macOS-style mini panel right beneath the icon.
  - Intercepts binary downloads (`.msixbundle`, `.zip`, `.exe`, etc.) and downloads them directly without displaying raw binary data.
  - Shows file icon, truncated filename, progress bar, byte counts, and a magnifier button to **"Show in File Explorer"**.
- **Chronos Compass Start Page**:
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
2. Compiles `Chronos.exe` with MSVC C++20.
3. Bundles `Chronos.exe` with all WebKit runtime DLLs into the `Chronos Compass` folder.
4. Packages and uploads `Chronos-Compass-Windows-x64.zip` for immediate download.
