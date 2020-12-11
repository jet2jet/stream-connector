#include "../framework.h"
#include "app.h"
#include "simple_dialog.h"

#include "../resource.h"
#include "../util/functions.h"

static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_COMMAND)
    {
        auto id = GET_WM_COMMAND_ID(wParam, lParam);
        if (id == IDOK || id == IDCANCEL)
        {
            ::EndDialog(hWnd, static_cast<INT_PTR>(id));
            return TRUE;
        }
    }
    return FALSE;
}

static INT_PTR CALLBACK UsageDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG)
    {
        std::wstring str(reinterpret_cast<LPCWSTR>(lParam));
        ReplaceReturnChars(str);
        ::SetDlgItemTextW(hWnd, IDC_USAGE_TEXT, str.c_str());
        return 0;
    }
    return DlgProc(hWnd, message, wParam, lParam);
}

_Use_decl_annotations_
void ShowDialog(LPCWSTR lpszTemplateName, HWND hWndParent)
{
    ::DialogBoxParamW(GetAppInstance(), lpszTemplateName, hWndParent, DlgProc, 0);
}

_Use_decl_annotations_
void ShowUsageDialog(PCWSTR pszText, HWND hWndParent)
{
    ::DialogBoxParamW(GetAppInstance(), MAKEINTRESOURCEW(IDD_USAGE), hWndParent, UsageDlgProc, reinterpret_cast<LPARAM>(pszText));
}

