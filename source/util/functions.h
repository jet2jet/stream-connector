#pragma once

void GenerateRandom16Bytes(_Out_writes_bytes_all_(16) BYTE outBytes[16]);

HRESULT __cdecl MakeFormattedString(_Out_ PWSTR* outBuffer, _In_z_ _Printf_format_string_ PCWSTR format, ...);
void ReplaceReturnChars(_Inout_ std::wstring& string);
HRESULT GetErrorString(_In_ HRESULT hr, _Out_ PWSTR* outMessage);
HRESULT LoadStringToObject(_In_ HINSTANCE hInstance, _In_ UINT id, _Out_ std::wstring& string);

HRESULT CreateOverlappedPipe(_In_ bool isOverlapped, _Out_ HANDLE* outPipeRead, _Out_ HANDLE* outPipeWrite, _In_opt_ SECURITY_ATTRIBUTES* psa);

HRESULT ReadFileTimeout(
    _In_ HANDLE hFile,
    _Out_writes_bytes_to_(len, *outLen) __out_data_source(NETWORK) void* buf,
    _In_ DWORD len,
    _Out_ DWORD* outLen,
    _Inout_ OVERLAPPED* pol,
    _In_ DWORD dwTimeoutMillisec
);
HRESULT WriteFileTimeout(
    _In_ HANDLE hFile,
    _In_reads_bytes_opt_(len) const void* buf,
    _In_ DWORD len,
    _Out_opt_ DWORD* outLen,
    _Inout_ OVERLAPPED* pol,
    _In_ DWORD dwTimeoutMillisec
);
_Check_return_
HRESULT ReadFileLineUtf8(
    _In_ HANDLE hFile,
    _When_(SUCCEEDED(return), _Outptr_result_z_) PSTR* outLine,
    _Inout_ OVERLAPPED* pol,
    _In_ DWORD dwTimeoutMillisec
);
void ResetOverlapped(_Inout_ OVERLAPPED* pol);

HRESULT InitCurrentProcessModuleName(_In_ HINSTANCE hInstance);
PCWSTR GetCurrentProcessModuleName();

bool IsSameProcessToCurrent(_In_ DWORD dwProcessId);
