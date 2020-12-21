#pragma once

#include "../options.h"
#include "../logger/logger.h"

void ReportListenersAndConnector(_Out_ std::wstring& outString);
LogLevel GetLogLevel();
void ClearLogs();

DWORD GetWslDefaultTimeout();
PCWSTR GetWslSocatLogLevel();

HINSTANCE GetAppInstance();
PCWSTR GetAppTitle();

void ProcessShutdown();

int AppMain(_In_ HINSTANCE hInstance, _In_ int nCmdShow, _In_ const Option& options);
