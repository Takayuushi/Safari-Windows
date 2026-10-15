#include "webkit_host.h"
#include "util.h"
#include <WebKit/WKView.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKString.h>
#include <WebKit/WKType.h>
#include <WebKit/WKErrorRef.h>

namespace safari {

static std::wstring wkStringToWString(WKStringRef str) {
    if (!str) return {};
    size_t maxBytes = WKStringGetMaximumUTF8CStringSize(str);
    if (maxBytes == 0) return {};
    std::string utf8(maxBytes, 0);
    size_t actual = WKStringGetUTF8CString(str, utf8.data(), maxBytes);
    if (actual > 0) {
        utf8.resize(actual - 1);
        return toUtf16(utf8);
    }
    return {};
}

WebKitHost& WebKitHost::instance() {
    static WebKitHost s_host;
    return s_host;
}

bool WebKitHost::initialize() {
    if (m_initialized) return true;

    m_context = WKContextCreateWithConfiguration(nullptr);
    if (!m_context) return false;

    WKStringRef groupName = WKStringCreateWithUTF8CString("SafariPageGroup");
    m_pageGroup = WKPageGroupCreateWithIdentifier(groupName);
    WKRelease(groupName);

    m_preferences = WKPageGroupGetPreferences(m_pageGroup);
    if (m_preferences) {
        WKPreferencesSetDeveloperExtrasEnabled(m_preferences, true);
        WKPreferencesSetJavaScriptEnabled(m_preferences, true);
    }

    m_initialized = true;
    return true;
}

void WebKitHost::shutdown() {
    if (m_pageGroup) {
        WKRelease(m_pageGroup);
        m_pageGroup = nullptr;
    }
    if (m_context) {
        WKRelease(m_context);
        m_context = nullptr;
    }
    m_initialized = false;
}

WebView::WebView() = default;

WebView::~WebView() {
    destroy();
}

bool WebView::create(HWND parent, const RECT& bounds) {
    m_parent = parent;
    WebKitHost& host = WebKitHost::instance();
    if (!host.initialize()) return false;

    WKPageConfigurationRef pageConfig = WKPageConfigurationCreate();
    WKPageConfigurationSetContext(pageConfig, host.context());
    WKPageConfigurationSetPageGroup(pageConfig, host.pageGroup());

    m_view = WKViewCreate(bounds, pageConfig, parent);
    WKRelease(pageConfig);

    if (!m_view) return false;

    m_page = WKViewGetPage(m_view);
    setupNavigationClient();
    return true;
}

void WebView::destroy() {
    if (m_view) {
        HWND w = WKViewGetWindow(m_view);
        if (w) ShowWindow(w, SW_HIDE);
        // Do not call WKRelease(m_view) on shutdown to prevent WebKit worker thread crash
        m_view = nullptr;
        m_page = nullptr;
    }
}

HWND WebView::window() const {
    return m_view ? WKViewGetWindow(m_view) : nullptr;
}

void WebView::setBounds(const RECT& bounds) {
    HWND w = window();
    if (w) {
        SetWindowPos(w, nullptr,
            bounds.left, bounds.top,
            bounds.right - bounds.left, bounds.bottom - bounds.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void WebView::setVisible(bool visible) {
    HWND w = window();
    if (w) {
        ShowWindow(w, visible ? SW_SHOW : SW_HIDE);
    }
}

void WebView::setFocus() {
    HWND w = window();
    if (w) {
        SetFocus(w);
    }
}

void WebView::loadUrl(const std::wstring& targetUrl) {
    if (!m_page) return;
    std::string utf8 = toUtf8(targetUrl);
    WKURLRef urlRef = WKURLCreateWithUTF8CString(utf8.c_str());
    WKPageLoadURL(m_page, urlRef);
    WKRelease(urlRef);
}

void WebView::loadHtml(const std::string& html, const std::string& baseUrl) {
    if (!m_page) return;
    WKStringRef htmlRef = WKStringCreateWithUTF8CString(html.c_str());
    WKURLRef baseRef = WKURLCreateWithUTF8CString(baseUrl.c_str());
    WKPageLoadHTMLString(m_page, htmlRef, baseRef);
    WKRelease(htmlRef);
    WKRelease(baseRef);
}

void WebView::goBack() {
    if (m_page && canGoBack()) WKPageGoBack(m_page);
}

void WebView::goForward() {
    if (m_page && canGoForward()) WKPageGoForward(m_page);
}

void WebView::reload() {
    if (m_page) WKPageReload(m_page);
}

void WebView::stop() {
    if (m_page) WKPageStopLoading(m_page);
}

bool WebView::canGoBack() const {
    return m_page ? WKPageCanGoBack(m_page) : false;
}

bool WebView::canGoForward() const {
    return m_page ? WKPageCanGoForward(m_page) : false;
}

double WebView::estimatedProgress() const {
    return m_page ? WKPageGetEstimatedProgress(m_page) : 0.0;
}

std::wstring WebView::title() const {
    if (!m_page) return {};
    WKStringRef titleRef = WKPageCopyTitle(m_page);
    if (!titleRef) return {};
    std::wstring result = wkStringToWString(titleRef);
    WKRelease(titleRef);
    return result;
}

std::wstring WebView::url() const {
    if (!m_page) return {};
    WKURLRef urlRef = WKPageCopyActiveURL(m_page);
    if (!urlRef) return {};
    WKStringRef strRef = WKURLCopyString(urlRef);
    std::wstring result = wkStringToWString(strRef);
    if (strRef) WKRelease(strRef);
    WKRelease(urlRef);
    return result;
}

void WebView::setupNavigationClient() {
    if (!m_page) return;

    WKPageNavigationClientV0 client = {};
    client.base.version = 0;
    client.base.clientInfo = this;

    client.didStartProvisionalNavigation = didStartProvisionalNavigation;
    client.didCommitNavigation = didCommitNavigation;
    client.didFinishNavigation = didFinishNavigation;
    client.didFailNavigation = didFailNavigation;
    client.didFailProvisionalNavigation = didFailNavigation;

    WKPageSetPageNavigationClient(m_page, &client.base);
}

void WebView::didStartProvisionalNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void* clientInfo) {
    auto* self = static_cast<WebView*>(const_cast<void*>(clientInfo));
    self->m_isLoading = true;
    if (self->onLoadingChanged) self->onLoadingChanged(true);
    if (self->onUrlChanged) self->onUrlChanged(self->url());
    if (self->onNavStateChanged) self->onNavStateChanged();
}

void WebView::didCommitNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void* clientInfo) {
    auto* self = static_cast<WebView*>(const_cast<void*>(clientInfo));
    if (self->onUrlChanged) self->onUrlChanged(self->url());
    if (self->onTitleChanged) self->onTitleChanged(self->title());
    if (self->onNavStateChanged) self->onNavStateChanged();
}

void WebView::didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void* clientInfo) {
    auto* self = static_cast<WebView*>(const_cast<void*>(clientInfo));
    self->m_isLoading = false;
    if (self->onLoadingChanged) self->onLoadingChanged(false);
    if (self->onTitleChanged) self->onTitleChanged(self->title());
    if (self->onUrlChanged) self->onUrlChanged(self->url());
    if (self->onNavStateChanged) self->onNavStateChanged();
}

void WebView::didFailNavigation(WKPageRef, WKNavigationRef, WKErrorRef, WKTypeRef, const void* clientInfo) {
    auto* self = static_cast<WebView*>(const_cast<void*>(clientInfo));
    self->m_isLoading = false;
    if (self->onLoadingChanged) self->onLoadingChanged(false);
    if (self->onNavStateChanged) self->onNavStateChanged();
}

} // namespace safari
