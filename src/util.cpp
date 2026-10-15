#include "util.h"
#include <unknwn.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

namespace safari {

static ULONG_PTR g_gdiplusToken = 0;

void initGdiplus() {
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr);
}

void shutdownGdiplus() {
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

std::string toUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring toUtf16(const std::string& utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), result.data(), size);
    return result;
}

int dipToPx(int dip, UINT dpi) {
    return MulDiv(dip, (int)dpi, 96);
}

int pxToDip(int px, UINT dpi) {
    return MulDiv(px, 96, (int)dpi);
}

RECT scaleRect(const RECT& r, UINT dpi) {
    return RECT{
        dipToPx(r.left, dpi),
        dipToPx(r.top, dpi),
        dipToPx(r.right, dpi),
        dipToPx(r.bottom, dpi)
    };
}

std::wstring normalizeInput(const std::wstring& input) {
    std::wstring trimmed = input;
    trimmed.erase(0, trimmed.find_first_not_of(L" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(L" \t\r\n") + 1);

    if (trimmed.empty()) return L"chronos:start";
    if (trimmed == L"chronos:start" || trimmed == L"safari:start" || trimmed == L"about:blank") return L"chronos:start";
    if (trimmed == L"chronos:settings" || trimmed == L"safari:settings" || trimmed == L"settings") return L"chronos:settings";
    if (trimmed == L"chronos:downloads" || trimmed == L"safari:downloads" || trimmed == L"downloads") return L"chronos:downloads";
    if (trimmed.starts_with(L"chronos:") || trimmed.starts_with(L"safari:")) return trimmed;

    if (trimmed.rfind(L"http://", 0) == 0 || trimmed.rfind(L"https://", 0) == 0) {
        return trimmed;
    }

    if (trimmed.find(L'.') != std::wstring::npos && trimmed.find(L' ') == std::wstring::npos) {
        return L"https://" + trimmed;
    }

    if (trimmed.rfind(L"localhost", 0) == 0) {
        return L"http://" + trimmed;
    }

    // Default search query
    std::string utf8 = toUtf8(trimmed);
    std::string escaped;
    for (unsigned char c : utf8) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped += (char)c;
        } else if (c == ' ') {
            escaped += '+';
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            escaped += buf;
        }
    }
    return L"https://www.google.com/search?q=" + toUtf16(escaped);
}

std::wstring extractDomain(const std::wstring& url) {
    if (url.empty() || url == L"chronos:start" || url == L"safari:start" || url == L"about:blank") {
        return L"Start Page";
    }
    if (url == L"chronos:settings" || url == L"safari:settings") {
        return L"Settings";
    }

    size_t start = 0;
    if (url.rfind(L"https://", 0) == 0) start = 8;
    else if (url.rfind(L"http://", 0) == 0) start = 7;

    size_t end = url.find(L'/', start);
    std::wstring host = (end == std::wstring::npos) ? url.substr(start) : url.substr(start, end - start);

    if (host.rfind(L"www.", 0) == 0) {
        host = host.substr(4);
    }
    return host.empty() ? url : host;
}

bool isWebUrl(const std::wstring& input) {
    return input.rfind(L"http://", 0) == 0 || input.rfind(L"https://", 0) == 0;
}

std::wstring getDownloadsFolder() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path))) {
        std::wstring result(path);
        CoTaskMemFree(path);
        return result;
    }
    return L"C:\\Downloads";
}

void revealFileInExplorer(const std::wstring& filePath) {
    if (filePath.empty()) {
        ShellExecuteW(nullptr, L"open", getDownloadsFolder().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
        return;
    }
    std::wstring params = L"/select,\"" + filePath + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWDEFAULT);
}

} // namespace safari
