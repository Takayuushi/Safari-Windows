#include "browser.h"
#include "gfx.h"
#include "util.h"
#include "start_page.h"
#include <gdiplus.h>
#include <algorithm>
#include <cstdio>

namespace safari {

Browser::Browser() = default;

Browser::~Browser() {
    for (auto& tab : m_tabs) {
        if (tab.webView) {
            tab.webView->destroy();
        }
    }
    m_tabs.clear();
}

void Browser::initData() {
    m_bookmarks.push_back({ L"Apple", L"https://www.apple.com", L"apple.com", RGB(51, 153, 255), L'A' });
    m_bookmarks.push_back({ L"Google", L"https://www.google.com", L"google.com", RGB(234, 67, 53), L'G' });
    m_bookmarks.push_back({ L"YouTube", L"https://www.youtube.com", L"youtube.com", RGB(255, 0, 0), L'Y' });
    m_bookmarks.push_back({ L"GitHub", L"https://github.com", L"github.com", RGB(110, 84, 148), L'G' });
    m_bookmarks.push_back({ L"Wikipedia", L"https://www.wikipedia.org", L"wikipedia.org", RGB(99, 110, 114), L'W' });
    m_bookmarks.push_back({ L"Reddit", L"https://www.reddit.com", L"reddit.com", RGB(255, 69, 0), L'R' });
    m_bookmarks.push_back({ L"LinkedIn", L"https://www.linkedin.com", L"linkedin.com", RGB(0, 119, 181), L'L' });
    m_bookmarks.push_back({ L"Netflix", L"https://www.netflix.com", L"netflix.com", RGB(229, 9, 20), L'N' });

    m_readingList.push_back({ L"WebKit — Open Source Web Browser Engine", L"https://webkit.org", L"webkit.org", L"WebKit powers Apple Safari and provides fast, standards-compliant web rendering." });
    m_readingList.push_back({ L"Apple Human Interface Guidelines", L"https://developer.apple.com/design/human-interface-guidelines", L"developer.apple.com", L"Explore guidelines for creating smooth, fluid, high quality user interfaces." });
}

bool Browser::initialize(HWND hwnd) {
    m_hwnd = hwnd;
    m_dpi = GetDpiForWindow(hwnd);
    initData();

    // Hook downloads listener
    DownloadsManager::instance().setChangeListener([this]() {
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
    });

    // Create Address Bar Edit Control (hidden initially, shown on click)
    m_editAddress = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL | ES_LEFT,
        0, 0, 0, 0,
        m_hwnd, (HMENU)1001, GetModuleHandle(nullptr), nullptr);

    SetWindowLongPtrW(m_editAddress, GWLP_USERDATA, (LONG_PTR)this);
    m_origEditProc = (WNDPROC)SetWindowLongPtrW(m_editAddress, GWLP_WNDPROC, (LONG_PTR)editSubclassProc);

