#include "webkit_host.h"
#include "util.h"
#include "downloads.h"
#include <WebKit/WKView.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKUserScriptRef.h>
#include <WebKit/WKUserContentControllerRef.h>
#include <WebKit/WKUserScriptInjectionTime.h>
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

static void configurePreferences(WKPreferencesRef prefs) {
    if (!prefs) return;
    WKPreferencesSetDeveloperExtrasEnabled(prefs, true);
    WKPreferencesSetJavaScriptEnabled(prefs, true);
    WKPreferencesSetWebAudioEnabled(prefs, true);
    WKPreferencesSetMediaCapabilitiesEnabled(prefs, true);
    WKPreferencesSetAllowMediaContentTypesRequiringHardwareSupportAsFallback(prefs, true);

    // Media & video playback permissions for YouTube / HTML5 streaming
    WKPreferencesSetMediaPlaybackAllowsInline(prefs, true);
    WKPreferencesSetInlineMediaPlaybackRequiresPlaysInlineAttribute(prefs, false);
    WKPreferencesSetMediaPlaybackRequiresUserGesture(prefs, false);
    WKPreferencesSetVideoPlaybackRequiresUserGesture(prefs, false);
    WKPreferencesSetAudioPlaybackRequiresUserGesture(prefs, false);
    WKPreferencesSetMainContentUserGestureOverrideEnabled(prefs, true);
    WKPreferencesSetMediaUserGestureInheritsFromDocument(prefs, true);
    WKPreferencesSetAllowsPictureInPictureMediaPlayback(prefs, true);
    WKPreferencesSetMediaPreloadingEnabled(prefs, true);
    WKPreferencesSetInvisibleMediaAutoplayPermitted(prefs, true);
}

