#include "webkit_host.h"
#include "util.h"
#include "downloads.h"
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
#include <WebKit/WKFramePolicyListener.h>
#include <WebKit/WKNavigationResponseRef.h>
#include <WebKit/WKURLResponse.h>
#include <WebKit/WKDownloadRef.h>
#include <WebKit/WKContextDownloadClient.h>
#include <algorithm>

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

    WKStringRef groupName = WKStringCreateWithUTF8CString("ChronosCompassPageGroup");
    m_pageGroup = WKPageGroupCreateWithIdentifier(groupName);
    WKRelease(groupName);

    m_preferences = WKPageGroupGetPreferences(m_pageGroup);
    if (m_preferences) {
        WKPreferencesSetDeveloperExtrasEnabled(m_preferences, true);
        WKPreferencesSetJavaScriptEnabled(m_preferences, true);
        WKPreferencesSetWebAudioEnabled(m_preferences, true);
        WKPreferencesSetMediaCapabilitiesEnabled(m_preferences, true);
        WKPreferencesSetAllowMediaContentTypesRequiringHardwareSupportAsFallback(m_preferences, true);
    }

    // Set up Download Client to save files to Downloads folder
    WKContextDownloadClientV0 dlClient = {};
    dlClient.base.version = 0;

    dlClient.decideDestinationWithSuggestedFilename = [](WKContextRef, WKDownloadRef, WKStringRef filename, bool* allowOverwrite, const void*) -> WKStringRef {
        std::wstring name = wkStringToWString(filename);
        if (name.empty()) name = L"download.bin";
        std::wstring destPath = getDownloadsFolder() + L"\\" + name;
        if (allowOverwrite) *allowOverwrite = true;
        return WKStringCreateWithUTF8CString(toUtf8(destPath).c_str());
    };

    dlClient.didReceiveResponse = [](WKContextRef, WKDownloadRef download, WKURLResponseRef response, const void*) {
        WKURLRef urlRef = response ? WKURLResponseCopyURL(response) : nullptr;
        WKStringRef urlStr = urlRef ? WKURLCopyString(urlRef) : nullptr;
        std::wstring url = wkStringToWString(urlStr);
        if (urlStr) WKRelease(urlStr);
        if (urlRef) WKRelease(urlRef);

        int64_t total = response ? WKURLResponseGetExpectedContentLength(response) : 0;
        WKStringRef suggRef = response ? WKURLResponseCopySuggestedFilename(response) : nullptr;
        std::wstring name = wkStringToWString(suggRef);
        if (suggRef) WKRelease(suggRef);
        if (name.empty()) name = L"download.bin";

        std::wstring destPath = getDownloadsFolder() + L"\\" + name;
        DownloadsManager::instance().addDownload(name, destPath, url, total);
    };

    dlClient.didReceiveData = [](WKContextRef, WKDownloadRef download, uint64_t length, const void*) {
        uintptr_t id = reinterpret_cast<uintptr_t>(download);
        DownloadsManager::instance().updateProgress(id, (int64_t)length, 0);
    };

    dlClient.didFinish = [](WKContextRef, WKDownloadRef download, const void*) {
        uintptr_t id = reinterpret_cast<uintptr_t>(download);
        DownloadsManager::instance().markCompleted(id);
    };

    dlClient.didFail = [](WKContextRef, WKDownloadRef download, WKErrorRef, const void*) {
        uintptr_t id = reinterpret_cast<uintptr_t>(download);
        DownloadsManager::instance().markFailed(id);
    };

    WKContextSetDownloadClient(m_context, &dlClient.base);

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

    // Authentic macOS Safari User-Agent for seamless web compatibility and HTML5 media playback
    WKStringRef ua = WKStringCreateWithUTF8CString(
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Safari/605.1.15"
    );
    WKPageSetCustomUserAgent(m_page, ua);
    WKRelease(ua);

    setupNavigationClient();
    setupPolicyClient();
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
        SetWindowPos(w, HWND_TOP,
            bounds.left, bounds.top,
            bounds.right - bounds.left, bounds.bottom - bounds.top,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        RedrawWindow(w, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
}

void WebView::setVisible(bool visible) {
    HWND w = window();
    if (w) {
        if (visible) {
            ShowWindow(w, SW_SHOW);
            SetWindowPos(w, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            RedrawWindow(w, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
        } else {
            ShowWindow(w, SW_HIDE);
        }
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

void WebView::setupPolicyClient() {
    if (!m_page) return;

    WKPagePolicyClientV1 policy = {};
    policy.base.version = 1;
    policy.base.clientInfo = this;
    policy.decidePolicyForResponse = decidePolicyForResponse;

    WKPageSetPagePolicyClient(m_page, &policy.base);
}

void WebView::decidePolicyForResponse(WKPageRef, WKFrameRef, WKURLResponseRef response, WKURLRequestRef, bool canShowMIMEType, WKFramePolicyListenerRef listener, WKTypeRef, const void* clientInfo) {
    auto* self = static_cast<WebView*>(const_cast<void*>(clientInfo));
    WKStringRef mimeRef = response ? WKURLResponseCopyMIMEType(response) : nullptr;
    std::wstring mime = wkStringToWString(mimeRef);
    if (mimeRef) WKRelease(mimeRef);

    WKURLRef urlRef = response ? WKURLResponseCopyURL(response) : nullptr;
    WKStringRef urlStr = urlRef ? WKURLCopyString(urlRef) : nullptr;
    std::wstring url = wkStringToWString(urlStr);
    if (urlStr) WKRelease(urlStr);
    if (urlRef) WKRelease(urlRef);

    std::wstring lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    bool isDownload = false;
    if (lower.ends_with(L".zip") || lower.ends_with(L".msixbundle") || lower.ends_with(L".exe") ||
        lower.ends_with(L".msi") || lower.ends_with(L".7z") || lower.ends_with(L".rar") ||
        lower.ends_with(L".tar") || lower.ends_with(L".gz") || lower.ends_with(L".iso") ||
        lower.ends_with(L".dmg") || lower.ends_with(L".pkg") || lower.ends_with(L".bin") ||
        mime == L"application/octet-stream" || mime == L"application/x-msdownload" ||
        mime == L"application/x-msixbundle" || mime == L"application/zip") {
        isDownload = true;
    }

    if (!canShowMIMEType || isDownload) {
        WKFramePolicyListenerDownload(listener);
        if (self && self->onDownloadStarted) {
            self->onDownloadStarted();
        }
    } else {
        WKFramePolicyListenerUse(listener);
    }
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
