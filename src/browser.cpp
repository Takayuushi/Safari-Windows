#include "browser.h"
#include "gfx.h"
#include "util.h"
#include "start_page.h"
#include "settings_page.h"
#include "downloads_page.h"
#include <gdiplus.h>
#include <algorithm>
#include <cstdio>

namespace safari {

Browser::Browser() = default;

Browser::~Browser() {
    if (m_hwndDownloadsPopup && IsWindow(m_hwndDownloadsPopup)) {
        DestroyWindow(m_hwndDownloadsPopup);
        m_hwndDownloadsPopup = nullptr;
    }
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
        if (m_hwndDownloadsPopup && IsWindowVisible(m_hwndDownloadsPopup)) {
            InvalidateRect(m_hwndDownloadsPopup, nullptr, TRUE);
        }
    });

    // Register and create floating downloads popup window
    WNDCLASSEXW pc = { sizeof(WNDCLASSEXW) };
    pc.lpfnWndProc = downloadsPopupProc;
    pc.hInstance = GetModuleHandle(nullptr);
    pc.lpszClassName = L"ChronosDownloadsPopupClass";
    pc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    pc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    pc.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&pc);

    m_hwndDownloadsPopup = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        pc.lpszClassName,
        L"",
        WS_POPUP | WS_CLIPSIBLINGS,
        0, 0, 320, 320,
        m_hwnd, nullptr, GetModuleHandle(nullptr), this);

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

    // Initial Tab with Chronos Start Page
    createTab(L"chronos:start");
    handleResize();
    return true;
}

int Browser::findTabIndex(const WebView* view) const {
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].webView.get() == view) return (int)i;
    }
    return -1;
}

