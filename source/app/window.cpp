#include "../framework.h"
#include "../resource.h"
#include "../util/functions.h"

#include "app.h"
#include "window.h"

#include "simple_dialog.h"

constexpr UINT ID_NOTIFYICON = 1;

constexpr auto PADDING_CHILDREN = 10;
constexpr auto PADDING_BETWEEN_LABEL_AND_FIELD = 2;
constexpr auto PADDING_BETWEEN_FIELDS = 4;
#define TEXT_HEIGHT_TO_LABEL_HEIGHT(th) ((th) + 2)

struct WindowData
{
    HWND hWndLabelStatus;
    HWND hWndStatus;
    HWND hWndLabelLog;
    HWND hWndLog;
    HWND hWndLastFocus;
    int textHeight;
};

static WNDPROC s_pfnEditProc = nullptr;
static LRESULT CALLBACK EditWndProc(_In_ HWND hWnd, _In_ UINT message, _In_ WPARAM wParam, _In_ LPARAM lParam);

static bool OnCreate(_In_ HWND hWnd, _In_ WindowData* data);
static void OnRefreshFont(_In_ HWND hWnd, _In_ WindowData* data, _In_ bool skipResize = false);
static void OnResize(_In_ HWND hWnd, _In_ WindowData* data);
static void OnCommand(_In_ HWND hWnd, _In_ WindowData* data, _In_ UINT uCmdID);
static void OnIconNotify(_In_ HWND hWnd, _In_ WindowData* data, _In_ WPARAM wParam, _In_ LPARAM lParam);

static HWND GetControlFocus(_In_ HWND hWnd)
{
    auto hTarget = ::GetFocus();
    if (!hTarget)
        return hTarget;
    auto hParent = ::GetParent(hTarget);
    while (hParent)
    {
        if (hParent == hWnd)
            return hTarget;
        hParent = ::GetParent(hParent);
    }
    return nullptr;
}

static void AddNotifyIcon(_In_ HWND hWnd)
{
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE |  NIF_SHOWTIP;
    nid.hWnd = hWnd;
    nid.uID = ID_NOTIFYICON;
    constexpr auto maxLen = std::extent<decltype(nid.szTip)>::value;
    wcsncpy_s(nid.szTip, GetAppTitle(), maxLen);
    nid.szTip[maxLen - 1] = 0;
    // TODO: icon
    nid.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
    nid.uCallbackMessage = MY_WM_ICONNOTIFY;

    ::Shell_NotifyIconW(NIM_ADD, &nid);
    nid.uVersion = NOTIFYICON_VERSION_4;
    ::Shell_NotifyIconW(NIM_SETVERSION, &nid);
}

static void RemoveNotifyIcon(_In_ HWND hWnd)
{
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.uFlags = 0;
    nid.hWnd = hWnd;
    nid.uID = ID_NOTIFYICON;

    ::Shell_NotifyIconW(NIM_DELETE, &nid);
}

static void OnUpdateLog(_In_ HWND hWndLog, _In_z_ PCWSTR pszLog)
{
    std::wstring str(pszLog);
    ReplaceReturnChars(str);
    if (str.size() > 65535)
    {
        str.replace(0, str.size() - 65535, L"");
    }

    auto dw = Edit_GetSel(hWndLog);
    auto selStart = LOWORD(dw);
    auto selEnd = HIWORD(dw);
    auto curTextLen = GetWindowTextLengthW(hWndLog);
    if (selStart == selEnd && selEnd == curTextLen)
    {
        selStart = selEnd = static_cast<WORD>(str.size());
    }
    SetWindowTextW(hWndLog, str.c_str());
    Edit_SetSel(hWndLog, selStart, selEnd);
    Edit_ScrollCaret(hWndLog);
}

_Use_decl_annotations_
static LRESULT CALLBACK EditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto lr = ::CallWindowProcW(s_pfnEditProc, hWnd, message, wParam, lParam);
    if (message == WM_GETDLGCODE)
    {
        lr &= ~(DLGC_WANTTAB | DLGC_WANTALLKEYS);
    }
    return lr;
}

