#include "../framework.h"
#include "../common.h"
#include "../util/functions.h"

#include "proxy.h"
#include "proxy_data.h"
#include "../logger/logger.h"
#include "../app/worker.h"

#include "../duplex/pipe_duplex.h"
#include "../duplex/syncfile_duplex.h"

class StdErrLogger : public Logger
{
public:
    StdErrLogger(HANDLE hStdErr)
        : m_hStdErr(hStdErr)
    {
#ifdef _DEBUG
        m_logLevel = LogLevel::Debug;
#else
        m_logLevel = LogLevel::Error;
#endif
    }

protected:
    virtual void OnLog(_In_ LogLevel level, _In_z_ PCWSTR pszLog)
    {
        if (level > m_logLevel)
            return;

        auto r = ::WideCharToMultiByte(CP_UTF8, 0, pszLog, -1, nullptr, 0, nullptr, nullptr);
        if (!r)
            return;
        auto p = static_cast<PSTR>(malloc(sizeof(CHAR) * r));
        ::WideCharToMultiByte(CP_UTF8, 0, pszLog, -1, p, r, nullptr, nullptr);
        ::WriteFile(m_hStdErr, p, static_cast<DWORD>(sizeof(CHAR)) * static_cast<DWORD>(r - 1), nullptr, nullptr);
    }

public:
    LogLevel m_logLevel;

private:
    HANDLE m_hStdErr;
};

_Use_decl_annotations_
int ProxyMain(const Option& options)
{
    //::MessageBoxW(nullptr, L"Do attach.", nullptr, MB_OK);
    StdErrLogger* myLogger = nullptr;

    {
        PCWSTR p = options.proxyPipeId;
        auto ret = false;
        // check if proxyPipeId is 'xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx'
        if (wcslen(p) == 36)
        {
            ret = true;
            for (int i = 0; i < 36; ++i)
            {
                if (i == 8 || i == 13 || i == 18 || i == 23)
                {
                    if (p[i] != L'-')
                    {
                        ret = false;
                        break;
                    }
                }
                else
                {
                    auto c = p[i];
                    if ((c < '0' || c > '9') && (c < 'A' || c > 'F') && (c < 'a' || c > 'f'))
                    {
                        ret = false;
                        break;
                    }
                }
            }
        }
        if (!ret)
            return 2;
    }

    auto hStdIn = ::GetStdHandle(STD_INPUT_HANDLE);
    auto hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
    auto hStdErr = ::GetStdHandle(STD_ERROR_HANDLE);
    if (hStdIn == nullptr)
        return 2;
    if (hStdOut == nullptr)
        hStdOut = INVALID_HANDLE_VALUE;

    PWSTR pszPipeName;
    auto hr = MakeFormattedString(&pszPipeName, PIPE_PROXY_NAME_FORMAT, options.proxyPipeId);
    if (FAILED(hr))
        return static_cast<int>(hr);
    auto hPipe = ::CreateFileW(pszPipeName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (hPipe == INVALID_HANDLE_VALUE)
    {
        auto err = ::GetLastError();
        free(pszPipeName);
        return static_cast<int>(HRESULT_FROM_WIN32(err));
    }
    free(pszPipeName);
    HANDLE hQuit = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hQuit)
    {
        auto err = ::GetLastError();
        ::CloseHandle(hPipe);
        return static_cast<int>(HRESULT_FROM_WIN32(err));
    }

    {
        ProxyData data;
        DWORD dw;
        OVERLAPPED ol = { 0 };
        ol.hEvent = hQuit;
        hr = ReadFileTimeout(hPipe, &data, sizeof(data), &dw, &ol, 3000);
        if (FAILED(hr))
        {
            ::CloseHandle(hQuit);
            ::CloseHandle(hPipe);
            return static_cast<int>(hr);
        }
        if (data.proxyVersion != PROXY_VERSION)
        {
            ::CloseHandle(hQuit);
            ::CloseHandle(hPipe);
            return static_cast<int>(E_ACCESSDENIED);
        }
        if (hStdErr != INVALID_HANDLE_VALUE)
        {
            myLogger = new StdErrLogger(hStdErr);
            myLogger->m_logLevel = static_cast<LogLevel>(data.logLevel);
            ::logger = myLogger;
        }
    }

    SyncFileDuplex pipeFrom(hStdIn, hStdOut, true);
    PipeDuplex pipeTo(hPipe, hPipe);

    ::ResetEvent(hQuit);
    hr = Transfer(hQuit, &pipeFrom, &pipeTo, AddLogFormatted);

    if (myLogger != nullptr)
    {
        ::logger = nullptr;
        delete myLogger;
    }

    ::CloseHandle(hQuit);

    return static_cast<int>(hr);
}
