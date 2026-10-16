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
    // 1. Safari identity polyfill
    try {
      Object.defineProperty(navigator, 'vendor', { get: function() { return 'Apple Computer, Inc.'; }, configurable: true });
      Object.defineProperty(navigator, 'platform', { get: function() { return 'MacIntel'; }, configurable: true });
    } catch(e) {}

    // 2. Fullscreen API polyfill
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

    // 3. Hardware-Accelerated H.264 & AAC Media Capabilities routing for Windows Media Foundation
    // Tell media players to select standard H.264 MP4 streams.
    // Block WebM/VP8/VP9/AV1 which Media Foundation cannot decode without specialized codecs.
    // Do NOT return 'probably' for HLS/m3u8, avoiding unsupported dynamic byte stream errors on Windows.
    if (window.HTMLMediaElement && window.HTMLMediaElement.prototype.canPlayType) {
      var origCanPlay = window.HTMLMediaElement.prototype.canPlayType;
      window.HTMLMediaElement.prototype.canPlayType = function(type) {
        if (!type) return '';
        var l = type.toLowerCase();
        if (l.indexOf('webm') !== -1 || l.indexOf('vp8') !== -1 || l.indexOf('vp9') !== -1 || l.indexOf('av01') !== -1 || l.indexOf('av1') !== -1) {
          return '';
        }
        if (l.indexOf('avc1') !== -1 || l.indexOf('mp4a') !== -1 || l.indexOf('video/mp4') !== -1 || l.indexOf('audio/mp4') !== -1 || l.indexOf('audio/x-m4a') !== -1 || l.indexOf('audio/aac') !== -1) {
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

    // 4. Option B: Clean YouTube Mobile & Embedded Player Routing
    if (window.location && window.location.hostname && window.location.hostname.indexOf('youtube.com') !== -1) {
      // If already on a dedicated embed page, let it play naturally
      if (window.location.pathname.indexOf('/embed/') === 0) return;

      function extractVideoId() {
        try {
          var u = new URL(window.location.href);
          if (u.searchParams && u.searchParams.has('v')) {
            return u.searchParams.get('v');
          }
          if (u.pathname.indexOf('/shorts/') === 0) {
            return u.pathname.substring(8).split('?')[0];
          }
        } catch(e) {}
        var m = window.location.href.match(/[?&]v=([a-zA-Z0-9_-]{11})/);
        return m ? m[1] : null;
      }

      function injectCleanPlayer() {
        var vid = extractVideoId();
        if (!vid) return;

        // Locate video player container on mobile (m.youtube.com) or desktop
        var container = document.querySelector(
          '.player-container, #player, ytm-player, #player-container-id, #movie_player, .html5-video-player'
        );
        if (!container) return;

        var existing = document.getElementById('chronos-clean-embed');
        if (existing) {
          if (existing.getAttribute('data-vid') === vid) {
            return;
          }
          existing.remove();
        }

        if (window.getComputedStyle(container).position === 'static') {
          container.style.position = 'relative';
        }

        // Mute / pause any underlying native video element that failed
        var videos = container.querySelectorAll('video');
        videos.forEach(function(v) {
          try {
            v.pause();
            v.style.display = 'none';
          } catch(e) {}
        });

        // Hide broken error screens
        var errOverlays = container.querySelectorAll('.ytp-error, .ytp-error-screen, .ytp-cued-thumbnail-overlay');
        errOverlays.forEach(function(el) {
          try { el.style.display = 'none'; } catch(e) {}
        });

        var wrap = document.createElement('div');
        wrap.id = 'chronos-clean-embed';
        wrap.setAttribute('data-vid', vid);
        wrap.style.cssText = 'position:absolute;top:0;left:0;width:100%;height:100%;z-index:9999;background:#000;display:flex;flex-direction:column;';

        var topControls = document.createElement('div');
        topControls.style.cssText = 'position:absolute;top:8px;right:12px;z-index:10001;display:flex;gap:8px;';

        var theaterBtn = document.createElement('a');
        theaterBtn.href = 'https://www.youtube.com/embed/' + vid + '?autoplay=1';
        theaterBtn.innerText = '⤢ Full Player';
        theaterBtn.title = 'Switch to full-window clean player';
        theaterBtn.style.cssText = 'background:rgba(20,20,20,0.75);color:#fff;padding:5px 12px;border-radius:14px;font-size:12px;text-decoration:none;font-weight:600;font-family:-apple-system,sans-serif;backdrop-filter:blur(8px);border:1px solid rgba(255,255,255,0.25);box-shadow:0 2px 8px rgba(0,0,0,0.4);';
        topControls.appendChild(theaterBtn);
        wrap.appendChild(topControls);

        var ifr = document.createElement('iframe');
        ifr.id = 'chronos-embed-iframe';
        ifr.src = 'https://www.youtube.com/embed/' + vid + '?autoplay=1&playsinline=1&enablejsapi=1&rel=0';
        ifr.style.cssText = 'width:100%;height:100%;border:none;flex:1;';
        ifr.setAttribute('allow', 'accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share');
        ifr.setAttribute('allowfullscreen', 'true');
        wrap.appendChild(ifr);

        container.appendChild(wrap);
      }

      window.addEventListener('DOMContentLoaded', injectCleanPlayer);
      window.addEventListener('yt-navigate-finish', injectCleanPlayer);
      window.addEventListener('yt-page-data-updated', injectCleanPlayer);
      window.addEventListener('popstate', injectCleanPlayer);
      setInterval(injectCleanPlayer, 600);
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

    // Initialize custom user agent
    updateCustomUserAgentForUrl(L"");

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

void WebView::updateCustomUserAgentForUrl(const std::wstring& url) {
    if (!m_page) return;
    bool isYouTube = (url.find(L"youtube.com") != std::wstring::npos || url.find(L"youtu.be") != std::wstring::npos);
    if (isYouTube) {
        // iPad Safari User-Agent: Delivers modern, lightweight HTML5 mobile/tablet layout without broken desktop Polymer Web Components
        WKStringRef ua = WKStringCreateWithUTF8CString(
            "Mozilla/5.0 (iPad; CPU OS 17_5 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.5 Mobile/15E148 Safari/604.1"
        );
        WKPageSetCustomUserAgent(m_page, ua);
        WKRelease(ua);
    } else {
        // Desktop macOS Safari User-Agent for standard web browsing
        WKStringRef ua = WKStringCreateWithUTF8CString(
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.6 Safari/605.1.15"
        );
        WKPageSetCustomUserAgent(m_page, ua);
        WKRelease(ua);
    }
}

void WebView::loadUrl(const std::wstring& targetUrl) {
    if (!m_page) return;

    std::wstring routedUrl = targetUrl;

    // Option B: Clean YouTube Routing
    // Rewrite desktop YouTube to m.youtube.com for fast, lightweight UI without broken desktop Polymer Web Components
    if (routedUrl.find(L"www.youtube.com") != std::wstring::npos || routedUrl.find(L"://youtube.com") != std::wstring::npos) {
        if (routedUrl.find(L"/embed/") == std::wstring::npos) {
            size_t pos = routedUrl.find(L"www.youtube.com");
            if (pos != std::wstring::npos) {
                routedUrl.replace(pos, 15, L"m.youtube.com");
            } else {
                pos = routedUrl.find(L"://youtube.com");
                if (pos != std::wstring::npos) {
                    routedUrl.replace(pos, 14, L"://m.youtube.com");
                }
            }
        }
    }

    updateCustomUserAgentForUrl(routedUrl);

    std::string utf8 = toUtf8(routedUrl);
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
        self->updateCustomUserAgentForUrl(provUrl);
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
