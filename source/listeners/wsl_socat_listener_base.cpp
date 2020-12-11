#include "../framework.h"
#include "../common.h"
#include "../logger/logger.h"
#include "../util/functions.h"
#include "../util/wsl_util.h"
#include "../proxy/proxy_data.h"
#include "../app/app.h"

#include "wsl_socat_listener_base.h"

#ifdef _WIN64

static HRESULT MakePipeId(_Out_ PWSTR* outPipeName, _Out_ PWSTR* outPipeId)
{
    *outPipeName = nullptr;
    *outPipeId = nullptr;

    union
    {
        BYTE b[16];
        struct
        {
            DWORD t1;
            WORD t2;
            WORD t3;
            BYTE t4[8];
        };
    } randomId;
    GenerateRandom16Bytes(randomId.b);
    PWSTR pszPipeId;
    auto hr = MakeFormattedString(&pszPipeId, L"%08lX-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X",
        randomId.t1, randomId.t2, randomId.t3, static_cast<UINT>(randomId.t4[0]), static_cast<UINT>(randomId.t4[1]),
        static_cast<UINT>(randomId.t4[2]), static_cast<UINT>(randomId.t4[3]), static_cast<UINT>(randomId.t4[4]),
        static_cast<UINT>(randomId.t4[5]), static_cast<UINT>(randomId.t4[6]), static_cast<UINT>(randomId.t4[7]));
    if (FAILED(hr))
        return hr;

    PWSTR pszPipeName;
    hr = MakeFormattedString(&pszPipeName, PIPE_PROXY_NAME_FORMAT, pszPipeId);
    if (FAILED(hr))
    {
        free(pszPipeId);
        return hr;
    }

    *outPipeName = pszPipeName;
    *outPipeId = pszPipeId;
    return S_OK;
}

//_Use_decl_annotations_
WslSocatListenerBase::WslSocatListenerBase()
    : m_pszWslDistribution(nullptr)
    , m_hEventTemp(INVALID_HANDLE_VALUE)
    , m_dwWslPid(0)
{
}

_Use_decl_annotations_
HRESULT WslSocatListenerBase::InitializeBase(LPCWSTR pszDistributionName, LPCWSTR pszListen, PAcceptHandler pfnOnAccept, void* callbackData)
{
    if (m_hPipeCurrent != INVALID_HANDLE_VALUE)
        return S_OK;
    auto pszDistributionNameDup = _wcsdup(pszDistributionName);
    if (!pszDistributionNameDup)
        return E_OUTOFMEMORY;
    PWSTR pszCurrentProcessWslFileName;
    auto hr = WslPath(pszDistributionName, GetCurrentProcessModuleName(), 10000, &pszCurrentProcessWslFileName);
    if (FAILED(hr))
    {
        free(pszDistributionNameDup);
        return hr;
    }
    PWSTR pszSocatFileName;
    hr = WslWhich(pszDistributionName, L"socat", 10000, &pszSocatFileName);
    if (FAILED(hr))
    {
        free(pszCurrentProcessWslFileName);
        free(pszDistributionNameDup);
        return hr;
    }

    PWSTR pszPipeName;
    PWSTR pszPipeId;
    hr = MakePipeId(&pszPipeName, &pszPipeId);
    if (FAILED(hr))
    {
        free(pszCurrentProcessWslFileName);
        free(pszDistributionNameDup);
        return hr;
    }

    PWSTR pszCommand;
    // execute wsl -d <distro> -e sh -c "('<socat>' \"unix-listen:'<sock-file>',fork\" \"exec:'\\\"<stream-connector.exe>\\\" -x <pipe-id>',nofork\" & echo $!; wait $!)"
    hr = MakeFormattedString(&pszCommand, L"sh -c \"('%s' \\\"%s\\\" \\\"exec:'\\\\\\\"%s\\\\\\\" -x %s',nofork\\\" & echo $!; wait $!)\"",
        pszSocatFileName, pszListen, pszCurrentProcessWslFileName, pszPipeId);
    free(pszSocatFileName);
    free(pszCurrentProcessWslFileName);
    free(pszPipeId);
    if (FAILED(hr))
    {
        free(pszPipeName);
        free(pszDistributionNameDup);
        return hr;
    }

    auto hTemp = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hTemp)
    {
        auto hr = HRESULT_FROM_WIN32(::GetLastError());
        free(pszCommand);
        free(pszPipeName);
        free(pszDistributionNameDup);
        return hr;
    }

    hr = NamedPipeListener::Initialize(pszPipeName, pfnOnAccept, callbackData);
    free(pszPipeName);
    if (FAILED(hr))
    {
        ::CloseHandle(hTemp);
        free(pszCommand);
        free(pszDistributionNameDup);
        return hr;
    }

    m_hEventTemp = hTemp;
    m_pszWslDistribution = pszDistributionNameDup;

    hr = m_process.StartProcess(pszDistributionNameDup, pszCommand, this,
        _ExitHandler, nullptr, _StdErrHandler);
    free(pszCommand);
    if (FAILED(hr))
    {
        Close();
        return hr;
    }
    PWSTR p;
    hr = m_process.ReadLineFromStdOut(&p, 5000);
    if (FAILED(hr))
    {
        Close();
        return hr;
    }
    m_dwWslPid = static_cast<DWORD>(_wtol(p));
    free(p);

    return S_OK;
}

