#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>

namespace safari {

namespace colors {
    // macOS Safari Dark Mode Palette
    inline const Gdiplus::Color WindowBg(255, 26, 26, 29);          // #1a1a1d
    inline const Gdiplus::Color ChromeBg(255, 34, 34, 38);          // #222226
    inline const Gdiplus::Color ChromeBorder(255, 48, 48, 54);      // #303036
    inline const Gdiplus::Color AddressBg(255, 44, 44, 49);         // #2c2c31
    inline const Gdiplus::Color AddressBorder(255, 60, 60, 68);     // #3c3c44
    inline const Gdiplus::Color TabActive(255, 54, 54, 61);         // #36363d
    inline const Gdiplus::Color TabHover(255, 42, 42, 48);          // #2a2a30
    inline const Gdiplus::Color TabInactive(0, 0, 0, 0);            // Transparent
    inline const Gdiplus::Color TabBorder(255, 68, 68, 77);         // #44444d
    inline const Gdiplus::Color TextPrimary(255, 245, 245, 247);    // #f5f5f7
    inline const Gdiplus::Color TextSecondary(255, 142, 142, 147);  // #8e8e93
    inline const Gdiplus::Color TextMuted(255, 110, 110, 115);      // #6e6e73
    inline const Gdiplus::Color AccentBlue(255, 0, 122, 255);       // #007aff
    inline const Gdiplus::Color PopoverBg(255, 38, 38, 43);         // #26262b
    inline const Gdiplus::Color PopoverBorder(255, 62, 62, 70);     // #3e3e46
    inline const Gdiplus::Color SidebarBg(255, 28, 28, 31);         // #1c1c1f
    inline const Gdiplus::Color SidebarBorder(255, 46, 46, 51);     // #2e2e33
    inline const Gdiplus::Color SidebarSegment(255, 42, 42, 48);    // #2a2a30
    inline const Gdiplus::Color SidebarActiveSeg(255, 58, 58, 66);  // #3a3a42

    // macOS Traffic Lights
    inline const Gdiplus::Color TrafficClose(255, 255, 95, 86);     // #ff5f56
    inline const Gdiplus::Color TrafficMin(255, 255, 189, 46);      // #ffbd2e
    inline const Gdiplus::Color TrafficMax(255, 39, 201, 63);       // #27c93f
}

// Draw a smooth rounded rectangle
inline void fillRoundedRect(Gdiplus::Graphics& g, const Gdiplus::Brush& brush, const Gdiplus::RectF& rect, float radius) {
    if (rect.Width <= 0 || rect.Height <= 0) return;
    float d = radius * 2.0f;
    if (d > rect.Width) d = rect.Width;
    if (d > rect.Height) d = rect.Height;

    Gdiplus::GraphicsPath path;
    path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
    g.FillPath(&brush, &path);
}

inline void drawRoundedRect(Gdiplus::Graphics& g, const Gdiplus::Pen& pen, const Gdiplus::RectF& rect, float radius) {
    if (rect.Width <= 0 || rect.Height <= 0) return;
    float d = radius * 2.0f;
    if (d > rect.Width) d = rect.Width;
    if (d > rect.Height) d = rect.Height;

    Gdiplus::GraphicsPath path;
    path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
    g.DrawPath(&pen, &path);
}

} // namespace safari