_Use_decl_annotations_
static bool OnCreate(HWND hWnd, WindowData* data)
{
    std::wstring str;
    ReportListenersAndConnector(str);
    ReplaceReturnChars(str);

    data->hWndLabelStatus = ::CreateWindowExW(0, L"Static", L"Status:",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        hWnd, nullptr, nullptr, nullptr);
    if (!data->hWndLabelStatus)
        return false;
    data->hWndStatus = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", str.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY | ES_MULTILINE,
        0, 0, 0, 0,
        hWnd, nullptr, nullptr, nullptr);
    if (!data->hWndStatus)
        return false;
    data->hWndLabelLog = ::CreateWindowExW(0, L"Static", L"Log:",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        hWnd, nullptr, nullptr, nullptr);
    if (!data->hWndLabelLog)
        return false;
    data->hWndLog = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY | ES_MULTILINE,
        0, 0, 0, 0,
        hWnd, nullptr, nullptr, nullptr);
    if (!data->hWndLog)
        return false;

    s_pfnEditProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(data->hWndStatus, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&EditWndProc)));
    ::SetWindowLongPtrW(data->hWndLog, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&EditWndProc));

    auto hMenu = ::LoadMenuW(GetAppInstance(), MAKEINTRESOURCEW(IDR_MAIN));
    if (!hMenu)
        return false;
    ::SetMenu(hWnd, hMenu);

    AddNotifyIcon(hWnd);
    OnRefreshFont(hWnd, data);

    return true;
}

_Use_decl_annotations_
static void OnRefreshFont(HWND hWnd, WindowData* data, bool skipResize)
{
    HFONT hFont = GetStockFont(DEFAULT_GUI_FONT);

    SetWindowFont(hWnd, hFont, TRUE);
    SetWindowFont(data->hWndLabelStatus, hFont, TRUE);
    SetWindowFont(data->hWndStatus, hFont, TRUE);
    SetWindowFont(data->hWndLabelLog, hFont, TRUE);
    SetWindowFont(data->hWndLog, hFont, TRUE);

    auto hDC = ::GetDC(hWnd);
    if (!hDC)
        return;
    TEXTMETRICW tm;
    auto hPrevFont = SelectFont(hDC, hFont);
    ::GetTextMetricsW(hDC, &tm);
    SelectFont(hDC, hPrevFont);
    ::ReleaseDC(hWnd, hDC);
    data->textHeight = tm.tmHeight;

    if (!skipResize)
        OnResize(hWnd, data);
}

_Use_decl_annotations_
static void OnResize(HWND hWnd, WindowData* data)
{
    RECT rc = { 0 };
    ::GetClientRect(hWnd, &rc);

    int y = PADDING_CHILDREN;
    int w = rc.right - (PADDING_CHILDREN * 2);
    if (w < 0)
        w = 4;
    auto hDWP = ::BeginDeferWindowPos(4);

    int hLabel = TEXT_HEIGHT_TO_LABEL_HEIGHT(data->textHeight);
    ::DeferWindowPos(hDWP, data->hWndLabelStatus, nullptr,
        PADDING_CHILDREN, y, w, hLabel, SWP_NOACTIVATE | SWP_DRAWFRAME);
    y += hLabel + PADDING_BETWEEN_LABEL_AND_FIELD;
    const int textFieldMinHeight = data->textHeight + 8;
    int hStatus = (data->textHeight + 4) * 5 + 4;

    int yBottom = rc.bottom - PADDING_CHILDREN;
    int hLog = textFieldMinHeight; // minimum value
    if (hStatus > yBottom - y - (PADDING_BETWEEN_FIELDS + hLabel + PADDING_BETWEEN_LABEL_AND_FIELD + hLog))
    {
        hStatus = yBottom - y - (PADDING_BETWEEN_FIELDS + hLabel + PADDING_BETWEEN_LABEL_AND_FIELD + hLog);
        if (hStatus < textFieldMinHeight)
            hStatus = textFieldMinHeight;
    }
    ::DeferWindowPos(hDWP, data->hWndStatus, nullptr,
        PADDING_CHILDREN, y, w, hStatus, SWP_NOACTIVATE | SWP_DRAWFRAME);

    int yLabelLog = y + hStatus + PADDING_BETWEEN_FIELDS;;
    ::DeferWindowPos(hDWP, data->hWndLabelLog, nullptr,
        PADDING_CHILDREN, yLabelLog, w, hLabel, SWP_NOACTIVATE | SWP_DRAWFRAME);

    int yLog = yLabelLog + hLabel + PADDING_BETWEEN_LABEL_AND_FIELD;
    hLog = yBottom - yLog;
    if (hLog < textFieldMinHeight)
        hLog = textFieldMinHeight;
    ::DeferWindowPos(hDWP, data->hWndLog, nullptr,
        PADDING_CHILDREN, yLog, w, hLog, SWP_NOACTIVATE | SWP_DRAWFRAME);

    ::EndDeferWindowPos(hDWP);
}

