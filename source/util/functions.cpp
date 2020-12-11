#include "../framework.h"
#include "../common.h"
#include "functions.h"

_Use_decl_annotations_
void GenerateRandom16Bytes(BYTE outBytes[16])
{
    BYTE* pLast = outBytes + 16;
    do
    {
        *outBytes = static_cast<BYTE>(rand() * 256 / RAND_MAX);
        ++outBytes;
    } while (outBytes < pLast);
}

_Use_decl_annotations_
HRESULT __cdecl MakeFormattedString(PWSTR* outBuffer, PCWSTR format, ...)
{
    *outBuffer = nullptr;
    va_list args;
    va_start(args, format);
    auto r = _vscwprintf(format, args);
    if (!r)
        return S_OK;
    auto bufSize = static_cast<size_t>(r) + 1;
    auto buf = static_cast<PWSTR>(malloc(sizeof(WCHAR) * bufSize));
    if (!buf)
        return E_OUTOFMEMORY;
    vswprintf_s(buf, bufSize, format, args);
    *outBuffer = buf;
    return S_OK;
}

_Use_decl_annotations_
HRESULT GetErrorString(HRESULT hr, PWSTR* outMessage)
{
    PWSTR p = nullptr;
    if (!::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(hr), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        static_cast<LPWSTR>(static_cast<void*>(&p)), 0, nullptr))
    {
        *outMessage = nullptr;
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    *outMessage = _wcsdup(p);
    ::LocalFree(p);
    if (!*outMessage)
        return E_OUTOFMEMORY;
    return S_OK;
}

