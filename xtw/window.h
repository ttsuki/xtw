/// @file
/// @brief  xtw::window
/// @author (C) 2023 ttsuki

#pragma once

#include <Windows.h>
#include <memory>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <string>
#include <tuple>
#include <atomic>

#include "unique_handle.h"

namespace xtw::window
{
    template <class WNDPROC = ::WNDPROC, std::enable_if_t<std::is_invocable_r_v<LRESULT, WNDPROC, HWND, UINT, WPARAM, LPARAM>>* = nullptr>
    static inline std::shared_ptr<std::remove_pointer_t<HWND>> CreateApplicationWindow(
        LPCWSTR lpClassName,
        LPCWSTR lpWindowName,
        LONG X,
        LONG Y,
        LONG ClientWidth,
        LONG ClientHeight,
        HICON hIcon,
        WNDPROC lpfnWndProc,
        DWORD dwClassStyle = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        DWORD dwWindowStyle = WS_OVERLAPPEDWINDOW,
        DWORD dwWindowExStyle = 0)
    {
        const HMODULE hInstance = GetModuleHandleA(nullptr);
        
        using WndProcContainer = std::tuple<WNDPROC>;
        constexpr auto fnProxyProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)-> LRESULT
        {
            if (message == WM_CREATE)
            {
                // Store WndProc
                auto desc = reinterpret_cast<LPCREATESTRUCTW>(lParam);
                auto target = static_cast<WndProcContainer*>(desc->lpCreateParams);
                SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(target));
            }

