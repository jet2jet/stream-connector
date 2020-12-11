#include "../framework.h"
#include "logger.h"

Logger* logger = nullptr;

_Use_decl_annotations_
void Logger::AddLog(LogLevel level, PCWSTR pszLog)
{
    auto len = wcslen(pszLog);
    if (!len)
        return;
    std::wstring str(pszLog);
    if (pszLog[len - 1] != L'\n')
        str += L'\n';
    OnLog(level, str.c_str());
}

_Use_decl_annotations_
void __cdecl Logger::AddLogFormatted(LogLevel level, PCWSTR pszLog, ...)
{
    va_list va;
    va_start(va, pszLog);
    AddLogFormattedV(level, pszLog, va);
    va_end(va);
}

_Use_decl_annotations_
void Logger::AddLogFormattedV(LogLevel level, PCWSTR pszLog, va_list args)
{
    auto r = _vscwprintf(pszLog, args);
    if (r <= 0)
        return;
    auto size = static_cast<size_t>(r) + 1;
    auto psz = static_cast<PWSTR>(malloc(sizeof(WCHAR) * size));
    if (!psz)
        return;
    vswprintf_s(psz, size, pszLog, args);

    AddLog(level, psz);
    free(psz);
}