    HFONT hFont = CreateFontW(
        -MulDiv(11, m_dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    SendMessageW(m_editAddress, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Initial Tab with Safari Start Page
    createTab(L"safari:start");
    handleResize();
    return true;
}

void Browser::createTab(const std::wstring& url) {
    auto tab = Tab{};
    tab.url = url;
    tab.title = (url == L"safari:start") ? L"Start Page" : L"Loading…";
    tab.webView = std::make_unique<WebView>();

    RECT webBounds{ 0, 86, 1200, 800 };
    tab.webView->create(m_hwnd, webBounds);

    size_t newIndex = m_tabs.size();
    auto* viewPtr = tab.webView.get();

    viewPtr->onTitleChanged = [this, newIndex](const std::wstring& newTitle) {
        if (newIndex < m_tabs.size()) {
            m_tabs[newIndex].title = newTitle.empty() ? extractDomain(m_tabs[newIndex].url) : newTitle;
            if (m_activeTab == newIndex) {
                SetWindowTextW(m_hwnd, (m_tabs[newIndex].title + L" — Safari").c_str());
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    };

    viewPtr->onUrlChanged = [this, newIndex](const std::wstring& newUrl) {
        if (newIndex < m_tabs.size()) {
            m_tabs[newIndex].url = newUrl;
            if (isWebUrl(newUrl)) {
                recordHistory(m_tabs[newIndex].title, newUrl);
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    };

    viewPtr->onLoadingChanged = [this, newIndex](bool loading) {
        if (newIndex < m_tabs.size()) {
            m_tabs[newIndex].isLoading = loading;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    };

    if (url == L"safari:start") {
        viewPtr->loadHtml(getStartPageHtml(), "safari:start");
    } else {
        viewPtr->loadUrl(url);
    }

    m_tabs.push_back(std::move(tab));
    selectTab(newIndex);
}

void Browser::selectTab(size_t index) {
    if (index >= m_tabs.size()) return;

    if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].webView) {
        m_tabs[m_activeTab].webView->setVisible(false);
    }

    m_activeTab = index;

    if (m_tabs[m_activeTab].webView) {
        m_tabs[m_activeTab].webView->setVisible(true);
        m_tabs[m_activeTab].webView->setFocus();
    }

    SetWindowTextW(m_hwnd, (m_tabs[m_activeTab].title + L" — Safari").c_str());
    handleResize();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Browser::closeTab(size_t index) {
    if (index >= m_tabs.size()) return;

    if (m_tabs.size() == 1) {
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
        return;
    }

    if (m_tabs[index].webView) {
        m_tabs[index].webView->destroy();
    }

    m_tabs.erase(m_tabs.begin() + index);

    if (m_activeTab >= m_tabs.size()) {
        m_activeTab = m_tabs.size() - 1;
    }
    selectTab(m_activeTab);
}

void Browser::navigate(const std::wstring& input) {
    std::wstring target = normalizeInput(input);
    if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].webView) {
        m_tabs[m_activeTab].url = target;
        if (target == L"safari:start") {
            m_tabs[m_activeTab].title = L"Start Page";
            m_tabs[m_activeTab].webView->loadHtml(getStartPageHtml(), "safari:start");
        } else {
            m_tabs[m_activeTab].webView->loadUrl(target);
        }
    }
    ShowWindow(m_editAddress, SW_HIDE);
    if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].webView) {
        m_tabs[m_activeTab].webView->setFocus();
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Browser::recordHistory(const std::wstring& title, const std::wstring& url) {
    HistoryItem item;
    item.title = title.empty() ? extractDomain(url) : title;
    item.url = url;
    item.domain = extractDomain(url);
    GetLocalTime(&item.time);

    m_history.insert(m_history.begin(), item);
    if (m_history.size() > 200) m_history.pop_back();
}

void Browser::toggleSidebar() {
    m_sidebarOpen = !m_sidebarOpen;
    handleResize();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Browser::toggleDownloadsPopover() {
    m_downloadsPopoverOpen = !m_downloadsPopoverOpen;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Browser::handleResize() {
    if (!m_hwnd) return;

    RECT client{};
    GetClientRect(m_hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    int sidebarWidth = m_sidebarOpen ? 260 : 0;
    int topChromeHeight = 86; // Toolbar 50px + Tab bar 36px

    RECT webRect{
        sidebarWidth,
        topChromeHeight,
        width,
        height
    };

    if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].webView) {
        m_tabs[m_activeTab].webView->setBounds(webRect);
    }

    updateLayout();
}

void Browser::updateLayout() {
    RECT client{};
    GetClientRect(m_hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    // Traffic lights
    m_btnTrafficClose = RECT{ 14, 18, 26, 30 };
    m_btnTrafficMin   = RECT{ 32, 18, 44, 30 };
    m_btnTrafficMax   = RECT{ 50, 18, 62, 30 };

    // Sidebar Toggle
    m_btnSidebarToggle = RECT{ 74, 10, 106, 42 };

    // Back / Forward Nav
    m_btnNavBack    = RECT{ 114, 12, 142, 40 };
    m_btnNavForward = RECT{ 144, 12, 172, 40 };

    // Address Bar
    int addrWidth = std::clamp(width - 480, 360, 640);
    int addrLeft = (width - addrWidth) / 2;
    m_addressBarRect = RECT{ addrLeft, 9, addrLeft + addrWidth, 43 };
    m_btnReload = RECT{ m_addressBarRect.right - 32, 11, m_addressBarRect.right - 4, 39 };

    // Right Action Buttons
    m_btnDownloads      = RECT{ width - 146, 10, width - 114, 42 };
    m_btnShare          = RECT{ width - 110, 10, width - 78, 42 };
    m_btnNewTabToolbar  = RECT{ width - 74, 10, width - 42, 42 };
    m_btnOverview       = RECT{ width - 38, 10, width - 6, 42 };

    // Tab Bar
    m_tabBarRect = RECT{ 12, 52, width - 12, 84 };
    m_tabRects.clear();
    m_tabCloseRects.clear();

    size_t count = m_tabs.size();
    if (count > 0) {
        int availWidth = width - 60;
        int tabW = std::clamp(availWidth / (int)count, 140, 220);
        int curX = 12;

        for (size_t i = 0; i < count; ++i) {
            RECT tr{ curX, 52, curX + tabW, 83 };
            m_tabRects.push_back(tr);
            RECT cr{ tr.right - 22, 59, tr.right - 6, 75 };
            m_tabCloseRects.push_back(cr);
            curX += tabW + 4;
        }

        m_btnNewTabInBar = RECT{ curX + 2, 53, curX + 30, 81 };
    }

    // Sidebar Rectangles
    if (m_sidebarOpen) {
        m_sidebarRect = RECT{ 0, 86, 260, height };
        m_segBookmarksRect = RECT{ 12, 96, 88, 124 };
        m_segReadingRect   = RECT{ 92, 96, 168, 124 };
        m_segHistoryRect   = RECT{ 172, 96, 248, 124 };
        m_sidebarSearchRect = RECT{ 12, 132, 248, 160 };
        m_btnClearHistoryRect = RECT{ 14, 166, 120, 186 };
    }

    // Downloads Popover
    m_downloadsPopoverRect = RECT{ m_btnDownloads.left - 240, 50, m_btnDownloads.left + 80, 360 };
    m_btnClearDownloadsRect = RECT{ m_downloadsPopoverRect.right - 60, 58, m_downloadsPopoverRect.right - 14, 78 };
    m_btnOpenFolderRect = RECT{ m_downloadsPopoverRect.left + 14, 324, m_downloadsPopoverRect.right - 14, 350 };
}

void Browser::paint(HDC hdc) {
    RECT client{};
    GetClientRect(m_hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    Gdiplus::Graphics g(memDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // Fill background
    Gdiplus::SolidBrush bgBrush(colors::WindowBg);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    paintTopChrome(g, width);
    paintTabBar(g);

    if (m_sidebarOpen) {
        paintSidebar(g, height);
    }

    if (m_downloadsPopoverOpen) {
        paintDownloadsPopover(g);
    }

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void Browser::paintTopChrome(Gdiplus::Graphics& g, int width) {
    // Top Chrome Background
    Gdiplus::SolidBrush chromeBrush(colors::ChromeBg);
    g.FillRectangle(&chromeBrush, 0, 0, width, 86);

    Gdiplus::Pen borderPen(colors::ChromeBorder, 1.0f);
    g.DrawLine(&borderPen, 0, 85, (int)width, 85);

    // Traffic Lights
    Gdiplus::SolidBrush closeBrush(colors::TrafficClose);
    Gdiplus::SolidBrush minBrush(colors::TrafficMin);
    Gdiplus::SolidBrush maxBrush(colors::TrafficMax);

    g.FillEllipse(&closeBrush, m_btnTrafficClose.left, m_btnTrafficClose.top, 12, 12);
    g.FillEllipse(&minBrush, m_btnTrafficMin.left, m_btnTrafficMin.top, 12, 12);
    g.FillEllipse(&maxBrush, m_btnTrafficMax.left, m_btnTrafficMax.top, 12, 12);

    // Sidebar Toggle Icon
    Gdiplus::Pen iconPen(colors::TextSecondary, 1.5f);
    g.DrawRectangle(&iconPen, (int)m_btnSidebarToggle.left + 7, (int)m_btnSidebarToggle.top + 8, 18, 14);
    g.DrawLine(&iconPen, (int)m_btnSidebarToggle.left + 13, (int)m_btnSidebarToggle.top + 8, (int)m_btnSidebarToggle.left + 13, (int)m_btnSidebarToggle.top + 22);

    // Back / Forward Capsule
    Gdiplus::SolidBrush navBg(colors::AddressBg);
    fillRoundedRect(g, navBg, Gdiplus::RectF(112.0f, 12.0f, 60.0f, 26.0f), 6.0f);

    Gdiplus::Pen navPen(m_hoverBack ? colors::TextPrimary : colors::TextSecondary, 1.8f);
    Gdiplus::PointF backPts[3] = { { 130.0f, 20.0f }, { 124.0f, 25.0f }, { 130.0f, 30.0f } };
    g.DrawLines(&navPen, backPts, 3);

    Gdiplus::Pen fwdPen(m_hoverForward ? colors::TextPrimary : colors::TextSecondary, 1.8f);
    Gdiplus::PointF fwdPts[3] = { { 152.0f, 20.0f }, { 158.0f, 25.0f }, { 152.0f, 30.0f } };
    g.DrawLines(&fwdPen, fwdPts, 3);

    // Smart Search Field (Address Bar)
    Gdiplus::RectF addrRectF(
        (float)m_addressBarRect.left, (float)m_addressBarRect.top,
        (float)(m_addressBarRect.right - m_addressBarRect.left),
        (float)(m_addressBarRect.bottom - m_addressBarRect.top));

    Gdiplus::SolidBrush addrBrush(colors::AddressBg);
    fillRoundedRect(g, addrBrush, addrRectF, 8.0f);

    Gdiplus::Pen addrBorder(colors::AddressBorder, 1.0f);
    drawRoundedRect(g, addrBorder, addrRectF, 8.0f);

    // aA Format Icon
    Gdiplus::Font fontSmall(L"Segoe UI Variable Text", 9.0f, Gdiplus::FontStyleBold);
    Gdiplus::SolidBrush textMuted(colors::TextMuted);
    g.DrawString(L"aA", -1, &fontSmall, Gdiplus::PointF(addrRectF.X + 10.0f, addrRectF.Y + 8.0f), &textMuted);

    // Padlock Icon
    Gdiplus::Pen lockPen(colors::TextMuted, 1.4f);
    g.DrawRectangle(&lockPen, addrRectF.X + 32.0f, addrRectF.Y + 13.0f, 8.0f, 7.0f);
    g.DrawArc(&lockPen, addrRectF.X + 33.0f, addrRectF.Y + 9.0f, 6.0f, 6.0f, 180.0f, 180.0f);

    // Domain Text
    std::wstring displayHost = (m_activeTab < m_tabs.size()) ? extractDomain(m_tabs[m_activeTab].url) : L"Start Page";
    Gdiplus::Font fontDomain(L"Segoe UI Variable Text", 10.0f, Gdiplus::FontStyleRegular);
    Gdiplus::SolidBrush textPrimary(colors::TextPrimary);

    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(displayHost.c_str(), -1, &fontDomain, addrRectF, &format, &textPrimary);

    // Reload Icon
    Gdiplus::Pen reloadPen(m_hoverReload ? colors::TextPrimary : colors::TextSecondary, 1.6f);
    g.DrawArc(&reloadPen, (float)m_btnReload.left + 6.0f, (float)m_btnReload.top + 6.0f, 13.0f, 13.0f, 45.0f, 270.0f);

    // Loading Progress Bar
    if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].isLoading) {
        Gdiplus::SolidBrush progBrush(colors::AccentBlue);
        g.FillRectangle(&progBrush, (int)addrRectF.X + 8, (int)addrRectF.Y + (int)addrRectF.Height - 3, (int)addrRectF.Width - 16, 2);
    }

    // Right Action Buttons
    // Downloads Button
    Gdiplus::Pen dlPen(colors::TextSecondary, 1.6f);
    g.DrawEllipse(&dlPen, (int)m_btnDownloads.left + 6, (int)m_btnDownloads.top + 6, 18, 18);
    g.DrawLine(&dlPen, (int)m_btnDownloads.left + 15, (int)m_btnDownloads.top + 10, (int)m_btnDownloads.left + 15, (int)m_btnDownloads.top + 18);
    Gdiplus::PointF arrowPts[3] = {
        { (float)m_btnDownloads.left + 11.0f, (float)m_btnDownloads.top + 15.0f },
        { (float)m_btnDownloads.left + 15.0f, (float)m_btnDownloads.top + 19.0f },
        { (float)m_btnDownloads.left + 19.0f, (float)m_btnDownloads.top + 15.0f }
    };
    g.DrawLines(&dlPen, arrowPts, 3);

    if (DownloadsManager::instance().hasActiveDownloads()) {
        Gdiplus::SolidBrush badgeBrush(colors::AccentBlue);
        g.FillEllipse(&badgeBrush, (int)m_btnDownloads.right - 10, (int)m_btnDownloads.top + 4, 6, 6);
    }

    // Share Button
    g.DrawRectangle(&dlPen, (int)m_btnShare.left + 8, (int)m_btnShare.top + 13, 14, 11);
    g.DrawLine(&dlPen, (int)m_btnShare.left + 15, (int)m_btnShare.top + 7, (int)m_btnShare.left + 15, (int)m_btnShare.top + 16);

    // New Tab Toolbar (+)
    g.DrawLine(&dlPen, (int)m_btnNewTabToolbar.left + 16, (int)m_btnNewTabToolbar.top + 10, (int)m_btnNewTabToolbar.left + 16, (int)m_btnNewTabToolbar.top + 22);
    g.DrawLine(&dlPen, (int)m_btnNewTabToolbar.left + 10, (int)m_btnNewTabToolbar.top + 16, (int)m_btnNewTabToolbar.left + 22, (int)m_btnNewTabToolbar.top + 16);

    // Overview Button (Grid)
    g.DrawRectangle(&dlPen, (int)m_btnOverview.left + 8, (int)m_btnOverview.top + 8, 14, 14);
}

void Browser::paintTabBar(Gdiplus::Graphics& g) {
    Gdiplus::Font tabFont(L"Segoe UI Variable Text", 9.5f, Gdiplus::FontStyleRegular);
    Gdiplus::SolidBrush textPrim(colors::TextPrimary);
    Gdiplus::SolidBrush textMuted(colors::TextSecondary);

    for (size_t i = 0; i < m_tabRects.size(); ++i) {
        const RECT& tr = m_tabRects[i];
        bool isActive = (i == m_activeTab);
        bool isHover = (m_hoverTab == (int)i);

        Gdiplus::RectF rf((float)tr.left, (float)tr.top, (float)(tr.right - tr.left), (float)(tr.bottom - tr.top));

        if (isActive) {
            Gdiplus::SolidBrush activeBrush(colors::TabActive);
            fillRoundedRect(g, activeBrush, rf, 7.0f);
            Gdiplus::Pen border(colors::TabBorder, 1.0f);
            drawRoundedRect(g, border, rf, 7.0f);
        } else if (isHover) {
            Gdiplus::SolidBrush hoverBrush(colors::TabHover);
            fillRoundedRect(g, hoverBrush, rf, 7.0f);
        } else {
            // Divider on right of inactive tab
            Gdiplus::Pen divPen(colors::ChromeBorder, 1.0f);
            g.DrawLine(&divPen, (int)tr.right + 2, (int)tr.top + 6, (int)tr.right + 2, (int)tr.bottom - 6);
        }

        // Letter Favicon squircle
        wchar_t letter = m_tabs[i].title.empty() ? L'S' : towupper(m_tabs[i].title[0]);
        wchar_t lbuf[2] = { letter, 0 };

        Gdiplus::SolidBrush badgeBrush(isActive ? colors::AccentBlue : Gdiplus::Color(255, 75, 75, 82));
        fillRoundedRect(g, badgeBrush, Gdiplus::RectF(rf.X + 8.0f, rf.Y + 7.0f, 16.0f, 16.0f), 4.0f);

        Gdiplus::Font fontL(L"Segoe UI Variable Text", 8.0f, Gdiplus::FontStyleBold);
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(lbuf, -1, &fontL, Gdiplus::RectF(rf.X + 8.0f, rf.Y + 7.0f, 16.0f, 16.0f), &sf, &textPrim);

        // Title text
        Gdiplus::StringFormat tf;
        tf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        tf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF textRf(rf.X + 30.0f, rf.Y, rf.Width - 52.0f, rf.Height);
        g.DrawString(m_tabs[i].title.c_str(), -1, &tabFont, textRf, &tf, isActive ? &textPrim : &textMuted);

        // Close Button (×)
        if (m_hoverTabClose == (int)i) {
            Gdiplus::SolidBrush closeCircle(Gdiplus::Color(255, 72, 72, 80));
            g.FillEllipse(&closeCircle, (int)m_tabCloseRects[i].left, (int)m_tabCloseRects[i].top, 14, 14);
        }
        Gdiplus::Font closeFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular);
        g.DrawString(L"×", -1, &closeFont, Gdiplus::PointF((float)m_tabCloseRects[i].left + 2.0f, (float)m_tabCloseRects[i].top - 2.0f), &textMuted);
    }

    // Plus (+) button in tab bar
    if (m_tabs.size() > 0) {
        Gdiplus::Pen plusPen(colors::TextSecondary, 1.6f);
        g.DrawLine(&plusPen, (int)m_btnNewTabInBar.left + 14, (int)m_btnNewTabInBar.top + 8, (int)m_btnNewTabInBar.left + 14, (int)m_btnNewTabInBar.top + 20);
        g.DrawLine(&plusPen, (int)m_btnNewTabInBar.left + 8, (int)m_btnNewTabInBar.top + 14, (int)m_btnNewTabInBar.left + 20, (int)m_btnNewTabInBar.top + 14);
    }
}

void Browser::paintSidebar(Gdiplus::Graphics& g, int height) {
    Gdiplus::SolidBrush sideBg(colors::SidebarBg);
    g.FillRectangle(&sideBg, 0, 86, 260, height - 86);

    Gdiplus::Pen border(colors::SidebarBorder, 1.0f);
    g.DrawLine(&border, 259, 86, 259, (int)height);

    // Segment Switcher (Bookmarks / Reading List / History)
    Gdiplus::SolidBrush segBg(colors::SidebarSegment);
    fillRoundedRect(g, segBg, Gdiplus::RectF(12.0f, 96.0f, 236.0f, 28.0f), 7.0f);

    Gdiplus::SolidBrush activeSeg(colors::SidebarActiveSeg);
    if (m_activeSidebarTab == SidebarTab::Bookmarks) {
        fillRoundedRect(g, activeSeg, Gdiplus::RectF(14.0f, 98.0f, 76.0f, 24.0f), 5.0f);
    } else if (m_activeSidebarTab == SidebarTab::ReadingList) {
        fillRoundedRect(g, activeSeg, Gdiplus::RectF(92.0f, 98.0f, 76.0f, 24.0f), 5.0f);
    } else {
        fillRoundedRect(g, activeSeg, Gdiplus::RectF(170.0f, 98.0f, 76.0f, 24.0f), 5.0f);
    }

    // Segment Icons
    Gdiplus::Pen segPen(colors::TextPrimary, 1.5f);
    // Ribbon Bookmark
    g.DrawRectangle(&segPen, 46, 104, 10, 12);
    // Eyeglasses
    g.DrawEllipse(&segPen, 120, 107, 7, 7);
    g.DrawEllipse(&segPen, 131, 107, 7, 7);
    g.DrawLine(&segPen, 127, 110, 131, 110);
    // Clock
    g.DrawEllipse(&segPen, 202, 104, 12, 12);
    g.DrawLine(&segPen, 208, 107, 208, 110);
    g.DrawLine(&segPen, 208, 110, 212, 110);

    // Search Box
    Gdiplus::SolidBrush searchBg(colors::AddressBg);
    fillRoundedRect(g, searchBg, Gdiplus::RectF(12.0f, 132.0f, 236.0f, 26.0f), 6.0f);
    Gdiplus::Font searchFont(L"Segoe UI Variable Text", 9.0f, Gdiplus::FontStyleRegular);
    Gdiplus::SolidBrush phBrush(colors::TextMuted);
    g.DrawString(L"Search", -1, &searchFont, Gdiplus::PointF(28.0f, 137.0f), &phBrush);

    // Sidebar items list
    Gdiplus::Font itemFont(L"Segoe UI Variable Text", 9.5f, Gdiplus::FontStyleRegular);
    Gdiplus::SolidBrush textPrim(colors::TextPrimary);
    Gdiplus::SolidBrush textSub(colors::TextSecondary);

    if (m_activeSidebarTab == SidebarTab::History) {
        // "Clear History..." link
        Gdiplus::Font clearFont(L"Segoe UI Variable Text", 9.0f, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush blueBrush(colors::AccentBlue);
        g.DrawString(L"Clear History…", -1, &clearFont, Gdiplus::PointF(14.0f, 168.0f), &blueBrush);

        int curY = 192;
        for (const auto& h : m_history) {
            if (curY > height - 30) break;
            g.DrawString(h.domain.c_str(), -1, &itemFont, Gdiplus::PointF(14.0f, (float)curY), &textPrim);

            wchar_t tbuf[32];
            swprintf(tbuf, 32, L"%d:%02d %ls", (h.time.wHour % 12 == 0 ? 12 : h.time.wHour % 12), h.time.wMinute, (h.time.wHour >= 12 ? L"PM" : L"AM"));
            Gdiplus::StringFormat sfRight;
            sfRight.SetAlignment(Gdiplus::StringAlignmentFar);
            g.DrawString(tbuf, -1, &itemFont, Gdiplus::RectF(14.0f, (float)curY, 232.0f, 20.0f), &sfRight, &textSub);

            curY += 24;
        }
    } else if (m_activeSidebarTab == SidebarTab::Bookmarks) {
        int curY = 168;
        for (const auto& b : m_bookmarks) {
            if (curY > height - 36) break;
            Gdiplus::SolidBrush bBrush(Gdiplus::Color(255, GetRValue(b.color), GetGValue(b.color), GetBValue(b.color)));
            fillRoundedRect(g, bBrush, Gdiplus::RectF(14.0f, (float)curY + 2.0f, 20.0f, 20.0f), 5.0f);

            wchar_t lbuf[2] = { b.letter, 0 };
            Gdiplus::Font lf(L"Segoe UI Variable Text", 8.5f, Gdiplus::FontStyleBold);
            Gdiplus::StringFormat sf;
            sf.SetAlignment(Gdiplus::StringAlignmentCenter);
            sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(lbuf, -1, &lf, Gdiplus::RectF(14.0f, (float)curY + 2.0f, 20.0f, 20.0f), &sf, &textPrim);

            g.DrawString(b.title.c_str(), -1, &itemFont, Gdiplus::PointF(42.0f, (float)curY + 3.0f), &textPrim);
            curY += 32;
        }
    } else if (m_activeSidebarTab == SidebarTab::ReadingList) {
        int curY = 168;
        for (const auto& r : m_readingList) {
            if (curY > height - 50) break;
            g.DrawString(r.title.c_str(), -1, &itemFont, Gdiplus::PointF(14.0f, (float)curY), &textPrim);
            Gdiplus::Font exFont(L"Segoe UI Variable Text", 8.5f, Gdiplus::FontStyleRegular);
            g.DrawString(r.excerpt.c_str(), -1, &exFont, Gdiplus::RectF(14.0f, (float)curY + 18.0f, 232.0f, 32.0f), nullptr, &textSub);
            curY += 56;
        }
    }
}

void Browser::paintDownloadsPopover(Gdiplus::Graphics& g) {
    Gdiplus::RectF popF(
        (float)m_downloadsPopoverRect.left, (float)m_downloadsPopoverRect.top,
        (float)(m_downloadsPopoverRect.right - m_downloadsPopoverRect.left),
        (float)(m_downloadsPopoverRect.bottom - m_downloadsPopoverRect.top));

    Gdiplus::SolidBrush popBg(colors::PopoverBg);
    fillRoundedRect(g, popBg, popF, 12.0f);

    Gdiplus::Pen border(colors::PopoverBorder, 1.0f);
    drawRoundedRect(g, border, popF, 12.0f);

    // Popover Header
    Gdiplus::Font headerFont(L"Segoe UI Variable Text", 10.5f, Gdiplus::FontStyleBold);
    Gdiplus::SolidBrush textPrim(colors::TextPrimary);
    g.DrawString(L"Downloads", -1, &headerFont, Gdiplus::PointF(popF.X + 16.0f, popF.Y + 12.0f), &textPrim);

    Gdiplus::Font clearFont(L"Segoe UI Variable Text", 9.0f, Gdiplus::FontStyleRegular);
    Gdiplus::SolidBrush blueBrush(colors::AccentBlue);
    g.DrawString(L"Clear", -1, &clearFont, Gdiplus::PointF(popF.X + popF.Width - 46.0f, popF.Y + 14.0f), &blueBrush);

    const auto& items = DownloadsManager::instance().items();
    if (items.empty()) {
        Gdiplus::Font emptyFont(L"Segoe UI Variable Text", 9.5f, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush muted(colors::TextSecondary);
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"No recent downloads", -1, &emptyFont, Gdiplus::RectF(popF.X, popF.Y + 100.0f, popF.Width, 30.0f), &sf, &muted);
    } else {
        int curY = (int)popF.Y + 44;
        Gdiplus::Font nameFont(L"Segoe UI Variable Text", 9.5f, Gdiplus::FontStyleBold);
        Gdiplus::Font sizeFont(L"Segoe UI Variable Text", 8.5f, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush subBrush(colors::TextSecondary);

        for (const auto& item : items) {
            if (curY > popF.Y + popF.Height - 50) break;

            // Card
            Gdiplus::SolidBrush cardBg(Gdiplus::Color(255, 48, 48, 54));
            fillRoundedRect(g, cardBg, Gdiplus::RectF(popF.X + 12.0f, (float)curY, popF.Width - 24.0f, 44.0f), 6.0f);

            // Document icon
            Gdiplus::Pen docPen(colors::TextSecondary, 1.4f);
            g.DrawRectangle(&docPen, popF.X + 20.0f, (float)curY + 10.0f, 14.0f, 18.0f);

            // Filename
            g.DrawString(item.filename.c_str(), -1, &nameFont, Gdiplus::PointF(popF.X + 44.0f, (float)curY + 6.0f), &textPrim);

            // Status
            wchar_t sbuf[64];
            if (item.state == DownloadState::Completed) {
                swprintf(sbuf, 64, L"Completed • %.1f MB", item.totalBytes / 1048576.0);
            } else {
                swprintf(sbuf, 64, L"Downloading… %.1f MB", item.receivedBytes / 1048576.0);
            }
            g.DrawString(sbuf, -1, &sizeFont, Gdiplus::PointF(popF.X + 44.0f, (float)curY + 24.0f), &subBrush);

            // Magnifier Icon (Reveal in Explorer)
            g.DrawEllipse(&docPen, popF.X + popF.Width - 36.0f, (float)curY + 14.0f, 10.0f, 10.0f);
            g.DrawLine(&docPen, popF.X + popF.Width - 29.0f, (float)curY + 21.0f, popF.X + popF.Width - 25.0f, (float)curY + 25.0f);

            curY += 50;
        }
    }

    // Footer: Open Downloads Folder
    Gdiplus::Pen divPen(colors::PopoverBorder, 1.0f);
    g.DrawLine(&divPen, (int)popF.X, (int)popF.Y + (int)popF.Height - 38, (int)popF.X + (int)popF.Width, (int)popF.Y + (int)popF.Height - 38);

    Gdiplus::Font footFont(L"Segoe UI Variable Text", 9.0f, Gdiplus::FontStyleRegular);
    Gdiplus::StringFormat footSf;
    footSf.SetAlignment(Gdiplus::StringAlignmentCenter);
    footSf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(L"Open Downloads Folder", -1, &footFont, Gdiplus::RectF(popF.X, popF.Y + popF.Height - 36.0f, popF.Width, 32.0f), &footSf, &blueBrush);
}

void Browser::onMouseMove(POINT pt) {
    m_hoverTraffic = 0;
    if (PtInRect(&m_btnTrafficClose, pt)) m_hoverTraffic = 1;
    else if (PtInRect(&m_btnTrafficMin, pt)) m_hoverTraffic = 2;
    else if (PtInRect(&m_btnTrafficMax, pt)) m_hoverTraffic = 3;

    m_hoverBack = PtInRect(&m_btnNavBack, pt);
    m_hoverForward = PtInRect(&m_btnNavForward, pt);
    m_hoverReload = PtInRect(&m_btnReload, pt);

    m_hoverTab = -1;
    m_hoverTabClose = -1;
    for (size_t i = 0; i < m_tabRects.size(); ++i) {
        if (PtInRect(&m_tabCloseRects[i], pt)) {
            m_hoverTabClose = (int)i;
        } else if (PtInRect(&m_tabRects[i], pt)) {
            m_hoverTab = (int)i;
        }
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Browser::onMouseDown(POINT pt) {
    // Traffic lights
    if (PtInRect(&m_btnTrafficClose, pt)) {
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
        return;
    }
    if (PtInRect(&m_btnTrafficMin, pt)) {
        ShowWindow(m_hwnd, SW_MINIMIZE);
        return;
    }
    if (PtInRect(&m_btnTrafficMax, pt)) {
        ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        return;
    }

    // Sidebar Toggle
    if (PtInRect(&m_btnSidebarToggle, pt)) {
        toggleSidebar();
        return;
    }

    // Back / Forward Nav
    if (PtInRect(&m_btnNavBack, pt)) {
        if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].webView) {
            m_tabs[m_activeTab].webView->goBack();
        }
        return;
    }
    if (PtInRect(&m_btnNavForward, pt)) {
        if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].webView) {
            m_tabs[m_activeTab].webView->goForward();
        }
        return;
    }

    // Reload
    if (PtInRect(&m_btnReload, pt)) {
        if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].webView) {
            m_tabs[m_activeTab].webView->reload();
        }
        return;
    }

    // Address Bar click -> show edit control
    if (PtInRect(&m_addressBarRect, pt)) {
        int x = m_addressBarRect.left + 50;
        int y = m_addressBarRect.top + 7;
        int w = (m_addressBarRect.right - m_addressBarRect.left) - 90;
        int h = 20;

        SetWindowPos(m_editAddress, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);
        std::wstring curUrl = (m_activeTab < m_tabs.size()) ? m_tabs[m_activeTab].url : L"";
        if (curUrl == L"safari:start") curUrl = L"";
        SetWindowTextW(m_editAddress, curUrl.c_str());
        SetFocus(m_editAddress);
        SendMessageW(m_editAddress, EM_SETSEL, 0, -1);
        return;
    }

    // Downloads Button
    if (PtInRect(&m_btnDownloads, pt)) {
        toggleDownloadsPopover();
        return;
    }

    // New Tab Toolbar (+)
    if (PtInRect(&m_btnNewTabToolbar, pt)) {
        createTab(L"safari:start");
        return;
    }

    // Tab Bar click
    for (size_t i = 0; i < m_tabRects.size(); ++i) {
        if (PtInRect(&m_tabCloseRects[i], pt)) {
            closeTab(i);
            return;
        }
        if (PtInRect(&m_tabRects[i], pt)) {
            selectTab(i);
            return;
        }
    }

    // New Tab in Bar (+)
    if (PtInRect(&m_btnNewTabInBar, pt)) {
        createTab(L"safari:start");
        return;
    }

    // Sidebar Segments click
    if (m_sidebarOpen) {
        if (PtInRect(&m_segBookmarksRect, pt)) {
            m_activeSidebarTab = SidebarTab::Bookmarks;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        if (PtInRect(&m_segReadingRect, pt)) {
            m_activeSidebarTab = SidebarTab::ReadingList;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        if (PtInRect(&m_segHistoryRect, pt)) {
            m_activeSidebarTab = SidebarTab::History;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        if (m_activeSidebarTab == SidebarTab::History && PtInRect(&m_btnClearHistoryRect, pt)) {
            m_history.clear();
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }

        // Bookmark list item click
        if (m_activeSidebarTab == SidebarTab::Bookmarks) {
            int curY = 168;
            for (const auto& b : m_bookmarks) {
                RECT r{ 12, curY, 248, curY + 30 };
                if (PtInRect(&r, pt)) {
                    navigate(b.url);
                    return;
                }
                curY += 32;
            }
        } else if (m_activeSidebarTab == SidebarTab::History) {
            int curY = 192;
            for (const auto& h : m_history) {
                RECT r{ 12, curY, 248, curY + 22 };
                if (PtInRect(&r, pt)) {
                    navigate(h.url);
                    return;
                }
                curY += 24;
            }
        }
    }

    // Downloads Popover clicks
    if (m_downloadsPopoverOpen) {
        if (PtInRect(&m_btnClearDownloadsRect, pt)) {
            DownloadsManager::instance().clearCompleted();
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        if (PtInRect(&m_btnOpenFolderRect, pt)) {
            revealFileInExplorer(L"");
            m_downloadsPopoverOpen = false;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
    }
}

void Browser::onMouseUp(POINT) {
}

void Browser::onMouseLeave() {
    m_hoverTraffic = 0;
    m_hoverBack = false;
    m_hoverForward = false;
    m_hoverReload = false;
    m_hoverTab = -1;
    m_hoverTabClose = -1;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

bool Browser::onKeyDown(WPARAM key) {
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        if (key == 'T') {
            createTab(L"safari:start");
            return true;
        }
        if (key == 'W') {
            closeTab(m_activeTab);
            return true;
        }
        if (key == 'R') {
            if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].webView) {
                m_tabs[m_activeTab].webView->reload();
            }
            return true;
        }
        if (key == 'L') {
            POINT pt{ m_addressBarRect.left + 10, m_addressBarRect.top + 10 };
            onMouseDown(pt);
            return true;
        }
    }
    return false;
}

LRESULT CALLBACK Browser::editSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = (Browser*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            wchar_t buffer[2048] = { 0 };
            GetWindowTextW(hwnd, buffer, 2048);
            if (self) self->navigate(buffer);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            ShowWindow(hwnd, SW_HIDE);
            if (self && self->m_activeTab < self->m_tabs.size() && self->m_tabs[self->m_activeTab].webView) {
                self->m_tabs[self->m_activeTab].webView->setFocus();
            }
            return 0;
        }
    }
    if (msg == WM_KILLFOCUS) {
        ShowWindow(hwnd, SW_HIDE);
        if (self) InvalidateRect(self->m_hwnd, nullptr, FALSE);
    }

    return CallWindowProcW(self->m_origEditProc, hwnd, msg, wParam, lParam);
}

} // namespace safari