bool WebKitHost::initialize() {
    if (m_initialized) return true;

    m_context = WKContextCreateWithConfiguration(nullptr);
    if (!m_context) return false;

    WKStringRef groupName = WKStringCreateWithUTF8CString("ChronosCompassPageGroup");
    m_pageGroup = WKPageGroupCreateWithIdentifier(groupName);
    WKRelease(groupName);

    m_preferences = WKPageGroupGetPreferences(m_pageGroup);
    configurePreferences(m_preferences);

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
        uintptr_t id = reinterpret_cast<uintptr_t>(download);
        DownloadsManager::instance().addDownload(id, name, destPath, url, total);
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

    // Apply media & autoplay preferences directly to pageConfig
    configurePreferences(WKPageConfigurationGetPreferences(pageConfig));

    // Inject h264ify & media compatibility shim so YouTube selects hardware-accelerated H.264 (avc1) / AAC (mp4a) Media Foundation streams
    WKUserContentControllerRef userContent = WKPageConfigurationGetUserContentController(pageConfig);
    bool createdUserContent = false;
    if (!userContent) {
        userContent = WKUserContentControllerCreate();
        WKPageConfigurationSetUserContentController(pageConfig, userContent);
        createdUserContent = true;
    }

    const char* shimJs = R"js(
(function() {
  try {
    // 1. Fullscreen API polyfill
    try {
      Object.defineProperty(document, 'fullscreenEnabled', { get: function() { return true; }, configurable: true });
      Object.defineProperty(document, 'webkitFullscreenEnabled', { get: function() { return true; }, configurable: true });
      if (!Element.prototype.requestFullscreen && Element.prototype.webkitRequestFullscreen) {
        Element.prototype.requestFullscreen = Element.prototype.webkitRequestFullscreen;
      }
      if (!document.exitFullscreen && document.webkitExitFullscreen) {
        document.exitFullscreen = document.webkitExitFullscreen;
      }
    } catch(fe) {}

    // 2. h264ify: Force YouTube & HTML5 players to select hardware-accelerated H.264 (avc1) / AAC (mp4a)
    // Block VP8, VP9, WebM, and AV1 so YouTube never requests non-H.264 streams
    if (window.MediaSource && typeof window.MediaSource.isTypeSupported === 'function') {
      var origIsTypeSupported = window.MediaSource.isTypeSupported.bind(window.MediaSource);
      window.MediaSource.isTypeSupported = function(type) {
        if (!type) return false;
        var l = type.toLowerCase();
        if (l.indexOf('webm') !== -1 || l.indexOf('vp8') !== -1 || l.indexOf('vp9') !== -1 || l.indexOf('av01') !== -1 || l.indexOf('av1') !== -1) {
          return false;
        }
        if (l.indexOf('mp4') !== -1 || l.indexOf('avc1') !== -1 || l.indexOf('mp4a') !== -1) {
          return true;
        }
        return origIsTypeSupported(type);
      };
    }

    if (window.HTMLMediaElement && window.HTMLMediaElement.prototype.canPlayType) {
      var origCanPlay = window.HTMLMediaElement.prototype.canPlayType;
      window.HTMLMediaElement.prototype.canPlayType = function(type) {
        if (!type) return '';
        var l = type.toLowerCase();
        if (l.indexOf('webm') !== -1 || l.indexOf('vp8') !== -1 || l.indexOf('vp9') !== -1 || l.indexOf('av01') !== -1 || l.indexOf('av1') !== -1) {
          return '';
        }
        if (l.indexOf('avc1') !== -1 || l.indexOf('mp4a') !== -1 || l.indexOf('video/mp4') !== -1 || l.indexOf('audio/mp4') !== -1) {
          return 'probably';
        }
        return origCanPlay.call(this, type);
      };
    }

    if (navigator.mediaCapabilities && typeof navigator.mediaCapabilities.decodingInfo === 'function') {
      var origDecodingInfo = navigator.mediaCapabilities.decodingInfo.bind(navigator.mediaCapabilities);
      navigator.mediaCapabilities.decodingInfo = function(config) {
        if (config && config.video && config.video.contentType) {
          var ct = config.video.contentType.toLowerCase();
          if (ct.indexOf('webm') !== -1 || ct.indexOf('vp8') !== -1 || ct.indexOf('vp9') !== -1 || ct.indexOf('av01') !== -1 || ct.indexOf('av1') !== -1) {
            return Promise.resolve({ supported: false, smooth: false, powerEfficient: false });
          }
          if (ct.indexOf('avc1') !== -1 || ct.indexOf('mp4') !== -1) {
            return Promise.resolve({ supported: true, smooth: true, powerEfficient: true });
          }
        }
        return origDecodingInfo(config);
      };
    }

    // 3. YouTube native player watchdog & unblocker
    if (window.location && window.location.hostname && window.location.hostname.indexOf('youtube.com') !== -1) {
      function cleanLegacy() {
        var oldP = document.getElementById('chronos-yt-embed');
        if (oldP) oldP.remove();
        var oldNative = document.getElementById('chronos-native-video');
        if (oldNative) oldNative.remove();
        var mp = document.getElementById('movie_player');
        if (mp && mp.style.display === 'none') {
          mp.style.display = '';
        }
      }

      function autoSkipAds() {
        cleanLegacy();
        var skipBtn = document.querySelector('.ytp-ad-skip-button, .ytp-ad-skip-button-modern, .ytp-skip-ad-button');
        if (skipBtn) {
          skipBtn.click();
          return;
        }
        var adShowing = document.querySelector('.ad-showing, .ad-interrupting');
        if (adShowing) {
          var v = document.querySelector('#movie_player video, video.html5-main-video');
          if (v && isFinite(v.duration) && v.duration > 0 && v.currentTime < v.duration) {
            v.currentTime = v.duration;
          }
          var closeBtn = document.querySelector('.ytp-ad-overlay-close-button');
          if (closeBtn) closeBtn.click();
        }
      }

      document.addEventListener('click', function(e) {
        var t = e.target;
        if (t && (t.closest('#movie_player') || t.closest('.ytp-large-play-button') || t.closest('.ytp-cued-thumbnail-overlay') || t.closest('.html5-video-player'))) {
          var mp = document.getElementById('movie_player');
          if (mp && typeof mp.playVideo === 'function') {
            try { mp.playVideo(); } catch(err) {}
          }
          var v = document.querySelector('#movie_player video, video.html5-main-video');
          if (v && v.paused) {
            try { v.play(); } catch(err) {}
          }
        }
      }, true);

      setInterval(autoSkipAds, 350);
      window.addEventListener('DOMContentLoaded', cleanLegacy);
      window.addEventListener('yt-navigate-finish', cleanLegacy);
    }
  } catch(e) {}
})();
)js";

    WKStringRef scriptStr = WKStringCreateWithUTF8CString(shimJs);
    WKUserScriptRef userScript = WKUserScriptCreateWithSource(scriptStr, kWKInjectAtDocumentStart, false);
    WKUserContentControllerAddUserScript(userContent, userScript);
    WKRelease(userScript);
    WKRelease(scriptStr);

    if (createdUserContent) {
        WKRelease(userContent);
    }

    m_view = WKViewCreate(bounds, pageConfig, parent);
    WKRelease(pageConfig);

    if (!m_view) return false;

    // Notify WebKit that the view is hosted in an active native window so WebCore initializes media data loading
    WKViewSetIsInWindow(m_view, true);
    WKViewWindowAncestryDidChange(m_view);

    m_page = WKViewGetPage(m_view);

    // Modern Windows Desktop User-Agent with Safari/WebKit signature
    // Instructs YouTube to deliver its primary Windows HTML5 player instead of macOS-exclusive FairPlay/AVFoundation code paths
    WKStringRef ua = WKStringCreateWithUTF8CString(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
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
        WKViewSetIsInWindow(m_view, false);
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
        if (m_view) {
            WKViewWindowAncestryDidChange(m_view);
        }
    }
}

