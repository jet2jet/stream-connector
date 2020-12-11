#pragma once

class Duplex;

typedef void (CALLBACK* PFinishHandler)(_In_ void* data, _In_ HRESULT hr);

typedef void (__cdecl* PAddLogFormatted)(_In_ LogLevel level, _In_z_ _Printf_format_string_ PCWSTR pszLog, ...);

HRESULT Transfer(_In_ HANDLE hEventQuit, _In_ Duplex* from, _In_ Duplex* to, _In_opt_ PAddLogFormatted logger);

_Check_return_
HRESULT StartWorker(_Out_ HANDLE* outThread, _In_ HANDLE hEventQuit, _In_ Duplex* duplexIn, _In_ Duplex* duplexOut,
    _In_opt_ PFinishHandler pfnFinishHandler, _In_opt_ void* dataHandler);