//_Use_decl_annotations_
void WslSocatListenerBase::Close()
{
    NamedPipeListener::Close();
    if (!m_strStdErrChunk.empty())
    {
        AddLogFormatted(LogLevel::Error, L"[wsl-socat] %s", m_strStdErrChunk.c_str());
        m_strStdErrChunk.clear();
    }
    if (m_dwWslPid != 0)
    {
        PWSTR p;
        if (SUCCEEDED(::MakeFormattedString(&p, L"sh -c \"kill -INT %lu\"", m_dwWslPid)))
        {
            HANDLE h;
            if (SUCCEEDED(::WslExecute(m_pszWslDistribution, p, false, &h, nullptr, nullptr, nullptr)))
            {
                if (::WaitForSingleObject(h, 3000) != WAIT_OBJECT_0)
                {
                    ::TerminateProcess(h, static_cast<UINT>(-1));
                }
                ::CloseHandle(h);
            }
            free(p);
        }
        m_dwWslPid = 0;
    }
    m_process.Close();
    if (m_hEventTemp != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEventTemp);
        m_hEventTemp = INVALID_HANDLE_VALUE;
    }
    OnCleanup(m_pszWslDistribution);
    if (m_pszWslDistribution)
    {
        free(m_pszWslDistribution);
        m_pszWslDistribution = nullptr;
    }
}

_Use_decl_annotations_
HRESULT WslSocatListenerBase::CheckConnectedPipe(_In_ HANDLE hPipe)
{
    // check client PID
    // (connection from WSL must be from copy of this program)
    DWORD processId = 0;
    if (!::GetNamedPipeClientProcessId(hPipe, &processId))
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    if (!IsSameProcessToCurrent(processId))
        return E_ACCESSDENIED;

    ProxyData data = { 0 };
    data.proxyVersion = PROXY_VERSION;
    data.logLevel = static_cast<BYTE>(GetLogLevel());

    OVERLAPPED ol = { 0 };
    ol.hEvent = m_hEventTemp;
    ::ResetEvent(m_hEventTemp);
    auto hr = WriteFileTimeout(hPipe, &data, sizeof(data), nullptr, &ol, 3000);
    if (FAILED(hr))
        return hr;
    ::FlushFileBuffers(hPipe);

    // all done
    return S_OK;
}

_Use_decl_annotations_
void CALLBACK WslSocatListenerBase::_ExitHandler(void* data, DWORD dwExitCode)
{
    auto pThis = static_cast<WslSocatListenerBase*>(data);
    pThis->Close();
}

_Use_decl_annotations_
HRESULT CALLBACK WslSocatListenerBase::_StdErrHandler(void* data, const void* receivedData, DWORD size)
{
    auto pThis = static_cast<WslSocatListenerBase*>(data);
    auto r = ::MultiByteToWideChar(CP_UTF8, 0, static_cast<PCSTR>(receivedData), static_cast<int>(size), nullptr, 0);
    if (r < 0)
        return E_UNEXPECTED;
    std::wstring str;
    str.resize(r);
    ::MultiByteToWideChar(CP_UTF8, 0, static_cast<PCSTR>(receivedData), static_cast<int>(size), &str.at(0), r);
    ReplaceReturnChars(str);
    str = pThis->m_strStdErrChunk + str;
    pThis->m_strStdErrChunk.clear();
    PWSTR p = &str.at(0);
    while (p && *p)
    {
        auto pNext = wcschr(p, L'\r');
        if (pNext)
        {
            *pNext++ = 0;
            if (*pNext == L'\n')
                ++pNext;
        }
        else
        {
            pThis->m_strStdErrChunk = p;
            break;
        }
        AddLogFormatted(LogLevel::Error, L"[wsl-socat] %s", p);
        p = pNext;
    }
    return S_OK;
}

#endif
