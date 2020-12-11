#pragma once

#define WINDOW_CLASS_NAME L"stream-connector-window"

#define MY_WM_UPDATELOG  (WM_USER + 1)
#define MY_WM_ICONNOTIFY (WM_USER + 2)

LRESULT CALLBACK WndProc(_In_ HWND hWnd, _In_ UINT message, _In_ WPARAM wParam, _In_ LPARAM lParam);
