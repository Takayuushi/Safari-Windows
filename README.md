# Safari 26 (Windows Native & Glassmorphic)

An ultra-refined Safari 26 copy for Windows built according to modern engineering practices:
- **macOS Sequoia Glassmorphic Design**: Real backdrop blur, vibrant translucent toolbar, dynamic theme switching (Auto / Light / Dark).
- **Pixel-Accurate Controls**: macOS traffic lights with interactive hover glyphs, SF-style vector glyphs, and compact tab layout.
- **Unified Smart Search**: Domain-highlighting address bar, instant URL vs search query differentiation, SSL lock status, and multi-engine fallback (Google, DuckDuckGo, Bing, Ecosia).
- **Safari Reader Mode**: Full distraction-free reading experience with Sepia, Dark, Gray, and Light color schemes with font size scaling.
- **Tab Overview**: Live bird's-eye preview card grid of open tabs with close and switch cards.
- **Collapsible Sidebar**: Unified Bookmarks, Reading List, and History with instant persistence.
- **Downloads Manager**: Toolbar download popover with circular progress ring + full `safari://downloads` interface.
- **Preferences & Settings**: Full `safari://settings` management for default search, homepage, privacy, tracker blocking, and hardware acceleration.
- **Full Video & Web Compatibility**: Full 4K/60fps YouTube, HDR, H.264/AV1/VP9 codecs without WinCairo video caps.
- **Dual Runtime Support**:
  - Run immediately on Windows via lightweight runner: `python run.py`
  - Build into native Windows desktop executable using Tauri & Rust: `cargo tauri build`