_Use_decl_annotations_
static void OnCommand(HWND hWnd, WindowData* data, UINT uCmdID)
{
    switch (uCmdID)
    {
        case ID_FILE_EXIT:
            //::SendMessageW(hWnd, WM_CLOSE, 0, 0);
            ::DestroyWindow(hWnd);
            break;
        case ID_FILE_CLOSE:
            ::SendMessageW(hWnd, WM_CLOSE, 0, 0);
            break;
        case ID_NOTIFY_STATUS:
            ::ShowWindow(hWnd, SW_SHOW);
            ::SetForegroundWindow(hWnd);
            break;
        case ID_HELP_ABOUT:
            ShowDialog(MAKEINTRESOURCEW(IDD_ABOUTBOX), hWnd);
            break;
    }
}

_Use_decl_annotations_
static void OnIconNotify(HWND hWnd, WindowData* data, WPARAM wParam, LPARAM lParam)
{
    switch (LOWORD(lParam))
    {
        case WM_LBUTTONDBLCLK:
        case NIN_KEYSELECT:
            ::SendMessageW(hWnd, WM_COMMAND, GET_WM_COMMAND_MPS(ID_NOTIFY_STATUS, nullptr, 0));
            break;
        case WM_CONTEXTMENU:
        {
            auto hMenu = ::LoadMenuW(GetAppInstance(), MAKEINTRESOURCEW(IDR_POPUP));
            if (hMenu)
            {
                auto hPopup = ::GetSubMenu(hMenu, 0);
                if (hPopup)
                {
                    POINT pt = { LOWORD(wParam), HIWORD(wParam) };
                    ::SetForegroundWindow(hWnd);

                    UINT uFlags = TPM_RIGHTBUTTON;
                    if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
                    {
                        uFlags |= TPM_RIGHTALIGN;
                    }
                    else
                    {
                        uFlags |= TPM_LEFTALIGN;
                    }

                    ::TrackPopupMenuEx(hPopup, uFlags, pt.x, pt.y, hWnd, NULL);
                }
                ::DestroyMenu(hMenu);
            }
        }
        break;
    }
}

_Use_decl_annotations_
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto data = reinterpret_cast<WindowData*>(::GetWindowLongPtrW(hWnd, 0));
    switch (message)
    {
        case WM_NCCREATE:
            data = static_cast<WindowData*>(malloc(sizeof(WindowData)));
            if (!data)
                return FALSE;
            ZeroMemory(data, sizeof(WindowData));
            ::SetWindowLongPtrW(hWnd, 0, reinterpret_cast<LONG_PTR>(data));
            break;
        case WM_NCDESTROY:
            ::SetWindowLongPtrW(hWnd, 0, reinterpret_cast<LONG_PTR>(nullptr));
            if (data)
                free(data);
            break;
        case WM_CREATE:
        {
            if (!data)
                return -1;
            auto lr = DefWindowProcW(hWnd, message, wParam, lParam);
            if (lr != 0)
                return lr;
            if (!OnCreate(hWnd, data))
                return -1;
            return 0;
        }
        case WM_SIZE:
            DefWindowProcW(hWnd, message, wParam, lParam);
            OnResize(hWnd, data);
            break;
        case WM_SETTINGCHANGE:
        case WM_DPICHANGED:
            DefWindowProcW(hWnd, message, wParam, lParam);
            OnRefreshFont(hWnd, data);
            return 0;
        case WM_CLOSE:
            ::ShowWindow(hWnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            RemoveNotifyIcon(hWnd);
            ::PostQuitMessage(0);
            break;
        case WM_GETDLGCODE:
            return static_cast<LRESULT>(DLGC_WANTTAB | DLGC_WANTALLKEYS);
        case WM_COMMAND:
            OnCommand(hWnd, data, GET_WM_COMMAND_ID(wParam, lParam));
            return 0;

        // save/restore control focus
        // (refs. https://docs.microsoft.com/en-us/windows/win32/dlgbox/dlgbox-programming-considerations#dialog-box-default-message-processing )
        case WM_SHOWWINDOW:
            if (!wParam)
                data->hWndLastFocus = ::GetControlFocus(hWnd);
            break;
        case WM_SYSCOMMAND:
            if (wParam == SC_MINIMIZE)
                data->hWndLastFocus = ::GetControlFocus(hWnd);
            break;
        case WM_SETFOCUS:
            ::SetFocus(data->hWndLastFocus ? data->hWndLastFocus : data->hWndStatus);
            return 0;
        case WM_ACTIVATE:
            ::SetFocus(data->hWndLastFocus ? data->hWndLastFocus : data->hWndStatus);
            break;

        case WM_ENDSESSION:
            if (wParam)
                ProcessShutdown();
            break;
        case MY_WM_UPDATELOG:
            OnUpdateLog(data->hWndLog, reinterpret_cast<PCWSTR>(lParam));
            return 0;
        case MY_WM_ICONNOTIFY:
            OnIconNotify(hWnd, data, wParam, lParam);
            return 0;
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}
