#include "../framework.h"

#include "functions.h"
#include "event_handler.h"

#include "wsl_util.h"

#ifdef _WIN64

static PWSTR s_pszWslExePath = nullptr;
static PWSTR s_pszWslPathPath = nullptr;

static void __cdecl _CleanupWsl()
{
    if (s_pszWslPathPath)
    {
        free(s_pszWslPathPath);
        s_pszWslPathPath = nullptr;
    }
    if (s_pszWslExePath)
    {
        free(s_pszWslExePath);
        s_pszWslExePath = nullptr;
    }
}

_When_(SUCCEEDED(return), _Post_satisfies_(s_pszWslExePath != nullptr))
static HRESULT _InitWsl()
{
    if (s_pszWslExePath)
        return S_OK;

    auto size = ::GetSystemDirectoryW(nullptr, 0);
    // 'size' should include null-terminate char
    if (!size)
        return HRESULT_FROM_WIN32(::GetLastError());
    if (size == 1)
        return E_UNEXPECTED;
    ++size;
    std::wstring str;
    try
    {
        str.resize(size);
    }
    catch (...)
    {
        return E_OUTOFMEMORY;
    }
    auto len = ::GetSystemDirectoryW(&str.at(0), size);
    if (!len)
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    str.resize(len);
    if (*str.rbegin() != L'\\')
    {
        str += L'\\';
    }
    str += L"wsl.exe";

    auto attr = ::GetFileAttributesW(str.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
        return __HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

    auto p = _wcsdup(str.c_str());
    if (!p)
        return E_OUTOFMEMORY;
    s_pszWslExePath = p;
    atexit(_CleanupWsl);
    return S_OK;
}

static HRESULT _WaitAndGetOutput(_In_ HANDLE hProcess, _In_ HANDLE hStdOutRead, _In_ DWORD dwTimeoutMillisec, _When_(SUCCEEDED(return), _Outptr_result_maybenull_z_) PWSTR* outOutput)
{
    auto r = ::WaitForSingleObject(hProcess, dwTimeoutMillisec);
    if (r == WAIT_FAILED || r == WAIT_TIMEOUT)
    {
        auto dw = r == WAIT_FAILED ? ::GetLastError() : ERROR_TIMEOUT;
        ::TerminateProcess(hProcess, static_cast<UINT>(-1));
        return HRESULT_FROM_WIN32(dw);
    }
    DWORD dwExitCode = 0;
    if (!::GetExitCodeProcess(hProcess, &dwExitCode))
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    if (dwExitCode == 0xFFFFFFFF)
    {
        // the error code '0xFFFFFFFF' (-1 for signed int) must be returned by Windows wsl.exe
        // (Linux command does not return negative exit code)
        return E_FAIL;
    }

    DWORD dwAvailSize = 0;
    if (!::PeekNamedPipe(hStdOutRead, nullptr, 0, nullptr, &dwAvailSize, nullptr))
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    if (dwAvailSize == 0)
    {
        *outOutput = nullptr;
    }
    else
    {
        auto dwBufferSize = dwAvailSize + 1;
        auto pszUtf8 = static_cast<PSTR>(malloc(dwBufferSize));
        if (!pszUtf8)
        {
            return E_OUTOFMEMORY;
        }
        if (!::ReadFile(hStdOutRead, pszUtf8, dwAvailSize, &dwAvailSize, nullptr))
        {
            auto dw = ::GetLastError();
            free(pszUtf8);
            return HRESULT_FROM_WIN32(dw);
        }

        dwBufferSize = static_cast<DWORD>(::MultiByteToWideChar(CP_UTF8, 0, pszUtf8, static_cast<int>(dwAvailSize), nullptr, 0)) + 1;
        auto pOut = static_cast<PWSTR>(malloc(sizeof(WCHAR) * dwBufferSize));
        if (!pOut)
        {
            free(pszUtf8);
            return E_OUTOFMEMORY;
        }
        ::MultiByteToWideChar(CP_UTF8, 0, pszUtf8, static_cast<int>(dwAvailSize), pOut, static_cast<int>(dwBufferSize));
        pOut[dwBufferSize - 1] = 0;
        *outOutput = pOut;
    }

    return dwExitCode == 0 ? S_OK : S_FALSE;
}

_Use_decl_annotations_
HRESULT WslGetDefaultDistribution(PWSTR* outDistribution)
{
    HKEY hKey;
    auto err = ::RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Lxss", 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hKey);
    if (err != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(err);
    DWORD dwType = 0;
    DWORD dwLen = 0;
    err = ::RegQueryValueExW(hKey, L"DefaultDistribution", nullptr, &dwType, nullptr, &dwLen);
    if (err != ERROR_SUCCESS)
    {
        ::RegCloseKey(hKey);
        return HRESULT_FROM_WIN32(err);
    }
    if (dwType != REG_SZ && dwType != REG_EXPAND_SZ && dwType != REG_MULTI_SZ)
    {
        ::RegCloseKey(hKey);
        return E_UNEXPECTED;
    }
    if (dwLen <= sizeof(WCHAR))
    {
        ::RegCloseKey(hKey);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
    auto pszDistroId = static_cast<PWSTR>(malloc(dwLen));
    if (!pszDistroId)
    {
        ::RegCloseKey(hKey);
        return E_OUTOFMEMORY;
    }
    err = ::RegQueryValueExW(hKey, L"DefaultDistribution", nullptr, &dwType, reinterpret_cast<LPBYTE>(pszDistroId), &dwLen);
    if (err != ERROR_SUCCESS)
    {
        free(pszDistroId);
        ::RegCloseKey(hKey);
        return HRESULT_FROM_WIN32(err);
    }

    DWORD cb = 0;
    err = ::RegGetValueW(hKey, pszDistroId, L"DistributionName", RRF_RT_REG_SZ | RRF_RT_REG_MULTI_SZ | RRF_RT_REG_EXPAND_SZ,
        nullptr, nullptr, &cb);
    if (err != ERROR_SUCCESS)
    {
        free(pszDistroId);
        ::RegCloseKey(hKey);
        return HRESULT_FROM_WIN32(err);
    }
    auto p = static_cast<PWSTR>(malloc(cb));
    err = ::RegGetValueW(hKey, pszDistroId, L"DistributionName", RRF_RT_REG_SZ | RRF_RT_REG_MULTI_SZ | RRF_RT_REG_EXPAND_SZ,
        nullptr, p, &dwLen);
    free(pszDistroId);
    ::RegCloseKey(hKey);
    if (err != ERROR_SUCCESS)
    {
        free(p);
        return HRESULT_FROM_WIN32(err);
    }

    // check if distribution is available for use
    __try
    {
        if (!::WslIsDistributionRegistered(p))
        {
            free(p);
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        // 'wslapi.dll' is not found -- WSL is not available
        free(p);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    if (outDistribution)
    {
        *outDistribution = p;
    }
    else
    {
        free(p);
    }
    return S_OK;
}

_Use_decl_annotations_
HRESULT WslIsValidDistribution(PCWSTR pszDistribution)
{
    if (pszDistribution && wcschr(pszDistribution, L' '))
        return E_INVALIDARG;

    HKEY hKey;
    auto err = ::RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Lxss", 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hKey);
    if (err != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(err);
    if (!pszDistribution || !*pszDistribution)
    {
        // if distribution is not specified (using default distribution),
        // check whether the default distribution is specified
        DWORD dwType = 0;
        DWORD dwLen = 0;
        err = ::RegQueryValueExW(hKey, L"DefaultDistribution", nullptr, &dwType, nullptr, &dwLen);
        ::RegCloseKey(hKey);
        if (err != ERROR_SUCCESS)
        {
            if (err == ERROR_FILE_NOT_FOUND)
                return S_FALSE;
            return HRESULT_FROM_WIN32(err);
        }
        if (dwType != REG_SZ && dwType != REG_EXPAND_SZ && dwType != REG_MULTI_SZ)
            return E_UNEXPECTED;
        if (dwLen <= sizeof(WCHAR))
            return S_FALSE;
        return S_OK;
    }
    auto pszKeyName = static_cast<PWSTR>(malloc(sizeof(WCHAR) * 256));
    if (!pszKeyName)
    {
        ::RegCloseKey(hKey);
        return E_OUTOFMEMORY;
    }
    DWORD dwIndex = 0;
    while (true)
    {
        DWORD dwNameLen = 256;
        err = ::RegEnumKeyExW(hKey, dwIndex, pszKeyName, &dwNameLen, nullptr, nullptr, nullptr, nullptr);
        if (err != ERROR_SUCCESS)
        {
            auto hr = HRESULT_FROM_WIN32(err);
            if (err == ERROR_NO_MORE_ITEMS)
                hr = S_FALSE;
            free(pszKeyName);
            ::RegCloseKey(hKey);
            return hr;
        }

        DWORD cb = 0;
        err = ::RegGetValueW(hKey, pszKeyName, L"DistributionName", RRF_RT_REG_SZ | RRF_RT_REG_MULTI_SZ | RRF_RT_REG_EXPAND_SZ,
            nullptr, nullptr, &cb);
        if (err != ERROR_SUCCESS)
        {
            free(pszKeyName);
            ::RegCloseKey(hKey);
            return HRESULT_FROM_WIN32(err);
        }
        auto p = static_cast<PWSTR>(malloc(cb));
        err = ::RegGetValueW(hKey, pszKeyName, L"DistributionName", RRF_RT_REG_SZ | RRF_RT_REG_MULTI_SZ | RRF_RT_REG_EXPAND_SZ,
            nullptr, p, &cb);
        if (err != ERROR_SUCCESS)
        {
            free(p);
            free(pszKeyName);
            ::RegCloseKey(hKey);
            return HRESULT_FROM_WIN32(err);
        }
        // distribution name is case-insensitive
        if (_wcsicmp(p, pszDistribution) == 0)
        {
            free(p);
            free(pszKeyName);
            ::RegCloseKey(hKey);
            return S_OK;
        }

        ++dwIndex;
    }
    // unreachable
    __assume(false);
}

_Use_decl_annotations_
HRESULT WslExecute(
    PCWSTR pszDistribution,
    PCWSTR pszCommandLine,
    bool isPipeOverlapped,
    HANDLE* outProcess,
    PipeData* outStdIn,
    PipeData* outStdOut,
    PipeData* outStdErr
)
{
    auto hr = _InitWsl();
    if (FAILED(hr))
        return hr;
    PWSTR psz;
    if (pszDistribution && *pszDistribution)
    {
        if (wcschr(pszDistribution, L' '))
            return E_INVALIDARG;
        hr = MakeFormattedString(&psz, L"\"%s\" -d %s -e %s", s_pszWslExePath, pszDistribution, pszCommandLine);
    }
    else
        hr = MakeFormattedString(&psz, L"\"%s\" -e %s", s_pszWslExePath, pszCommandLine);
    if (FAILED(hr))
        return hr;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    HANDLE hPipeStdInRead, hPipeStdInWrite;
    HANDLE hPipeStdOutRead, hPipeStdOutWrite;
    HANDLE hPipeStdErrRead, hPipeStdErrWrite;
    hr = CreateOverlappedPipe(isPipeOverlapped , &hPipeStdInRead, &hPipeStdInWrite, &sa);
    if (FAILED(hr))
    {
        free(psz);
        return hr;
    }
    hr = CreateOverlappedPipe(isPipeOverlapped, &hPipeStdOutRead, &hPipeStdOutWrite, &sa);
    if (FAILED(hr))
    {
        free(psz);
        ::CloseHandle(hPipeStdInRead);
        ::CloseHandle(hPipeStdInWrite);
        return hr;
    }
    hr = CreateOverlappedPipe(isPipeOverlapped, &hPipeStdErrRead, &hPipeStdErrWrite, &sa);
    if (FAILED(hr))
    {
        free(psz);
        ::CloseHandle(hPipeStdInRead);
        ::CloseHandle(hPipeStdInWrite);
        ::CloseHandle(hPipeStdOutRead);
        ::CloseHandle(hPipeStdOutWrite);
        return hr;
    }
    if (!::SetHandleInformation(hPipeStdInWrite, HANDLE_FLAG_INHERIT, 0) ||
        !::SetHandleInformation(hPipeStdOutRead, HANDLE_FLAG_INHERIT, 0) ||
        !::SetHandleInformation(hPipeStdErrRead, HANDLE_FLAG_INHERIT, 0))
    {
        auto dw = ::GetLastError();
        free(psz);
        ::CloseHandle(hPipeStdInRead);
        ::CloseHandle(hPipeStdInWrite);
        ::CloseHandle(hPipeStdOutRead);
        ::CloseHandle(hPipeStdOutWrite);
        ::CloseHandle(hPipeStdErrRead);
        ::CloseHandle(hPipeStdErrWrite);
        return HRESULT_FROM_WIN32(dw);
    }

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);
    si.hStdInput = hPipeStdInRead;
    si.hStdOutput = hPipeStdOutWrite;
    si.hStdError = hPipeStdErrWrite;
    si.dwFlags = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi = { 0 };
    if (!::CreateProcessW(nullptr, psz, nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi))
    {
        auto dw = ::GetLastError();
        free(psz);
        ::CloseHandle(hPipeStdInRead);
        ::CloseHandle(hPipeStdInWrite);
        ::CloseHandle(hPipeStdOutRead);
        ::CloseHandle(hPipeStdOutWrite);
        ::CloseHandle(hPipeStdErrRead);
        ::CloseHandle(hPipeStdErrWrite);
        return HRESULT_FROM_WIN32(dw);
    }
    free(psz);
    ::CloseHandle(pi.hThread);

    *outProcess = pi.hProcess;
    if (outStdIn)
        *outStdIn = { hPipeStdInRead, hPipeStdInWrite };
    else
    {
        ::CloseHandle(hPipeStdInRead);
        ::CloseHandle(hPipeStdInWrite);
    }
    if (outStdOut)
        *outStdOut = { hPipeStdOutRead, hPipeStdOutWrite };
    else
    {
        ::CloseHandle(hPipeStdOutRead);
        ::CloseHandle(hPipeStdOutWrite);
    }
    if (outStdErr)
        *outStdErr = { hPipeStdErrRead, hPipeStdErrWrite };
    else
    {
        ::CloseHandle(hPipeStdErrRead);
        ::CloseHandle(hPipeStdErrWrite);
    }
    return S_OK;
}

_Use_decl_annotations_
HRESULT WslWhich(PCWSTR pszDistribution, PCWSTR pszExecutable, DWORD dwTimeoutMillisec, PWSTR* pszResult)
{
    auto hr = _InitWsl();
    if (FAILED(hr))
        return hr;
    PWSTR psz;
    // use 'command' builtin command ('which' command may not be installed for some Linux system)
    hr = MakeFormattedString(&psz, L"sh -c \"command -v '%s'\"", pszExecutable);
    if (FAILED(hr))
        return hr;

    HANDLE hProcess;
    PipeData stdOut;
    hr = WslExecute(pszDistribution, psz, false, &hProcess, nullptr, &stdOut, nullptr);
    free(psz);
    if (FAILED(hr))
        return hr;
    hr = _WaitAndGetOutput(hProcess, stdOut.hRead, dwTimeoutMillisec, &psz);
    ::CloseHandle(hProcess);
    ::CloseHandle(stdOut.hRead);
    ::CloseHandle(stdOut.hWrite);
    if (FAILED(hr))
        return hr;

    if (!psz)
    {
        *pszResult = nullptr;
        return S_FALSE;
    }
    auto pCRChar = wcschr(psz, L'\r');
    auto pLFChar = wcschr(psz, L'\n');
    if (pCRChar)
        *pCRChar = 0;
    if (pLFChar)
        *pLFChar = 0;
    if (!*psz)
    {
        free(psz);
        *pszResult = nullptr;
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }
    if (*psz != L'/')
    {
        // may be a builtin command -- use it anyway (do nothing here)
    }
    *pszResult = psz;
    return S_OK;
}

_Use_decl_annotations_
HRESULT WslPath(PCWSTR pszDistribution, PCWSTR pszFile, DWORD dwTimeoutMillisec, PWSTR* pszResult)
{
    auto hr = _InitWsl();
    if (FAILED(hr))
        return hr;
    PWSTR psz;
    hr = MakeFormattedString(&psz, L"sh -c \"wslpath '%s'\"", pszFile);
    if (FAILED(hr))
        return hr;

    HANDLE hProcess;
    PipeData stdOut;
    hr = WslExecute(pszDistribution, psz, false, &hProcess, nullptr, &stdOut, nullptr);
    free(psz);
    if (FAILED(hr))
        return hr;
    hr = _WaitAndGetOutput(hProcess, stdOut.hRead, dwTimeoutMillisec, &psz);
    ::CloseHandle(hProcess);
    ::CloseHandle(stdOut.hRead);
    ::CloseHandle(stdOut.hWrite);
    if (FAILED(hr))
        return hr;

    if (!psz)
    {
        *pszResult = nullptr;
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
    auto pCRChar = wcschr(psz, L'\r');
    auto pLFChar = wcschr(psz, L'\n');
    if (pCRChar)
        *pCRChar = 0;
    if (pLFChar)
        *pLFChar = 0;
    if (!*psz || *psz != L'/')
    {
        free(psz);
        *pszResult = nullptr;
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
    *pszResult = psz;
    return S_OK;
}

_Use_decl_annotations_
HRESULT WslTestIfWritable(PCWSTR pszDistribution, PCWSTR pszWslFile, DWORD dwTimeoutMillisec)
{
    if (*pszWslFile != L'/')
        return E_INVALIDARG;
    auto pLastPath = wcsrchr(pszWslFile, L'/');
    if (!pLastPath)
        return E_UNEXPECTED;
    if (pLastPath == pszWslFile)
        ++pLastPath;
    auto dirLen = pLastPath - pszWslFile;
    auto pDir = static_cast<PWSTR>(malloc(sizeof(WCHAR) * (dirLen + 1)));
    if (!pDir)
        return E_OUTOFMEMORY;
    memcpy(pDir, pszWslFile, sizeof(WCHAR) * dirLen);
    pDir[dirLen] = 0;

    PWSTR pszCommand;
    // test \( ! -e '<pszWslFile>' -a -d '<pDir>' \) -o \( ! -d '<pszWslFile>' -a -w '<pszWslFile>' \)
    auto hr = MakeFormattedString(&pszCommand, L"sh -c \"test \\( ! -e '%s' -a -d '%s' \\) -o \\( ! -d '%s' -a -w '%s' \\) \"",
        pszWslFile, pDir, pszWslFile, pszWslFile);
    free(pDir);
    if (FAILED(hr))
        return hr;

    HANDLE hProcess;
    hr = WslExecute(pszDistribution, pszCommand, false, &hProcess, nullptr, nullptr, nullptr);
    free(pszCommand);
    if (FAILED(hr))
        return hr;

    auto r = ::WaitForSingleObject(hProcess, dwTimeoutMillisec);
    if (r == WAIT_FAILED || r == WAIT_TIMEOUT)
    {
        auto dw = r == WAIT_FAILED ? ::GetLastError() : ERROR_TIMEOUT;
        ::TerminateProcess(hProcess, static_cast<UINT>(-1));
        return HRESULT_FROM_WIN32(dw);
    }
    DWORD dwExitCode = 0;
    if (!::GetExitCodeProcess(hProcess, &dwExitCode))
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    if (dwExitCode == 0xFFFFFFFF)
    {
        // the error code '0xFFFFFFFF' (-1 for signed int) must be returned by Windows wsl.exe
        // (Linux command does not return negative exit code)
        return E_FAIL;
    }

    return dwExitCode == 0 ? S_OK :
        dwExitCode == 1 ? S_FALSE : E_FAIL;
}

#endif