void Browser::createTab(const std::wstring& url) {
    auto tab = Tab{};
    tab.url = url;
    if (url == L"chronos:settings" || url == L"safari:settings") {
        tab.title = L"Settings";
    } else if (url == L"chronos:downloads" || url == L"safari:downloads") {
        tab.title = L"Downloads";
    } else if (url == L"chronos:start" || url == L"safari:start") {
        tab.title = L"Start Page";
    } else {
        tab.title = L"Loading…";
    }
    tab.webView = std::make_unique<WebView>();

    RECT webBounds{ 0, 86, 1200, 800 };
    tab.webView->create(m_hwnd, webBounds);

    size_t newIndex = m_tabs.size();
    auto* viewPtr = tab.webView.get();

    viewPtr->onTitleChanged = [this, viewPtr](const std::wstring& newTitle) {
        int idx = findTabIndex(viewPtr);
        if (idx >= 0 && (size_t)idx < m_tabs.size()) {
            m_tabs[idx].title = newTitle.empty() ? extractDomain(m_tabs[idx].url) : newTitle;
            if (m_activeTab == (size_t)idx) {
                SetWindowTextW(m_hwnd, (m_tabs[idx].title + L" — Chronos").c_str());
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    };

    viewPtr->onUrlChanged = [this, viewPtr](const std::wstring& newUrl) {
        int idx = findTabIndex(viewPtr);
        if (idx >= 0 && (size_t)idx < m_tabs.size()) {
            if (newUrl.starts_with(L"chronos:") || newUrl.starts_with(L"safari:")) {
                handleInternalUrl((size_t)idx, newUrl);
                return;
            }
            m_tabs[idx].url = newUrl;
            if (isWebUrl(newUrl)) {
                recordHistory(m_tabs[idx].title, newUrl);
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    };

    viewPtr->onLoadingChanged = [this, viewPtr](bool loading) {
        int idx = findTabIndex(viewPtr);
        if (idx >= 0 && (size_t)idx < m_tabs.size()) {
            m_tabs[idx].isLoading = loading;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    };

    viewPtr->onDownloadStarted = [this]() {
        // Show compact popover briefly then show progress on toolbar button
        if (!m_downloadsPopoverOpen) {
            toggleDownloadsPopover();
            SetTimer(m_hwnd, 2001, 2500, nullptr);
        } else if (m_hwndDownloadsPopup) {
            InvalidateRect(m_hwndDownloadsPopup, nullptr, TRUE);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    };

    if (url == L"chronos:start" || url == L"safari:start") {
        viewPtr->loadHtml(getStartPageHtml(), "chronos:start");
    } else if (url == L"chronos:settings" || url == L"safari:settings") {
        viewPtr->loadHtml(getSettingsPageHtml(), "chronos:settings");
    } else if (url == L"chronos:downloads" || url == L"safari:downloads") {
        viewPtr->loadHtml(getDownloadsPageHtml(), "chronos:downloads");
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

    handleResize();

    if (m_tabs[m_activeTab].webView) {
        m_tabs[m_activeTab].webView->setVisible(true);
        m_tabs[m_activeTab].webView->setFocus();
    }

    SetWindowTextW(m_hwnd, (m_tabs[m_activeTab].title + L" — Chronos").c_str());
    RedrawWindow(m_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
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

void Browser::handleInternalUrl(size_t tabIdx, const std::wstring& target) {
    if (tabIdx >= m_tabs.size() || !m_tabs[tabIdx].webView) return;

    if (target.starts_with(L"chronos:reveal?id=") || target.starts_with(L"safari:reveal?id=")) {
        size_t eq = target.find(L'=');
        if (eq != std::wstring::npos) {
            uint64_t id = _wcstoui64(target.c_str() + eq + 1, nullptr, 10);
            for (const auto& item : DownloadsManager::instance().items()) {
                if (item.id == id) {
                    revealFileInExplorer(item.filePath);
                    break;
                }
            }
        }
        m_tabs[tabIdx].title = L"Downloads";
        m_tabs[tabIdx].url = L"chronos:downloads";
        m_tabs[tabIdx].webView->loadHtml(getDownloadsPageHtml(), "chronos:downloads");
        return;
    }
    if (target.starts_with(L"chronos:reveal_folder") || target.starts_with(L"safari:reveal_folder")) {
        revealFileInExplorer(L"");
        m_tabs[tabIdx].title = L"Downloads";
        m_tabs[tabIdx].url = L"chronos:downloads";
        m_tabs[tabIdx].webView->loadHtml(getDownloadsPageHtml(), "chronos:downloads");
        return;
    }
    if (target.starts_with(L"chronos:pause?id=") || target.starts_with(L"safari:pause?id=")) {
        size_t eq = target.find(L'=');
        if (eq != std::wstring::npos) {
            uint64_t id = _wcstoui64(target.c_str() + eq + 1, nullptr, 10);
            DownloadsManager::instance().pauseDownload(id);
        }
        m_tabs[tabIdx].title = L"Downloads";
        m_tabs[tabIdx].url = L"chronos:downloads";
        m_tabs[tabIdx].webView->loadHtml(getDownloadsPageHtml(), "chronos:downloads");
        return;
    }
    if (target.starts_with(L"chronos:resume?id=") || target.starts_with(L"safari:resume?id=")) {
        size_t eq = target.find(L'=');
        if (eq != std::wstring::npos) {
            uint64_t id = _wcstoui64(target.c_str() + eq + 1, nullptr, 10);
            DownloadsManager::instance().resumeDownload(id);
        }
        m_tabs[tabIdx].title = L"Downloads";
        m_tabs[tabIdx].url = L"chronos:downloads";
        m_tabs[tabIdx].webView->loadHtml(getDownloadsPageHtml(), "chronos:downloads");
        return;
    }
    if (target.starts_with(L"chronos:cancel?id=") || target.starts_with(L"safari:cancel?id=")) {
        size_t eq = target.find(L'=');
        if (eq != std::wstring::npos) {
            uint64_t id = _wcstoui64(target.c_str() + eq + 1, nullptr, 10);
            DownloadsManager::instance().cancelDownload(id);
        }
        m_tabs[tabIdx].title = L"Downloads";
        m_tabs[tabIdx].url = L"chronos:downloads";
        m_tabs[tabIdx].webView->loadHtml(getDownloadsPageHtml(), "chronos:downloads");
        return;
    }
    if (target.starts_with(L"chronos:remove?id=") || target.starts_with(L"safari:remove?id=")) {
        size_t eq = target.find(L'=');
        if (eq != std::wstring::npos) {
            uint64_t id = _wcstoui64(target.c_str() + eq + 1, nullptr, 10);
            DownloadsManager::instance().removeDownload(id);
        }
        m_tabs[tabIdx].title = L"Downloads";
        m_tabs[tabIdx].url = L"chronos:downloads";
        m_tabs[tabIdx].webView->loadHtml(getDownloadsPageHtml(), "chronos:downloads");
        return;
    }
    if (target == L"chronos:clear" || target == L"safari:clear") {
        DownloadsManager::instance().clearCompleted();
        m_tabs[tabIdx].title = L"Downloads";
        m_tabs[tabIdx].url = L"chronos:downloads";
        m_tabs[tabIdx].webView->loadHtml(getDownloadsPageHtml(), "chronos:downloads");
        return;
    }
    if (target == L"chronos:start" || target == L"safari:start") {
        m_tabs[tabIdx].title = L"Start Page";
        m_tabs[tabIdx].url = target;
        m_tabs[tabIdx].webView->loadHtml(getStartPageHtml(), "chronos:start");
        return;
    }
    if (target == L"chronos:settings" || target == L"safari:settings") {
        m_tabs[tabIdx].title = L"Settings";
        m_tabs[tabIdx].url = target;
        m_tabs[tabIdx].webView->loadHtml(getSettingsPageHtml(), "chronos:settings");
        return;
    }
    if (target == L"chronos:downloads" || target == L"safari:downloads") {
        m_tabs[tabIdx].title = L"Downloads";
        m_tabs[tabIdx].url = target;
        m_tabs[tabIdx].webView->loadHtml(getDownloadsPageHtml(), "chronos:downloads");
        return;
    }
}

void Browser::navigate(const std::wstring& input) {
    std::wstring target = normalizeInput(input);
    if (m_activeTab < m_tabs.size() && m_tabs[m_activeTab].webView) {
        if (target.starts_with(L"chronos:") || target.starts_with(L"safari:")) {
            handleInternalUrl(m_activeTab, target);
        } else {
            m_tabs[m_activeTab].url = target;
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
    if (m_hwndDownloadsPopup) {
        if (m_downloadsPopoverOpen) {
            int itemCount = (int)DownloadsManager::instance().items().size();
            int popHeight = (itemCount == 0) ? 120 : std::clamp(48 + itemCount * 52 + 38, 138, 300);

            POINT pt{ m_btnDownloads.left - 240, 50 };
            ClientToScreen(m_hwnd, &pt);
            SetWindowPos(m_hwndDownloadsPopup, HWND_TOPMOST, pt.x, pt.y, 320, popHeight, SWP_SHOWWINDOW);
            SetFocus(m_hwndDownloadsPopup);
            InvalidateRect(m_hwndDownloadsPopup, nullptr, TRUE);
        } else {
            ShowWindow(m_hwndDownloadsPopup, SW_HIDE);
        }
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Browser::handleResize() {
    if (!m_hwnd) return;

    RECT client{};
    GetClientRect(m_hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    int sidebarWidth = m_sidebarOpen ? m_sidebarWidth : 0;
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
        int maxTabW = 240;
        int minTabW = 120;
        int tabW = std::clamp((availWidth - (int)(count - 1) * 4) / (int)count, minTabW, maxTabW);
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
        int sbW = m_sidebarWidth;
        m_sidebarRect = RECT{ 0, 86, sbW, height };
        m_sidebarSplitterRect = RECT{ sbW - 4, 86, sbW + 4, height };

        int totalSegW = sbW - 24;
        int segW = (totalSegW - 8) / 3;
        m_segBookmarksRect = RECT{ 12, 96, 12 + segW, 124 };
        m_segReadingRect   = RECT{ 16 + segW, 96, 16 + segW * 2, 124 };
        m_segHistoryRect   = RECT{ 20 + segW * 2, 96, sbW - 12, 124 };
        m_sidebarSearchRect = RECT{ 12, 132, sbW - 12, 160 };
        m_btnClearHistoryRect = RECT{ 14, 166, 120, 186 };
        m_btnSettings = RECT{ 12, height - 44, sbW - 12, height - 10 };
    } else {
        m_sidebarRect = {};
        m_sidebarSplitterRect = {};
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
    // Downloads Button (Safari circular progress indicator)
    float dlCenterX = (float)m_btnDownloads.left + 16.0f;
    float dlCenterY = (float)m_btnDownloads.top + 16.0f;
    float dlRadius = 9.0f;

    bool hasDownloads = DownloadsManager::instance().hasActiveDownloads();
    double overallProg = DownloadsManager::instance().overallProgress();

    if (hasDownloads) {
        // Track ring
        Gdiplus::Pen trackPen(Gdiplus::Color(255, 65, 65, 75), 2.0f);
        g.DrawEllipse(&trackPen, dlCenterX - dlRadius, dlCenterY - dlRadius, dlRadius * 2.0f, dlRadius * 2.0f);

        // Progress sweep ring (starts from 12 o'clock / -90 deg, sweeps clockwise)
        float sweepAngle = std::max(20.0f, (float)(overallProg * 360.0));
        Gdiplus::Pen progressPen(colors::AccentBlue, 2.2f);
        g.DrawArc(&progressPen, dlCenterX - dlRadius, dlCenterY - dlRadius, dlRadius * 2.0f, dlRadius * 2.0f, -90.0f, sweepAngle);

        // Downward Arrow in center
        Gdiplus::Pen arrowPen(colors::AccentBlue, 1.6f);
        g.DrawLine(&arrowPen, dlCenterX, dlCenterY - 4.0f, dlCenterX, dlCenterY + 4.0f);
        Gdiplus::PointF arrowHead[3] = {
            { dlCenterX - 3.5f, dlCenterY + 1.0f },
            { dlCenterX, dlCenterY + 4.5f },
            { dlCenterX + 3.5f, dlCenterY + 1.0f }
        };
        g.DrawLines(&arrowPen, arrowHead, 3);
    } else {
        Gdiplus::Pen dlPen(m_hoverDownloads ? colors::TextPrimary : colors::TextSecondary, 1.6f);
        g.DrawEllipse(&dlPen, dlCenterX - dlRadius, dlCenterY - dlRadius, dlRadius * 2.0f, dlRadius * 2.0f);
        g.DrawLine(&dlPen, dlCenterX, dlCenterY - 5.0f, dlCenterX, dlCenterY + 3.0f);
        Gdiplus::PointF arrowHead[3] = {
            { dlCenterX - 4.0f, dlCenterY },
            { dlCenterX, dlCenterY + 4.0f },
            { dlCenterX + 4.0f, dlCenterY }
        };
        g.DrawLines(&dlPen, arrowHead, 3);
    }

    // Share Button
    Gdiplus::Pen toolPen(colors::TextSecondary, 1.6f);
    g.DrawRectangle(&toolPen, (int)m_btnShare.left + 8, (int)m_btnShare.top + 13, 14, 11);
    g.DrawLine(&toolPen, (int)m_btnShare.left + 15, (int)m_btnShare.top + 7, (int)m_btnShare.left + 15, (int)m_btnShare.top + 16);

    // New Tab Toolbar (+)
    g.DrawLine(&toolPen, (int)m_btnNewTabToolbar.left + 16, (int)m_btnNewTabToolbar.top + 10, (int)m_btnNewTabToolbar.left + 16, (int)m_btnNewTabToolbar.top + 22);
    g.DrawLine(&toolPen, (int)m_btnNewTabToolbar.left + 10, (int)m_btnNewTabToolbar.top + 16, (int)m_btnNewTabToolbar.left + 22, (int)m_btnNewTabToolbar.top + 16);

    // Overview Button (Show All Tabs - Safari Two Overlapping Cards)
    Gdiplus::Pen ovPen(m_hoverOverview ? colors::TextPrimary : colors::TextSecondary, 1.4f);
    Gdiplus::SolidBrush ovBg(colors::ChromeBg);
    fillRoundedRect(g, ovBg, Gdiplus::RectF((float)m_btnOverview.left + 7.0f, (float)m_btnOverview.top + 9.0f, 13.0f, 11.0f), 2.0f);
    drawRoundedRect(g, ovPen, Gdiplus::RectF((float)m_btnOverview.left + 7.0f, (float)m_btnOverview.top + 9.0f, 13.0f, 11.0f), 2.0f);
    fillRoundedRect(g, ovBg, Gdiplus::RectF((float)m_btnOverview.left + 12.0f, (float)m_btnOverview.top + 13.0f, 13.0f, 11.0f), 2.0f);
    drawRoundedRect(g, ovPen, Gdiplus::RectF((float)m_btnOverview.left + 12.0f, (float)m_btnOverview.top + 13.0f, 13.0f, 11.0f), 2.0f);
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
        tf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        tf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        tf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF textRf(rf.X + 28.0f, rf.Y, rf.Width - 48.0f, rf.Height);
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
    int sbW = m_sidebarWidth;
    Gdiplus::SolidBrush sideBg(colors::SidebarBg);
    g.FillRectangle(&sideBg, 0, 86, sbW, height - 86);

    Gdiplus::Pen border(colors::SidebarBorder, 1.0f);
    g.DrawLine(&border, sbW - 1, 86, sbW - 1, (int)height);

    // Splitter border highlight
    Gdiplus::Pen splitPen(Gdiplus::Color(45, 255, 255, 255), 1.0f);
    g.DrawLine(&splitPen, sbW - 2, 86, sbW - 2, (int)height);

    // Segment Switcher (Bookmarks / Reading List / History)
    float totalSegW = (float)(sbW - 24);
    float segW = (totalSegW - 8.0f) / 3.0f;
    Gdiplus::SolidBrush segBg(colors::SidebarSegment);
    fillRoundedRect(g, segBg, Gdiplus::RectF(12.0f, 96.0f, totalSegW, 30.0f), 7.0f);

    Gdiplus::SolidBrush activeSeg(colors::SidebarActiveSeg);
    if (m_activeSidebarTab == SidebarTab::Bookmarks) {
        fillRoundedRect(g, activeSeg, Gdiplus::RectF(14.0f, 98.0f, segW, 26.0f), 5.0f);
    } else if (m_activeSidebarTab == SidebarTab::ReadingList) {
        fillRoundedRect(g, activeSeg, Gdiplus::RectF(14.0f + segW + 4.0f, 98.0f, segW, 26.0f), 5.0f);
    } else {
        fillRoundedRect(g, activeSeg, Gdiplus::RectF(14.0f + (segW + 4.0f) * 2.0f, 98.0f, segW, 26.0f), 5.0f);
    }

    // Segment Labels
    Gdiplus::Font segFont(L"Segoe UI Variable Text", 9.5f, Gdiplus::FontStyleBold);
    Gdiplus::SolidBrush textWhite(colors::TextPrimary);
    Gdiplus::SolidBrush textMuted(colors::TextSecondary);
    Gdiplus::StringFormat sfCenter;
    sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
    sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    g.DrawString(L"Bookmarks", -1, &segFont, Gdiplus::RectF(14.0f, 98.0f, segW, 26.0f), &sfCenter,
                 (m_activeSidebarTab == SidebarTab::Bookmarks) ? &textWhite : &textMuted);
    g.DrawString(L"Reading", -1, &segFont, Gdiplus::RectF(14.0f + segW + 4.0f, 98.0f, segW, 26.0f), &sfCenter,
                 (m_activeSidebarTab == SidebarTab::ReadingList) ? &textWhite : &textMuted);
    g.DrawString(L"History", -1, &segFont, Gdiplus::RectF(14.0f + (segW + 4.0f) * 2.0f, 98.0f, segW, 26.0f), &sfCenter,
                 (m_activeSidebarTab == SidebarTab::History) ? &textWhite : &textMuted);

    // Search Box (28px height, modern rounded)
    Gdiplus::SolidBrush searchBg(colors::AddressBg);
    fillRoundedRect(g, searchBg, Gdiplus::RectF(12.0f, 134.0f, (float)(sbW - 24), 28.0f), 7.0f);

    Gdiplus::Font searchFont(L"Segoe UI Variable Text", 9.5f, Gdiplus::FontStyleRegular);
    Gdiplus::SolidBrush phBrush(colors::TextMuted);
    Gdiplus::Pen glassPen(colors::TextMuted, 1.3f);
    g.DrawEllipse(&glassPen, 22.0f, 142.0f, 7.0f, 7.0f);
    g.DrawLine(&glassPen, 27.0f, 147.0f, 31.0f, 151.0f);

    const wchar_t* searchPh = (m_activeSidebarTab == SidebarTab::Bookmarks) ? L"Search Bookmarks" :
                              (m_activeSidebarTab == SidebarTab::ReadingList) ? L"Search Reading List" : L"Search History";
    g.DrawString(searchPh, -1, &searchFont, Gdiplus::PointF(36.0f, 139.0f), &phBrush);

    // Section Header Tag & Items List
    Gdiplus::Font sectionHeaderFont(L"Segoe UI Variable Text", 8.5f, Gdiplus::FontStyleBold);
    Gdiplus::SolidBrush headerTagBrush(Gdiplus::Color(255, 142, 142, 147));
    Gdiplus::Font itemFont(L"Segoe UI Variable Text", 10.0f, Gdiplus::FontStyleRegular);
    Gdiplus::Font itemBoldFont(L"Segoe UI Variable Text", 10.0f, Gdiplus::FontStyleBold);
    Gdiplus::Font subFont(L"Segoe UI Variable Text", 8.5f, Gdiplus::FontStyleRegular);

    if (m_activeSidebarTab == SidebarTab::History) {
        g.DrawString(L"TODAY", -1, &sectionHeaderFont, Gdiplus::PointF(14.0f, 172.0f), &headerTagBrush);

        Gdiplus::Font clearFont(L"Segoe UI Variable Text", 8.5f, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush blueBrush(colors::AccentBlue);
        Gdiplus::StringFormat sfFar;
        sfFar.SetAlignment(Gdiplus::StringAlignmentFar);
        g.DrawString(L"Clear History\x2026", -1, &clearFont, Gdiplus::RectF(14.0f, 172.0f, (float)(sbW - 28), 18.0f), &sfFar, &blueBrush);

        int curY = 196;
        for (const auto& h : m_history) {
            if (curY > height - 60) break;

            Gdiplus::SolidBrush circleBg(Gdiplus::Color(255, 48, 48, 54));
            fillRoundedRect(g, circleBg, Gdiplus::RectF(14.0f, (float)curY + 2.0f, 20.0f, 20.0f), 5.0f);

            wchar_t lbuf[2] = { (wchar_t)::towupper(h.domain.empty() ? L'H' : h.domain[0]), 0 };
            Gdiplus::Font lf(L"Segoe UI Variable Text", 8.5f, Gdiplus::FontStyleBold);
            Gdiplus::StringFormat sfCenterBadge;
            sfCenterBadge.SetAlignment(Gdiplus::StringAlignmentCenter);
            sfCenterBadge.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(lbuf, -1, &lf, Gdiplus::RectF(14.0f, (float)curY + 2.0f, 20.0f, 20.0f), &sfCenterBadge, &textWhite);

            g.DrawString(h.domain.c_str(), -1, &itemFont, Gdiplus::PointF(40.0f, (float)curY + 3.0f), &textWhite);

            wchar_t tbuf[32];
            swprintf(tbuf, 32, L"%d:%02d %ls", (h.time.wHour % 12 == 0 ? 12 : h.time.wHour % 12), h.time.wMinute, (h.time.wHour >= 12 ? L"PM" : L"AM"));
            g.DrawString(tbuf, -1, &subFont, Gdiplus::RectF(14.0f, (float)curY + 4.0f, (float)(sbW - 28), 20.0f), &sfFar, &textMuted);

            curY += 30;
        }
    } else if (m_activeSidebarTab == SidebarTab::Bookmarks) {
        g.DrawString(L"FAVORITES", -1, &sectionHeaderFont, Gdiplus::PointF(14.0f, 172.0f), &headerTagBrush);

        int curY = 196;
        for (const auto& b : m_bookmarks) {
            if (curY > height - 60) break;
            Gdiplus::SolidBrush bBrush(Gdiplus::Color(255, GetRValue(b.color), GetGValue(b.color), GetBValue(b.color)));
            fillRoundedRect(g, bBrush, Gdiplus::RectF(14.0f, (float)curY + 2.0f, 22.0f, 22.0f), 6.0f);

            wchar_t lbuf[2] = { b.letter, 0 };
            Gdiplus::Font lf(L"Segoe UI Variable Text", 9.0f, Gdiplus::FontStyleBold);
            Gdiplus::StringFormat sf;
            sf.SetAlignment(Gdiplus::StringAlignmentCenter);
            sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(lbuf, -1, &lf, Gdiplus::RectF(14.0f, (float)curY + 2.0f, 22.0f, 22.0f), &sf, &textWhite);

            g.DrawString(b.title.c_str(), -1, &itemFont, Gdiplus::PointF(44.0f, (float)curY + 4.0f), &textWhite);
            curY += 34;
        }
    } else if (m_activeSidebarTab == SidebarTab::ReadingList) {
        g.DrawString(L"UNREAD", -1, &sectionHeaderFont, Gdiplus::PointF(14.0f, 172.0f), &headerTagBrush);

        int curY = 196;
        for (const auto& r : m_readingList) {
            if (curY > height - 70) break;
            g.DrawString(r.title.c_str(), -1, &itemBoldFont, Gdiplus::PointF(14.0f, (float)curY), &textWhite);
            g.DrawString(r.excerpt.c_str(), -1, &subFont, Gdiplus::RectF(14.0f, (float)curY + 20.0f, (float)(sbW - 28), 32.0f), nullptr, &textMuted);
            curY += 58;
        }
    }

    // Settings Button at bottom of sidebar (Gear icon)
    Gdiplus::RectF setBtnF((float)m_btnSettings.left, (float)m_btnSettings.top,
                           (float)(m_btnSettings.right - m_btnSettings.left),
                           (float)(m_btnSettings.bottom - m_btnSettings.top));

    if (m_hoverSettings) {
        Gdiplus::SolidBrush hoverBrush(Gdiplus::Color(255, 48, 48, 54));
        fillRoundedRect(g, hoverBrush, setBtnF, 6.0f);
    }

    Gdiplus::Pen sepPen(colors::SidebarBorder, 1.0f);
    g.DrawLine(&sepPen, 12, (int)m_btnSettings.top - 6, sbW - 12, (int)m_btnSettings.top - 6);

    Gdiplus::Font gearFont(L"Segoe UI Symbol", 11.5f, Gdiplus::FontStyleRegular);
    g.DrawString(L"\x2699", -1, &gearFont, Gdiplus::PointF(setBtnF.X + 8.0f, setBtnF.Y + 4.0f), &textWhite);

    Gdiplus::Font setLabelFont(L"Segoe UI Variable Text", 9.5f, Gdiplus::FontStyleRegular);
    g.DrawString(L"Settings", -1, &setLabelFont, Gdiplus::PointF(setBtnF.X + 30.0f, setBtnF.Y + 6.0f), &textWhite);

    Gdiplus::StringFormat sfFar;
    sfFar.SetAlignment(Gdiplus::StringAlignmentFar);
    g.DrawString(L"\x203A", -1, &setLabelFont, Gdiplus::RectF(setBtnF.X, setBtnF.Y + 4.0f, setBtnF.Width - 8.0f, setBtnF.Height), &sfFar, &textMuted);
}

void Browser::paintDownloadsPopup(HDC hdc) {
    RECT client{};
    GetClientRect(m_hwndDownloadsPopup, &client);
    int popWidth = client.right - client.left;
    int popHeight = client.bottom - client.top;

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // Popover rounded background
    Gdiplus::SolidBrush bgBrush(colors::PopoverBg);
    fillRoundedRect(g, bgBrush, Gdiplus::RectF(0.0f, 0.0f, (float)popWidth, (float)popHeight), 12.0f);

    Gdiplus::Pen borderPen(colors::PopoverBorder, 1.0f);
    drawRoundedRect(g, borderPen, Gdiplus::RectF(0.0f, 0.0f, (float)popWidth - 1.0f, (float)popHeight - 1.0f), 12.0f);

    // Header
    Gdiplus::Font headerFont(L"Segoe UI Variable Text", 11.0f, Gdiplus::FontStyleBold);
    Gdiplus::SolidBrush textPrim(colors::TextPrimary);
    g.DrawString(L"Downloads", -1, &headerFont, Gdiplus::PointF(16.0f, 14.0f), &textPrim);

    Gdiplus::Font clearFont(L"Segoe UI Variable Text", 9.5f, Gdiplus::FontStyleRegular);
    Gdiplus::SolidBrush blueBrush(colors::AccentBlue);
    g.DrawString(L"Clear", -1, &clearFont, Gdiplus::PointF((float)popWidth - 50.0f, 16.0f), &blueBrush);

    const auto& items = DownloadsManager::instance().items();
    if (items.empty()) {
        Gdiplus::Font emptyFont(L"Segoe UI Variable Text", 10.0f, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush muted(colors::TextSecondary);
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"No recent downloads", -1, &emptyFont, Gdiplus::RectF(0.0f, 50.0f, (float)popWidth, 30.0f), &sf, &muted);
    } else {
        int curY = 48;
        Gdiplus::Font nameFont(L"Segoe UI Variable Text", 10.0f, Gdiplus::FontStyleBold);
        Gdiplus::Font sizeFont(L"Segoe UI Variable Text", 8.5f, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush subBrush(colors::TextSecondary);

        for (const auto& item : items) {
            if (curY > popHeight - 50) break;

            Gdiplus::SolidBrush cardBg(Gdiplus::Color(255, 48, 48, 54));
            fillRoundedRect(g, cardBg, Gdiplus::RectF(12.0f, (float)curY, (float)popWidth - 24.0f, 46.0f), 7.0f);

            Gdiplus::Pen docPen(colors::TextSecondary, 1.4f);
            g.DrawRectangle(&docPen, 22.0f, (float)curY + 12.0f, 14.0f, 18.0f);

            g.DrawString(item.filename.c_str(), -1, &nameFont, Gdiplus::PointF(46.0f, (float)curY + 8.0f), &textPrim);

            wchar_t sbuf[64];
            if (item.state == DownloadState::Completed) {
                swprintf(sbuf, 64, L"Completed \x2022 %.1f MB", (item.totalBytes > 0 ? item.totalBytes : item.receivedBytes) / 1048576.0);
            } else if (item.state == DownloadState::InProgress) {
                if (item.totalBytes > 0) {
                    double pct = (double)item.receivedBytes / (double)item.totalBytes * 100.0;
                    swprintf(sbuf, 64, L"Downloading\x2026 %.1f / %.1f MB (%.0f%%)", item.receivedBytes / 1048576.0, item.totalBytes / 1048576.0, pct);
                } else {
                    swprintf(sbuf, 64, L"Downloading\x2026 %.1f MB", item.receivedBytes / 1048576.0);
                }
            } else if (item.state == DownloadState::Paused) {
                swprintf(sbuf, 64, L"Paused \x2022 %.1f MB", item.receivedBytes / 1048576.0);
            } else if (item.state == DownloadState::Cancelled) {
                swprintf(sbuf, 64, L"Cancelled");
            } else {
                swprintf(sbuf, 64, L"Failed");
            }
            g.DrawString(sbuf, -1, &sizeFont, Gdiplus::PointF(46.0f, (float)curY + 24.0f), &subBrush);

            // Mini progress bar for in-progress or paused downloads
            if (item.state == DownloadState::InProgress || item.state == DownloadState::Paused) {
                float barW = (float)popWidth - 110.0f;
                float pct = 0.0f;
                if (item.totalBytes > 0) {
                    pct = (float)((double)item.receivedBytes / (double)item.totalBytes);
                    if (pct > 1.0f) pct = 1.0f;
                }
                Gdiplus::SolidBrush barBg(Gdiplus::Color(255, 60, 60, 66));
                fillRoundedRect(g, barBg, Gdiplus::RectF(46.0f, (float)curY + 38.0f, barW, 3.0f), 1.5f);
                if (pct > 0.0f) {
                    Gdiplus::SolidBrush barFill(item.state == DownloadState::Paused ? Gdiplus::Color(255, 255, 159, 10) : colors::AccentBlue);
                    fillRoundedRect(g, barFill, Gdiplus::RectF(46.0f, (float)curY + 38.0f, barW * pct, 3.0f), 1.5f);
                }
            }

            g.DrawEllipse(&docPen, (float)popWidth - 38.0f, (float)curY + 16.0f, 10.0f, 10.0f);
            g.DrawLine(&docPen, (float)popWidth - 31.0f, (float)curY + 23.0f, (float)popWidth - 27.0f, (float)curY + 27.0f);

            curY += 52;
        }
    }

    // Footer: Show All Downloads
    Gdiplus::Pen divPen(colors::PopoverBorder, 1.0f);
    g.DrawLine(&divPen, 0, popHeight - 38, popWidth, popHeight - 38);

    Gdiplus::Font footFont(L"Segoe UI Variable Text", 9.5f, Gdiplus::FontStyleRegular);
    Gdiplus::StringFormat footSf;
    footSf.SetAlignment(Gdiplus::StringAlignmentCenter);
    footSf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(L"Show All Downloads", -1, &footFont, Gdiplus::RectF(0.0f, (float)popHeight - 36.0f, (float)popWidth, 32.0f), &footSf, &blueBrush);
}

void Browser::onDownloadsPopupClick(POINT pt) {
    RECT client{};
    GetClientRect(m_hwndDownloadsPopup, &client);
    int popHeight = client.bottom - client.top;

    if (pt.x >= 260 && pt.x <= 310 && pt.y >= 10 && pt.y <= 36) {
        DownloadsManager::instance().clearCompleted();
        if (m_hwndDownloadsPopup) InvalidateRect(m_hwndDownloadsPopup, nullptr, TRUE);
        return;
    }
    // "Show All Downloads" footer
    if (pt.y >= popHeight - 38) {
        toggleDownloadsPopover();
        createTab(L"chronos:downloads");
        return;
    }
    int curY = 48;
    const auto& items = DownloadsManager::instance().items();
    for (const auto& item : items) {
        if (curY > popHeight - 50) break;
        RECT itemRect{ 12, curY, 308, curY + 46 };
        if (PtInRect(&itemRect, pt)) {
            revealFileInExplorer(item.filePath);
            toggleDownloadsPopover();
            return;
        }
        curY += 52;
    }
}

LRESULT CALLBACK Browser::downloadsPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = (Browser*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (self) self->paintDownloadsPopup(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (self) self->onDownloadsPopupClick(pt);
        return 0;
    }
    case WM_KILLFOCUS: {
        if (self) {
            self->m_downloadsPopoverOpen = false;
            ShowWindow(hwnd, SW_HIDE);
            InvalidateRect(self->m_hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            if (self) {
                self->m_downloadsPopoverOpen = false;
                ShowWindow(hwnd, SW_HIDE);
                InvalidateRect(self->m_hwnd, nullptr, FALSE);
            }
            return 0;
        }
        break;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Browser::onMouseMove(POINT pt) {
    m_lastMousePos = pt;

    if (m_isDraggingSidebar) {
        SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        m_sidebarWidth = std::clamp((int)pt.x, 240, 560);
        handleResize();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    m_hoverTraffic = 0;
    if (PtInRect(&m_btnTrafficClose, pt)) m_hoverTraffic = 1;
    else if (PtInRect(&m_btnTrafficMin, pt)) m_hoverTraffic = 2;
    else if (PtInRect(&m_btnTrafficMax, pt)) m_hoverTraffic = 3;

    m_hoverSidebar = PtInRect(&m_btnSidebarToggle, pt);
    m_hoverBack = PtInRect(&m_btnNavBack, pt);
    m_hoverForward = PtInRect(&m_btnNavForward, pt);
    m_hoverReload = PtInRect(&m_btnReload, pt);
    m_hoverDownloads = PtInRect(&m_btnDownloads, pt);
    m_hoverShare = PtInRect(&m_btnShare, pt);
    m_hoverNewTab = PtInRect(&m_btnNewTabToolbar, pt);
    m_hoverOverview = PtInRect(&m_btnOverview, pt);
    m_hoverSettings = (m_sidebarOpen && PtInRect(&m_btnSettings, pt));

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

    // Sidebar splitter drag
    if (m_sidebarOpen && PtInRect(&m_sidebarSplitterRect, pt)) {
        m_isDraggingSidebar = true;
        SetCapture(m_hwnd);
        SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
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
        if (curUrl == L"chronos:start" || curUrl == L"safari:start") curUrl = L"";
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
        createTab(L"chronos:start");
        return;
    }

    // Tab Overview (Show All Tabs - 4th button)
    if (PtInRect(&m_btnOverview, pt)) {
        if (m_tabs.size() > 1) {
            selectTab((m_activeTab + 1) % m_tabs.size());
        }
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
        createTab(L"chronos:start");
        return;
    }

    // Sidebar Segments click & Settings click
    if (m_sidebarOpen) {
        if (PtInRect(&m_btnSettings, pt)) {
            navigate(L"chronos:settings");
            return;
        }
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
                RECT r{ 12, curY, m_sidebarWidth - 12, curY + 30 };
                if (PtInRect(&r, pt)) {
                    navigate(b.url);
                    return;
                }
                curY += 32;
            }
        } else if (m_activeSidebarTab == SidebarTab::History) {
            int curY = 192;
            for (const auto& h : m_history) {
                RECT r{ 12, curY, m_sidebarWidth - 12, curY + 22 };
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
    if (m_isDraggingSidebar) {
        m_isDraggingSidebar = false;
        ReleaseCapture();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Browser::onMouseLeave() {
    m_hoverTraffic = 0;
    m_hoverBack = false;
    m_hoverForward = false;
    m_hoverReload = false;
    m_hoverSettings = false;
    m_hoverTab = -1;
    m_hoverTabClose = -1;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

bool Browser::onSetCursor() {
    if (m_isDraggingSidebar || (m_sidebarOpen && PtInRect(&m_sidebarSplitterRect, m_lastMousePos))) {
        SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        return true;
    }
    return false;
}

bool Browser::onKeyDown(WPARAM key) {
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        if (key == 'T') {
            createTab(L"chronos:start");
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

bool Browser::isHitInteractive(POINT pt) const {
    if (PtInRect(&m_btnTrafficClose, pt)) return true;
    if (PtInRect(&m_btnTrafficMin, pt)) return true;
    if (PtInRect(&m_btnTrafficMax, pt)) return true;
    if (PtInRect(&m_btnSidebarToggle, pt)) return true;
    if (PtInRect(&m_btnNavBack, pt)) return true;
    if (PtInRect(&m_btnNavForward, pt)) return true;
    if (PtInRect(&m_addressBarRect, pt)) return true;
    if (PtInRect(&m_btnReload, pt)) return true;
    if (PtInRect(&m_btnDownloads, pt)) return true;
    if (PtInRect(&m_btnShare, pt)) return true;
    if (PtInRect(&m_btnNewTabToolbar, pt)) return true;
    if (PtInRect(&m_btnOverview, pt)) return true;

    // Tab bar area is interactive
    if (pt.y >= 50 && pt.y < 86) return true;

    // Downloads Popover
    if (m_downloadsPopoverOpen && PtInRect(&m_downloadsPopoverRect, pt)) return true;

    // Sidebar & Splitter
    if (m_sidebarOpen && (PtInRect(&m_sidebarRect, pt) || PtInRect(&m_sidebarSplitterRect, pt))) return true;

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
