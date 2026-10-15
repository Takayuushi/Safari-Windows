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
    case WM_NCCALCSIZE: {
        if (wParam == TRUE) {
            if (IsZoomed(hwnd)) {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = { sizeof(mi) };
                if (GetMonitorInfoW(hMonitor, &mi)) {
                    params->rgrc[0] = mi.rcWork;
                }
            }
            return 0;
        }
        return 0;
    }

    case WM_NCHITTEST: {
        POINT screenPt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        POINT clientPt = screenPt;
        ScreenToClient(hwnd, &clientPt);

        if (!IsZoomed(hwnd)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int border = 6;
            if (clientPt.y < border && clientPt.x < border) return HTTOPLEFT;
            if (clientPt.y < border && clientPt.x >= rc.right - border) return HTTOPRIGHT;
            if (clientPt.y >= rc.bottom - border && clientPt.x < border) return HTBOTTOMLEFT;
            if (clientPt.y >= rc.bottom - border && clientPt.x >= rc.right - border) return HTBOTTOMRIGHT;
            if (clientPt.y < border) return HTTOP;
            if (clientPt.y >= rc.bottom - border) return HTBOTTOM;
            if (clientPt.x < border) return HTLEFT;
            if (clientPt.x >= rc.right - border) return HTRIGHT;
        }

        if (g_browser) {
            if (g_browser->isHitInteractive(clientPt)) {
                return HTCLIENT;
            }
            if (clientPt.y < 50) {
                return HTCAPTION;
            }
        }
        return HTCLIENT;
    }

    case WM_CREATE: {
        // Enable Immersive Dark Mode on Windows 10/11
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));

        // Extend frame into client area to remove Windows titlebar
        MARGINS margins = { 0, 0, 1, 0 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        g_browser = new safari::Browser();
        if (!g_browser->initialize(hwnd)) {
            MessageBoxW(hwnd, L"Failed to initialize Chronos WebKit engine.", L"Chronos Error", MB_ICONERROR);
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
    wc.lpszClassName = L"ChronosCompassWindowClass";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"Compass",
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