_Use_decl_annotations_
HRESULT LoadStringToObject(HINSTANCE hInstance, UINT id, std::wstring& string)
{
    LPWSTR p = nullptr;
    auto r = ::LoadStringW(hInstance, id, reinterpret_cast<LPWSTR>(&p), 0);
    if (!r)
    {
        string.clear();
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    string.assign(p, p + r);
    return S_OK;
}

_Use_decl_annotations_
void ReplaceReturnChars(std::wstring& string)
{
    if (string.empty())
        return;
    std::wstring::size_type pos = 0;
    while ((pos = string.find('\n', pos)) != std::wstring::npos)
    {
        string.replace(pos, 1, L"\r\n");
        pos += 2;
    }
}

static ULONGLONG s_dwPipeCount = 0;

_Use_decl_annotations_
HRESULT CreateOverlappedPipe(bool isOverlapped, HANDLE* outPipeRead, HANDLE* outPipeWrite, SECURITY_ATTRIBUTES* psa)
{
    *outPipeRead = INVALID_HANDLE_VALUE;
    *outPipeWrite = INVALID_HANDLE_VALUE;
    if (!isOverlapped)
    {
        if (!::CreatePipe(outPipeRead, outPipeWrite, psa, 0))
            return HRESULT_FROM_WIN32(::GetLastError());
        return S_OK;
    }
    PWSTR pName;
    auto hr = MakeFormattedString(&pName, PIPE_ANONYMOUS_NAME_FORMAT,
        ::GetCurrentProcessId(), s_dwPipeCount);
    if (FAILED(hr))
        return hr;
    auto hRead = ::CreateNamedPipeW(pName, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, psa);
    if (hRead == INVALID_HANDLE_VALUE)
    {
        auto err = ::GetLastError();
        free(pName);
        return HRESULT_FROM_WIN32(err);
    }
    auto hWrite = ::CreateFileW(pName, GENERIC_WRITE, FILE_SHARE_READ, psa, OPEN_EXISTING, 0, nullptr);
    if (hWrite == INVALID_HANDLE_VALUE)
    {
        auto err = ::GetLastError();
        ::CloseHandle(hRead);
        free(pName);
        return HRESULT_FROM_WIN32(err);
    }
    free(pName);
    ++s_dwPipeCount;
    *outPipeRead = hRead;
    *outPipeWrite = hWrite;
    return S_OK;
}

_Use_decl_annotations_
HRESULT ReadFileTimeout(HANDLE hFile, void* buf, DWORD len, DWORD* outLen, OVERLAPPED* pol, DWORD dwTimeoutMillisec)
{
    ResetOverlapped(pol);
    ::ResetEvent(pol->hEvent);
    if (::ReadFile(hFile, buf, len, outLen, pol))
        return S_OK;
    auto err = ::GetLastError();
    if (err != ERROR_IO_PENDING)
        return HRESULT_FROM_WIN32(err);
    auto r = ::WaitForSingleObject(pol->hEvent, dwTimeoutMillisec);
    if (r != WAIT_OBJECT_0)
    {
        err = ::GetLastError();
        ::CancelIo(hFile);
        return r == WAIT_TIMEOUT ? E_FAIL : HRESULT_FROM_WIN32(err);
    }
    if (!::GetOverlappedResult(hFile, pol, outLen, FALSE))
        return HRESULT_FROM_WIN32(::GetLastError());
    return S_OK;
}

_Use_decl_annotations_
HRESULT WriteFileTimeout(HANDLE hFile, const void* buf, DWORD len, DWORD* outLen, OVERLAPPED* pol, DWORD dwTimeoutMillisec)
{
    ResetOverlapped(pol);
    ::ResetEvent(pol->hEvent);
    if (::WriteFile(hFile, buf, len, outLen, pol))
        return S_OK;
    auto err = ::GetLastError();
    if (err != ERROR_IO_PENDING)
        return HRESULT_FROM_WIN32(err);
    auto r = ::WaitForSingleObject(pol->hEvent, dwTimeoutMillisec);
    if (r != WAIT_OBJECT_0)
    {
        err = ::GetLastError();
        ::CancelIo(hFile);
        return r == WAIT_TIMEOUT ? E_FAIL : HRESULT_FROM_WIN32(err);
    }
    DWORD dw = 0;
    if (!::GetOverlappedResult(hFile, pol, &dw, FALSE))
        return HRESULT_FROM_WIN32(::GetLastError());
    if (outLen)
        *outLen = dw;
    return S_OK;
}

_Use_decl_annotations_
HRESULT ReadFileLineUtf8(HANDLE hFile, PSTR* outLine, OVERLAPPED* pol, DWORD dwTimeoutMillisec)
{
    *outLine = nullptr;
    auto buf = static_cast<char*>(malloc(256));
    if (!buf)
        return E_OUTOFMEMORY;
    size_t bufLen = 256;
    size_t pos = 0;
    auto timeEnd = ::GetTickCount64() + dwTimeoutMillisec;
    while (true)
    {
        char c;
        DWORD len = 0;
        if (!::ReadFile(hFile, &c, 1, &len, pol))
        {
            auto err = ::GetLastError();
            if (err != ERROR_IO_PENDING)
            {
                free(buf);
                return HRESULT_FROM_WIN32(err);
            }
            DWORD interval = 0;
            auto x = ::GetTickCount64();
            if (x < timeEnd)
                interval = static_cast<DWORD>(timeEnd - x);
            if (!::GetOverlappedResult(hFile, pol, &len, TRUE))
            {
                err = ::GetLastError();
                free(buf);
                return HRESULT_FROM_WIN32(err);
            }
        }
        if (c == '\n')
            break;
        buf[pos] = c;
        ++pos;
        if (pos == bufLen)
        {
            bufLen += 256;
            auto p = static_cast<char*>(realloc(buf, bufLen));
            if (!p)
            {
                free(buf);
                return E_OUTOFMEMORY;
            }
        }
    }
    buf[pos] = 0;
    *outLine = buf;
    return S_OK;
}

_Use_decl_annotations_
void ResetOverlapped(OVERLAPPED* pol)
{
    auto h = pol->hEvent;
    ZeroMemory(pol, sizeof(*pol));
    pol->hEvent = h;
}

static wchar_t* s_pszCurrentProcessModuleName = nullptr;

static void __cdecl _CleanupCurrentProcessModuleName()
{
    if (s_pszCurrentProcessModuleName)
    {
        free(s_pszCurrentProcessModuleName);
        s_pszCurrentProcessModuleName = nullptr;
    }
}

HRESULT InitCurrentProcessModuleName(_In_ HINSTANCE hInstance)
{
    PWSTR pszBase = nullptr;
    DWORD dw = MAX_PATH;
    while (true)
    {
        if (pszBase)
        {
            auto p = static_cast<PWSTR>(realloc(pszBase, sizeof(WCHAR) * dw));
            if (!p)
            {
                free(pszBase);
                return E_OUTOFMEMORY;
            }
            pszBase = p;
        }
        else
        {
            pszBase = static_cast<PWSTR>(malloc(sizeof(WCHAR) * dw));
            if (!pszBase)
                return E_OUTOFMEMORY;
        }
        auto r = ::GetModuleFileNameW(hInstance, pszBase, dw);
        if (!r)
        {
            auto err = ::GetLastError();
            free(pszBase);
            return HRESULT_FROM_WIN32(err);
        }
        if (r != dw)
            break;
        dw += MAX_PATH;
    }

    dw = ::GetLongPathNameW(pszBase, nullptr, 0);
    if (!dw)
    {
        auto err = ::GetLastError();
        free(pszBase);
        return HRESULT_FROM_WIN32(err);
    }
    ++dw;
    auto psz = static_cast<PWSTR>(malloc(sizeof(WCHAR) * dw));
    if (!psz)
    {
        free(pszBase);
        return E_OUTOFMEMORY;
    }
    ::GetLongPathNameW(pszBase, psz, dw);
    s_pszCurrentProcessModuleName = psz;
    atexit(_CleanupCurrentProcessModuleName);
    return S_OK;
}

PCWSTR GetCurrentProcessModuleName()
{
    return s_pszCurrentProcessModuleName;
}

_Use_decl_annotations_
bool IsSameProcessToCurrent(DWORD dwProcessId)
{
    auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
    if (!hProcess)
        return false;
    // retrieve with maximum length = 'wcslen(s_pszCurrentProcessModuleName) + 1'
    // e.g.1:
    // - currentProcess: 'C:\foo.exe'
    // - targetProcess: 'C:\foobar.exe'
    // - targetProcessImage would be L"C:\\foobar.e"
    // e.g.2:
    // - currentProcess: 'C:\foo.exe'
    // - targetProcess: 'C:\foo.exe2'
    // - targetProcessImage would be L"C:\\foo.exe2"
    auto maxLen = wcslen(s_pszCurrentProcessModuleName) + 2;
    auto targetProcessImage = static_cast<PWSTR>(malloc(sizeof(WCHAR) * maxLen));
    if (!targetProcessImage)
    {
        ::CloseHandle(hProcess);
        return false;
    }
    DWORD dw = static_cast<DWORD>(maxLen);
    if (!::QueryFullProcessImageNameW(hProcess, 0, targetProcessImage, &dw))
    {
        ::CloseHandle(hProcess);
        return false;
    }
    ::CloseHandle(hProcess);
    targetProcessImage[maxLen - 1] = 0;

    // compare the path name only (hard link or symbolic link is not treated as same here)
    // NOTE: both currentProcessImage and targetProcessImage should be full path
    auto ret = (_wcsicmp(s_pszCurrentProcessModuleName, targetProcessImage) == 0);

    free(targetProcessImage);
    return ret;
}