void WebView::setVisible(bool visible) {
    if (m_view) {
        WKViewSetIsInWindow(m_view, visible);
        if (visible) {
            WKViewWindowAncestryDidChange(m_view);
        }
    }
    HWND w = window();
    if (w) {
        if (visible) {
            ShowWindow(w, SW_SHOW);
            SetWindowPos(w, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            InvalidateRect(w, nullptr, TRUE);
            UpdateWindow(w);
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
        if (m_view) {
            WKViewWindowAncestryDidChange(m_view);
        }
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

void WebView::setPageZoom(double factor) {
    if (m_page) {
        WKPageSetPageZoomFactor(m_page, factor);
    }
}

double WebView::pageZoom() const {
    return m_page ? WKPageGetPageZoomFactor(m_page) : 1.0;
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
    if (!urlRef) {
        urlRef = WKPageCopyProvisionalURL(m_page);
    }
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

    // Internal pages must never be treated as downloads
    if (url.starts_with(L"chronos:") || url.starts_with(L"safari:") || url.starts_with(L"about:")) {
        WKFramePolicyListenerUse(listener);
        return;
    }

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

void WebView::didStartProvisionalNavigation(WKPageRef page, WKNavigationRef, WKTypeRef, const void* clientInfo) {
    auto* self = static_cast<WebView*>(const_cast<void*>(clientInfo));
    self->m_isLoading = true;
    if (self->onLoadingChanged) self->onLoadingChanged(true);

    // Only dispatch provisional navigation URL updates for internal action commands (e.g. chronos:pause?id=...)
    // so they can be intercepted immediately. Regular URLs update on commit/finish, eliminating infinite reloads.
    WKURLRef provRef = WKPageCopyProvisionalURL(page);
    if (provRef) {
        WKStringRef strRef = WKURLCopyString(provRef);
        std::wstring provUrl = wkStringToWString(strRef);
        if (strRef) WKRelease(strRef);
        WKRelease(provRef);
        if (!provUrl.empty() && provUrl.find(L"?") != std::wstring::npos && self->onUrlChanged) {
            self->onUrlChanged(provUrl);
        }
    }

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

    // Render a polished Safari-styled error page for failed web navigations
    std::wstring curUrl = self->url();
    if (!curUrl.empty() && (curUrl.starts_with(L"http://") || curUrl.starts_with(L"https://"))) {
        std::string errHtml =
            "<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<style>"
            "body{background:#1c1c1e;color:#f5f5f7;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI Variable Text',sans-serif;"
            "display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:85vh;margin:0;user-select:none;}"
            ".icon{font-size:44px;margin-bottom:18px;opacity:0.7;}"
            "h1{font-size:22px;font-weight:600;margin:0 0 10px;}"
            "p{font-size:14px;color:#8e8e93;max-width:440px;text-align:center;line-height:1.5;margin:0 0 24px;}"
            ".btn{background:#007aff;color:#fff;padding:8px 22px;border-radius:8px;font-size:14px;text-decoration:none;font-weight:500;}"
            "</style></head><body>"
            "<div class='icon'>⚠️</div>"
            "<h1>Safari Can't Open the Page</h1>"
            "<p>The page could not be opened because the server stopped responding or the network connection was interrupted.</p>"
            "<a class='btn' href='javascript:location.reload()'>Try Again</a>"
            "</body></html>";
        self->loadHtml(errHtml, toUtf8(curUrl));
    }
}

} // namespace safari
