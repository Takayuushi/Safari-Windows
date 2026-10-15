#pragma once

#include <windows.h>
#include <unknwn.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <memory>
#include "webkit_host.h"
#include "downloads.h"

namespace safari {

enum class SidebarTab {
    Bookmarks,
    ReadingList,
    History
};

struct Tab {
    std::unique_ptr<WebView> webView;
    std::wstring title = L"Compass";
    std::wstring url = L"chronos:start";
    bool isLoading = false;
};

struct BookmarkItem {
    std::wstring title;
    std::wstring url;
    std::wstring domain;
    COLORREF color;
    wchar_t letter;
};

struct ReadingItem {
    std::wstring title;
    std::wstring url;
    std::wstring domain;
    std::wstring excerpt;
};

struct HistoryItem {
    std::wstring title;
    std::wstring url;
    std::wstring domain;
    SYSTEMTIME time;
};

class Browser {
public:
    Browser();
    ~Browser();

    bool initialize(HWND hwnd);
    void handleResize();
    void paint(HDC hdc);

    // Mouse & Keyboard interaction
    void onMouseMove(POINT pt);
    void onMouseDown(POINT pt);
    void onMouseUp(POINT pt);
    void onMouseLeave();
    bool onKeyDown(WPARAM key);

    void navigate(const std::wstring& input);
    void createTab(const std::wstring& url = L"chronos:start");
    void closeTab(size_t index);
    void selectTab(size_t index);

    void toggleSidebar();
    void toggleDownloadsPopover();
    bool isDownloadsPopoverOpen() const { return m_downloadsPopoverOpen; }

    bool isHitInteractive(POINT pt) const;
    bool onSetCursor();
    HWND window() const { return m_hwnd; }

private:
    HWND m_hwnd = nullptr;
    HWND m_editAddress = nullptr;
    HWND m_editSidebarSearch = nullptr;
    WNDPROC m_origEditProc = nullptr;
    WNDPROC m_origSearchProc = nullptr;

    UINT m_dpi = 96;
    bool m_sidebarOpen = false;
    bool m_downloadsPopoverOpen = false;
    SidebarTab m_activeSidebarTab = SidebarTab::Bookmarks;
    std::wstring m_sidebarSearchQuery;

    // Tabs
    std::vector<Tab> m_tabs;
    size_t m_activeTab = 0;

    // Data lists
    std::vector<BookmarkItem> m_bookmarks;
    std::vector<ReadingItem> m_readingList;
    std::vector<HistoryItem> m_history;

    // Hit test regions
    RECT m_btnTrafficClose{};
    RECT m_btnTrafficMin{};
    RECT m_btnTrafficMax{};
    RECT m_btnSidebarToggle{};
    RECT m_btnNavBack{};
    RECT m_btnNavForward{};
    RECT m_addressBarRect{};
    RECT m_btnReload{};
    RECT m_btnDownloads{};
    RECT m_btnShare{};
    RECT m_btnNewTabToolbar{};
    RECT m_btnOverview{};

    RECT m_tabBarRect{};
    std::vector<RECT> m_tabRects;
    std::vector<RECT> m_tabCloseRects;
    RECT m_btnNewTabInBar{};

    RECT m_sidebarRect{};
    RECT m_segBookmarksRect{};
    RECT m_segReadingRect{};
    RECT m_segHistoryRect{};
    RECT m_sidebarSearchRect{};
    RECT m_btnClearHistoryRect{};
    RECT m_btnSettings{};
    std::vector<RECT> m_sidebarItemRects;

    RECT m_downloadsPopoverRect{};
    RECT m_btnClearDownloadsRect{};
    RECT m_btnOpenFolderRect{};
    std::vector<RECT> m_downloadItemRects;
    std::vector<RECT> m_downloadRevealRects;

    // Hover states
    int m_hoverTraffic = 0; // 1=close, 2=min, 3=max
    bool m_hoverSidebar = false;
    bool m_hoverSettings = false;
    bool m_hoverBack = false;
    bool m_hoverForward = false;
    bool m_hoverAddress = false;
    bool m_hoverReload = false;
    bool m_hoverDownloads = false;
    bool m_hoverShare = false;
    bool m_hoverNewTab = false;
    bool m_hoverOverview = false;
    int m_hoverTab = -1;
    int m_hoverTabClose = -1;
    bool m_hoverNewTabInBar = false;

    // Sidebar state
    int m_sidebarWidth = 320;
    bool m_isDraggingSidebar = false;
    POINT m_lastMousePos{};
    RECT m_sidebarSplitterRect{};

    void updateLayout();
    void initData();
    void recordHistory(const std::wstring& title, const std::wstring& url);
    void handleInternalUrl(size_t tabIdx, const std::wstring& target);
    int findTabIndex(const WebView* view) const;

    void paintTopChrome(Gdiplus::Graphics& g, int width);
    void paintTabBar(Gdiplus::Graphics& g);
    void paintSidebar(Gdiplus::Graphics& g, int height);

    HWND m_hwndDownloadsPopup = nullptr;
    void paintDownloadsPopup(HDC hdc);
    void onDownloadsPopupClick(POINT pt);

    static LRESULT CALLBACK editSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK searchSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK downloadsPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace safari
