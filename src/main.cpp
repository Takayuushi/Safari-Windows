#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include "browser.h"
#include "util.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

static safari::Browser* g_browser = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Enable Immersive Dark Mode on Windows 10/11
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));

        g_browser = new safari::Browser();
        if (!g_browser->initialize(hwnd)) {
            MessageBoxW(hwnd, L"Failed to initialize Safari WebKit engine.", L"Safari Error", MB_ICONERROR);
            return -1;
        }
        return 0;
    }

    case WM_SIZE:
        if (g_browser) {
            g_browser->handleResize();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_browser) {
            g_browser->paint(hdc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        if (g_browser) {
            g_browser->onMouseMove(pt);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (g_browser) {
            g_browser->onMouseDown(pt);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (g_browser) {
            g_browser->onMouseUp(pt);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_browser) {
            g_browser->onMouseLeave();
        }
        return 0;

    case WM_KEYDOWN:
        if (g_browser && g_browser->onKeyDown(wParam)) {
            return 0;
        }
        break;

    case WM_DESTROY:
        if (g_browser) {
            delete g_browser;
            g_browser = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    safari::initGdiplus();

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SafariMainWindowClass";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"Safari",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 840,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) {
        safari::shutdownGdiplus();
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    safari::shutdownGdiplus();
    return (int)msg.wParam;
}
