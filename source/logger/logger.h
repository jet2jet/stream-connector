#pragma once

enum class LogLevel : BYTE
{
    Error = 0,
    Info,
    Debug,
    _Count
};

class Logger
{
public:
    virtual ~Logger() {}

    void AddLog(_In_ LogLevel level, _In_z_ PCWSTR pszLog);
    void __cdecl AddLogFormatted(_In_ LogLevel level, _In_z_ _Printf_format_string_ PCWSTR pszLog, ...);
    void AddLogFormattedV(_In_ LogLevel level, _In_z_ _Printf_format_string_ PCWSTR pszLog, va_list args);

protected:
    virtual void OnLog(_In_ LogLevel level, _In_z_ PCWSTR pszLog) = 0;
};

extern Logger* logger;

inline void AddLog(_In_ LogLevel level, _In_z_ PCWSTR pszLog)
{
    if (!logger)
        return;
    logger->AddLog(level, pszLog);
}

inline void __cdecl AddLogFormatted(_In_ LogLevel level, _In_z_ _Printf_format_string_ PCWSTR pszLog, ...)
{
    if (!logger)
        return;
    va_list va;
    va_start(va, pszLog);
    logger->AddLogFormattedV(level, pszLog, va);
    va_end(va);
}

