#pragma once

#include <windows.h>
#include <WebKit/WKBase.h>
#include <WebKit/WKBaseWin.h>
#include <string>
#include <functional>
#include <memory>

namespace safari {

class WebKitHost {
public:
    static WebKitHost& instance();

    bool initialize();
    void shutdown();

    WKContextRef context() const { return m_context; }
    WKPageGroupRef pageGroup() const { return m_pageGroup; }

private:
    WebKitHost() = default;
    ~WebKitHost() = default;

    WKContextRef m_context = nullptr;
    WKPageGroupRef m_pageGroup = nullptr;
    WKPreferencesRef m_preferences = nullptr;
    bool m_initialized = false;
};

class WebView {
public:
    WebView();
    ~WebView();

    bool create(HWND parent, const RECT& bounds);
    void destroy();

    HWND window() const;
    void setBounds(const RECT& bounds);
    void setVisible(bool visible);
    void setFocus();

    void loadUrl(const std::wstring& url);
    void loadHtml(const std::string& html, const std::string& baseUrl = "safari:start");

    void goBack();
    void goForward();
    void reload();
    void stop();

    bool canGoBack() const;
    bool canGoForward() const;
    double estimatedProgress() const;

    std::wstring title() const;
    std::wstring url() const;

    // Callbacks
    std::function<void(const std::wstring&)> onTitleChanged;
    std::function<void(const std::wstring&)> onUrlChanged;
    std::function<void(double)> onProgressChanged;
    std::function<void(bool loading)> onLoadingChanged;
    std::function<void()> onNavStateChanged;

private:
    WKViewRef m_view = nullptr;
    WKPageRef m_page = nullptr;
    HWND m_parent = nullptr;
    bool m_isLoading = false;

    void setupNavigationClient();

    static void didStartProvisionalNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef userData, const void* clientInfo);
    static void didCommitNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef userData, const void* clientInfo);
    static void didFinishNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef userData, const void* clientInfo);
    static void didFailNavigation(WKPageRef page, WKNavigationRef navigation, WKErrorRef error, WKTypeRef userData, const void* clientInfo);
};

} // namespace safari
