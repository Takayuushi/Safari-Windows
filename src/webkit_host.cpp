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
    WKPreferencesSetMediaSourceEnabled(prefs, true);
    WKPreferencesSetSourceBufferChangeTypeEnabled(prefs, true);
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

    // 3. HTML5 Media compatibility shim
    if (window.HTMLMediaElement && window.HTMLMediaElement.prototype.canPlayType) {
      var origCanPlay = window.HTMLMediaElement.prototype.canPlayType;
      window.HTMLMediaElement.prototype.canPlayType = function(type) {
        if (!type) return '';
        var l = type.toLowerCase();
        if (l.indexOf('avc1') !== -1 || l.indexOf('mp4a') !== -1 ||
            l.indexOf('video/mp4') !== -1 || l.indexOf('audio/mp4') !== -1 ||
            l.indexOf('mpegurl') !== -1 || l.indexOf('m3u8') !== -1) {
          return 'probably';
        }
        return origCanPlay.call(this, type);
      };
    }

    // 4. YouTube Watch Player Native Direct Stream Connector
    if (window.location && window.location.hostname && (window.location.hostname.indexOf('youtube.com') !== -1 || window.location.hostname.indexOf('youtube-nocookie.com') !== -1 || window.location.hostname.indexOf('googleapis.com') !== -1)) {
      var currentConnectedVideoId = null;
      var isConnecting = false;

      // Clean up YouTube skeleton icons and ensure full scroll expands
      try {
        var styleEl = document.createElement('style');
        styleEl.id = 'chronos-yt-fixes';
        styleEl.textContent = 'html, body { height: auto !important; min-height: 100vh !important; overflow-y: auto !important; overflow-x: hidden !important; } ' +
          'ytd-app, #content, #page-manager, #primary, ytd-rich-grid-renderer, #contents { height: auto !important; overflow: visible !important; } ' +
          '#masthead-skeleton-icons, #masthead-skeleton, #home-page-skeleton, #guide-skeleton, [id*="-skeleton"] { display: none !important; } ' +
          'yt-icon { background: transparent !important; }';
        (document.head || document.documentElement).appendChild(styleEl);
      } catch(se) {}

      function purgeSkeletons() {
        try {
          var skels = document.querySelectorAll('#home-page-skeleton, #masthead-skeleton, #masthead-skeleton-icons, #guide-skeleton');
          for (var i = 0; i < skels.length; i++) {
            skels[i].remove();
          }
        } catch(e) {}
      }
      setInterval(purgeSkeletons, 300);
      document.addEventListener('DOMContentLoaded', purgeSkeletons);

      // Unpause video on user gesture if WebKit autoplay policy requires consent
      document.addEventListener('click', function() {
        try {
          var v = document.querySelector('video.video-stream.html5-main-video') || document.querySelector('video');
          if (v && v.paused && v.src) {
            v.play().catch(function() {});
          }
        } catch(e) {}
      }, true);

      function getVideoId() {
        try {
          var u = new URL(window.location.href);
          if (u.searchParams && u.searchParams.has('v')) return u.searchParams.get('v');
          if (u.pathname.indexOf('/shorts/') === 0) return u.pathname.substring(8).split('?')[0];
          if (u.pathname.indexOf('/embed/') === 0) return u.pathname.substring(7).split('?')[0];
        } catch(e) {}
        var match = window.location.search.match(/[?&]v=([^&]+)/);
        if (match) return match[1];
        var path = window.location.pathname;
        if (path.indexOf('/embed/') === 0) return path.split('/')[2];
        if (path.indexOf('/shorts/') === 0) return path.split('/')[2];
        return null;
      }

      async function connectStream() {
        var vid = getVideoId();
        if (!vid) {
          currentConnectedVideoId = null;
          return;
        }
        if (vid === currentConnectedVideoId || isConnecting) return;
        isConnecting = true;

        try {
          var payload = {
            context: {
              client: {
                clientName: 'ANDROID',
                clientVersion: '21.26.364',
                androidSdkVersion: 30,
                osName: 'Android',
                osVersion: '11',
                hl: 'en',
                timeZone: 'UTC',
                utcOffsetMinutes: 0
              }
            },
            videoId: vid,
            playbackContext: {
              contentPlaybackContext: {
                html5Preference: 'HTML5_PREF_WANTS'
              }
            },
            contentCheckOk: true,
            racyCheckOk: true
          };

          var res = await fetch('https://www.youtube.com/youtubei/v1/player', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
              'X-YouTube-Client-Name': '3',
              'X-YouTube-Client-Version': '21.26.364'
            },
            body: JSON.stringify(payload)
          });

          var data = await res.json();
          var formats = data && data.streamingData && data.streamingData.formats;
          if (formats && formats.length > 0) {
            var best = formats.find(function(f) { return f.itag === 22 && f.url; }) ||
                       formats.find(function(f) { return f.itag === 18 && f.url; }) ||
                       formats.find(function(f) { return f.url; }) ||
                       formats[0];
            if (best && best.url) {
              var streamUrl = best.url;
              currentConnectedVideoId = vid;

              var attachAttempts = 0;
              var attachInterval = setInterval(function() {
                attachAttempts++;
                var video = document.querySelector('video.video-stream.html5-main-video') || document.querySelector('video');
                if (video) {
                  if (video.src !== streamUrl) {
                    video.src = streamUrl;
                    video.load();
                    video.play().catch(function() {});
                  }
                  var spinner = document.querySelector('.ytp-spinner');
                  if (spinner) spinner.style.setProperty('display', 'none', 'important');
                  var errorScreen = document.querySelector('.ytp-error');
                  if (errorScreen) errorScreen.style.setProperty('display', 'none', 'important');
                  clearInterval(attachInterval);
                } else if (attachAttempts > 50) {
                  clearInterval(attachInterval);
                }
              }, 100);
            }
          }
        } catch (e) {
        } finally {
          isConnecting = false;
        }
      }

      window.addEventListener('yt-navigate-finish', function() {
        currentConnectedVideoId = null;
        connectStream();
      });
      window.addEventListener('yt-page-data-updated', connectStream);
      window.addEventListener('popstate', function() {
        connectStream();
      });
      document.addEventListener('DOMContentLoaded', connectStream);
      setInterval(function() {
        var vid = getVideoId();
        if (vid && vid !== currentConnectedVideoId) {
          connectStream();
        }
      }, 500);
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

void WebView::updateCustomUserAgentForUrl(const std::wstring&) {
    if (!m_page) return;
    // Standard macOS Safari User-Agent matching the working ChatGPT WebKit build
    WKStringRef ua = WKStringCreateWithUTF8CString(
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_5) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.5 Safari/605.1.15"
    );
    WKPageSetCustomUserAgent(m_page, ua);
    WKRelease(ua);
}

void WebView::loadUrl(const std::wstring& targetUrl) {
    if (!m_page) return;

    updateCustomUserAgentForUrl(targetUrl);

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