            // Load WndProc
            if (auto target = reinterpret_cast<WndProcContainer*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA)))
                return std::get<0>(*target)(hWnd, message, wParam, lParam);

            return DefWindowProcW(hWnd, message, wParam, lParam);
        };

        // Register Window Class
        WNDCLASSEXW wcex{sizeof(WNDCLASSEXW),};
        wcex.style = dwClassStyle;
        wcex.lpfnWndProc = fnProxyProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = hIcon;
        wcex.hCursor = ::LoadCursorA(nullptr, IDC_ARROW);
        wcex.hbrBackground = static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = lpClassName;
        wcex.hIconSm = hIcon;

        ATOM atom = ::RegisterClassExW(&wcex);
        if (!atom)
        {
            // Retry with additional suffix
            static std::atomic_int i{};
            std::wstring buf = lpClassName;
            buf += std::to_wstring(GetTickCount());
            buf += std::to_wstring(i++);
            wcex.lpszClassName = buf.c_str();
            atom = ::RegisterClassExW(&wcex);

            if (!atom)
            {
                throw std::runtime_error("FAILED to register window class");
            }
        }

        auto uhClassName = unique_handle_t{
            reinterpret_cast<LPCWSTR>(atom),
            [hInstance](LPCWSTR atom) { if (hInstance && atom) { ::UnregisterClassW(atom, hInstance); } }
        };

        // Make window procedure permanent
        auto upWindowProc = std::make_unique<WndProcContainer>(std::move(lpfnWndProc));

        // Calculate window size from the client size
        RECT rcWindow = {0, 0, ClientWidth, ClientHeight};
        ::AdjustWindowRect(&rcWindow, dwWindowStyle, FALSE);

        // Create Window
        HWND hWnd = ::CreateWindowExW(
            dwWindowExStyle,
            uhClassName.get(),
            lpWindowName,
            dwWindowStyle,
            X, Y, rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top,
            /* hWndParent */ nullptr,
            /* hMenu */ nullptr,
            /* hInstance */ hInstance,
            /* lpParam */ upWindowProc.get());

        if (!hWnd)
        {
            throw std::runtime_error("FAILED to create window");
        }

        auto uhWindow = unique_handle_t{hWnd, &::DestroyWindow};

        ::ShowWindow(hWnd, SW_NORMAL);
        ::UpdateWindow(hWnd);

        return {
            hWnd,
            [
                win_proc = std::move(upWindowProc),
                win_class = std::move(uhClassName),
                win_handle = std::move(uhWindow)
            ](HWND) mutable
            {
                win_handle.reset();
                win_class.reset();
                win_proc.reset();
            }
        };
    }

    static inline bool ApplyDarkModeForWindow(HWND hWnd)
    {
        HMODULE hNtDll = ::GetModuleHandleW(L"ntdll.dll") ? ::GetModuleHandleW(L"ntdll.dll") : ::LoadLibraryExW(L"ntdll.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        HMODULE hUser32 = ::GetModuleHandleW(L"user32.dll") ? ::GetModuleHandleW(L"user32.dll") : ::LoadLibraryExW(L"user32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        HMODULE hUxtheme = ::GetModuleHandleW(L"uxtheme.dll") ? ::GetModuleHandleW(L"uxtheme.dll") : ::LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

        // Checks Windows Build number.
        if (auto pfnRtlGetVersion = reinterpret_cast<LONG(__stdcall*)(PRTL_OSVERSIONINFOW)>(::GetProcAddress(hNtDll, "RtlGetVersion")))
        {
            if (RTL_OSVERSIONINFOW result{sizeof(RTL_OSVERSIONINFOW),};
                pfnRtlGetVersion(&result) == 0
                && result.dwMajorVersion >= 10
                && result.dwMinorVersion >= 0
                && result.dwBuildNumber >= 19042)
            {
                BOOL enterDarkMode = FALSE;
                enum struct PreferredAppMode { AllowDark = 1 };
                if (auto pfnSetPreferredAppMode = reinterpret_cast<PreferredAppMode(WINAPI*)(PreferredAppMode appMode)>(::GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)))) pfnSetPreferredAppMode(PreferredAppMode::AllowDark);
                if (auto pfnAllowDarkModeForWindow = reinterpret_cast<bool (WINAPI*)(HWND hWnd, bool allow)>(::GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)))) pfnAllowDarkModeForWindow(hWnd, true);
                if (auto pfnRefreshImmersiveColorPolicyState = reinterpret_cast<void (WINAPI*)()>(::GetProcAddress(hUxtheme, MAKEINTRESOURCEA(104)))) pfnRefreshImmersiveColorPolicyState();
                if (auto pfnShouldAppsUseDarkMode = reinterpret_cast<bool (WINAPI*)()>(::GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132))))enterDarkMode = pfnShouldAppsUseDarkMode();

                enum struct WINDOWCOMPOSITIONATTRIB : DWORD
                {
                    WCA_USEDARKMODECOLORS = 26
                };

                struct WINDOWCOMPOSITIONATTRIBDATA
                {
                    WINDOWCOMPOSITIONATTRIB Attrib;
                    PVOID pvData;
                    SIZE_T cbData;
                };

                if (auto pfnSetWindowCompositionAttribute = reinterpret_cast<BOOL(WINAPI*)(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA*)>(GetProcAddress(hUser32, "SetWindowCompositionAttribute")))
                {
                    WINDOWCOMPOSITIONATTRIBDATA data = {WINDOWCOMPOSITIONATTRIB::WCA_USEDARKMODECOLORS, &enterDarkMode, sizeof(enterDarkMode)};
                    return pfnSetWindowCompositionAttribute(hWnd, &data);
                }
            }
        }

        return false;
    }

    static inline void EnablePerMonitorDpiAwarenessV2()
    {
        ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        ::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    static inline SIZE CalculateWindowSizeFromClientSizeWithDpiAwareness(HWND hWnd, SIZE client, int dpi = 0)
    {
        if (dpi == 0) dpi = static_cast<int>(::GetDpiForWindow(hWnd));
        auto style = static_cast<DWORD>(::GetWindowLongPtrW(hWnd, GWL_STYLE));
        auto menu = ::GetMenu(hWnd);
        auto ex_style = static_cast<DWORD>(::GetWindowLongPtrW(hWnd, GWL_EXSTYLE));
        auto request = RECT{0, 0, client.cx * dpi / 96, client.cy * dpi / 96};
        ::AdjustWindowRectExForDpi(&request, style, menu != nullptr, ex_style, dpi);
        auto calculated = SIZE{request.right - request.left, request.bottom - request.top};
        return calculated;
    }

    static inline void ResizeWindowWithDpiAwareness(HWND hWnd, SIZE client, int dpi = 0)
    {
        auto calculated = CalculateWindowSizeFromClientSizeWithDpiAwareness(hWnd, client, dpi);
        auto flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE;
        ::SetWindowPos(hWnd, nullptr, 0, 0, calculated.cx, calculated.cy, flags);
    }

    static inline void MoveWindowToCenterOfMonitor(HWND hWnd)
    {
        const HMONITOR hMonitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

        MONITORINFO mi{sizeof(MONITORINFO),};
        ::GetMonitorInfoW(hMonitor, &mi);

        RECT rc{};
        ::GetWindowRect(hWnd, &rc);

        const auto MonitorWidth = mi.rcWork.right - mi.rcWork.left;
        const auto MonitorHeight = mi.rcWork.bottom - mi.rcWork.top;
        const auto WindowWidth = rc.right - rc.left;
        const auto WindowHeight = rc.bottom - rc.top;
        const auto WindowLeft = mi.rcWork.left + (MonitorWidth / 2 - WindowWidth / 2);
        const auto WindowTop = mi.rcWork.top + (MonitorHeight / 2 - WindowHeight / 2);

        ::SetWindowPos(hWnd, HWND_TOP, WindowLeft, WindowTop, WindowWidth, WindowHeight, SWP_NOSIZE);
    }

    static inline void ProcessMessages()
    {
        MSG msg = {};
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }
}
