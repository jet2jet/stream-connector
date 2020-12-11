#include "framework.h"
#include "options.h"

#include "app/simple_dialog.h"
#include "util/functions.h"

#include <stdarg.h>

static void ShowHelp(_In_opt_z_ PCWSTR errorReason = nullptr)
{
    constexpr auto helpMessage =
        L"Usage: stream-connector.exe <options>\n"
        L"\n"
        L"<options>:\n"
        L"  -h, -?, --help : Show this help\n"
        L"  -l <listener>, --listener <listener> : [Required] Add listener (can be specified more than one)\n"
        L"  -c <connector>, --connector <connector> : [Required] Set connector\n"
        L"  -n <name>, --name <name> : User-defined name\n"
        L"  --log <level> : Set log level\n"
        L"    <level>: error, info, debug (default: error)\n"
        L"\n"
        L"<listener>:\n"
        L"  tcp-socket [-4 | -6] [<address>:]<port> : TCP socket listener (port num. can be 0 for auto-assign)\n"
        L"    alias for 'tcp-socket': s, sock, socket, tcp\n"
        L"  unix-socket [--abstract] <file-path> : Unix socket listener (listener with the socket file)\n"
        L"    alias for 'unix-socket': u, unix\n"
        L"  cygwin-sockfile <win-file-path> : Listener with Cygwin-spec socket file\n"
        L"    alias for 'cygwin-sockfile': c\n"
        L"  pipe <pipe-name> : Named-pipe listener\n"
        L"    alias for 'pipe': p\n"
        L"  wsl-tcp-socket [-d <distribution>] [-4 | -6] [<address>:]<port> : TCP socket listener in WSL (port num. can be 0 for auto-assign)\n"
        L"    alias for 'wsl-tcp-socket': ws, wt\n"
        L"  wsl-unix-socket [-d <distribution>] <wsl-file-path> : Unix socket listener in WSL (listener with the socket file in WSL)\n"
        L"    alias for 'wsl-unix-socket': wu\n"
        L"\n"
        L"<connector>:\n"
        L"  tcp-socket <address>:<port> : TCP socket connector (port num. cannot be 0)\n"
        L"  unix-socket [--abstract] <file-name> : Unix socket connector\n"
        L"  pipe <pipe-name> : Named-pipe connector\n"
        L"  wsl-tcp-socket [-d <distribution>] <address>:<port> : TCP socket connector in WSL (port num. cannot be 0)\n"
        L"  wsl-unix-socket [-d <distribution>] [--abstract] <wsl-file-path> : Unix socket connector in WSL\n"
        ;
    PWSTR pszMessage = nullptr;
    if (errorReason)
        MakeFormattedString(&pszMessage, L"Error: %s\n\n%s", errorReason, helpMessage);
    else
        pszMessage = _wcsdup(helpMessage);
    if (pszMessage)
    {
        //::MessageBoxW(nullptr, pszMessage, nullptr, MB_ICONEXCLAMATION);
        ShowUsageDialog(pszMessage, nullptr);
    }
}

static HRESULT CheckAndPrepareFilePath(_Inout_z_ PWSTR pszPath, _Outptr_result_maybenull_z_ PWSTR* outErrorReason)
{
    *outErrorReason = nullptr;
    // replace '/' to '\'
    {
        auto ptr = pszPath;
        while (*ptr)
        {
            if (*ptr == L'/')
                *ptr = L'\\';
            ++ptr;
        }
    }
    // check if directory is available
    auto pszLastPathDelimiter = wcsrchr(pszPath, L'\\');
    if (pszLastPathDelimiter)
    {
        if (pszLastPathDelimiter == pszPath + 2 && pszPath[1] == L':')
        {
            // pszPath seems to be on the root directory -- it's ok
        }
        else if (pszLastPathDelimiter[1] == L'\0')
        {
            MakeFormattedString(outErrorReason, L"The path must not end with delimiter: '%s'", pszPath);
            return E_INVALIDARG;
        }
        else
        {
            // set null-char to make pszPath a directory name
            *pszLastPathDelimiter = L'\0';
            auto attr = ::GetFileAttributesW(pszPath);
            if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
            {
                MakeFormattedString(outErrorReason, L"The directory must exist: '%s'", pszPath);
                return E_INVALIDARG;
            }
            // restore
            *pszLastPathDelimiter = L'\\';
        }
    }
    auto curAttr = ::GetFileAttributesW(pszPath);
    if (curAttr != INVALID_FILE_ATTRIBUTES && (
        (curAttr & FILE_ATTRIBUTE_READONLY) ||
        (curAttr & FILE_ATTRIBUTE_DEVICE) ||
        (curAttr & FILE_ATTRIBUTE_DIRECTORY)
        ))
    {
        MakeFormattedString(outErrorReason, L"The file seems to be unwritable: '%s' (attr: 0x%08lX)", pszPath, curAttr);
        return E_INVALIDARG;
    }
    return S_OK;
}

static HRESULT _ParseWslDistributionParam(
    _When_(SUCCEEDED(return), _Out_) PWSTR* outDistribution,
    _When_(SUCCEEDED(return), _Out_) PCWSTR* outRestArg,
    _In_ wchar_t** restArgs,
    _In_ int argc,
    _Out_ int* outArgReadCount
)
{
    *outArgReadCount = 0;

    int c = 0;
    if (argc < 1)
        return E_INVALIDARG;
    PWSTR pszDistribution = nullptr;
    auto rest = restArgs[0];
    ++c;
    if (wcscmp(rest, L"-d") == 0 || wcscmp(rest, L"/d") == 0 ||
        wcscmp(rest, L"--distribution") == 0 || wcscmp(rest, L"/distribution") == 0)
    {
        if (argc < 3)
            return E_INVALIDARG;
        pszDistribution = _wcsdup(restArgs[1]);
        if (!pszDistribution)
            return E_OUTOFMEMORY;
        rest = restArgs[2];
        c += 2;
    }
    *outDistribution = pszDistribution;
    *outRestArg = rest;
    *outArgReadCount = c;
    return S_OK;
}

static HRESULT AddListener(
    _Inout_ Option* options,
    _In_z_ PCWSTR pszArg1,
    _In_reads_(argc) wchar_t** restArgs,
    _In_ int argc,
    _Out_ int* outArgReadCount,
    _Outptr_result_maybenull_z_ PWSTR* outErrorReason
)
{
    *outErrorReason = nullptr;
    *outArgReadCount = 0;

    if (!options->listeners)
    {
        auto listeners = new std::vector<ListenerData*>();
        if (!listeners)
            return E_OUTOFMEMORY;
        options->listeners = listeners;
    }

    int c = 1;
    if (wcscmp(pszArg1, L"s") == 0 || wcscmp(pszArg1, L"sock") == 0 || wcscmp(pszArg1, L"socket") == 0 ||
        wcscmp(pszArg1, L"tcp") == 0 || wcscmp(pszArg1, L"tcp-socket") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        bool isIPv6 = false;
        auto pszArg2 = restArgs[0];
        ++c;
        if (wcscmp(pszArg2, L"-4") == 0 || wcscmp(pszArg2, L"/4") == 0 ||
            wcscmp(pszArg2, L"-6") == 0 || wcscmp(pszArg2, L"/6") == 0)
        {
            if (argc <= 1)
                return E_INVALIDARG;
            isIPv6 = pszArg2[1] == L'6';
            pszArg2 = restArgs[1];
            ++c;
        }
        else
        {
            isIPv6 = (pszArg2[0] == L'[');
        }
        long portNum = -1;
        auto pszPortStr = wcsrchr(pszArg2, L':');
        if (!pszPortStr)
        {
            pszPortStr = pszArg2;
            pszArg2 = nullptr;
        }
        else
        {
            if (pszPortStr == pszArg2)
                pszArg2 = nullptr;
            ++pszPortStr;
        }
        {
            PWSTR ptr = nullptr;
            portNum = wcstol(pszPortStr, &ptr, 10);
            if (!ptr || *ptr != L'\0')
                portNum = -1;
            else if (portNum < 0 || portNum > 65535)
                portNum = -1;
        }
        if (portNum < 0)
        {
            MakeFormattedString(outErrorReason, L"Address format must be '<port>' or '<address>:<port>' (actual: %s)", pszArg2);
            return E_INVALIDARG;
        }
        _Analysis_assume_(pszPortStr != nullptr);
        _Analysis_assume_(portNum >= 0 && portNum <= 65535);

        PWSTR pszAddress = nullptr;
        if (pszArg2)
        {
            pszAddress = _wcsdup(pszArg2);
            if (!pszAddress)
                return E_OUTOFMEMORY;
            pszAddress[(pszPortStr - 1) - pszArg2] = L'\0';
        }
        auto d = static_cast<TcpSocketListenerData*>(malloc(sizeof(TcpSocketListenerData)));
        if (!d)
        {
            free(pszAddress);
            return E_OUTOFMEMORY;
        }
        d->type = ListenerType::TcpSocket;
        d->pszAddress = pszAddress;
        d->isIPv6 = isIPv6;
        d->port = static_cast<WORD>(portNum);
        options->listeners->push_back(d);
        d->id = static_cast<WORD>(options->listeners->size());
    }
    else if (wcscmp(pszArg1, L"u") == 0 || wcscmp(pszArg1, L"unix") == 0 || wcscmp(pszArg1, L"unix-socket") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        auto pszArg2 = restArgs[0];
        ++c;
        bool isAbstract = false;
        if (wcscmp(pszArg2, L"--abstract") == 0 || wcscmp(pszArg2, L"-a") == 0 ||
            wcscmp(pszArg2, L"/abstract") == 0 || wcscmp(pszArg2, L"/a") == 0)
        {
            if (argc <= 1)
                return E_INVALIDARG;
            ++c;
            isAbstract = true;
            pszArg2 = restArgs[1];
        }
        auto pszPath = _wcsdup(pszArg2);
        if (!pszPath)
            return E_OUTOFMEMORY;
        auto hr = CheckAndPrepareFilePath(pszPath, outErrorReason);
        if (FAILED(hr))
        {
            free(pszPath);
            return hr;
        }

        auto d = static_cast<UnixSocketListenerData*>(malloc(sizeof(UnixSocketListenerData)));
        if (!d)
        {
            free(pszPath);
            return E_OUTOFMEMORY;
        }
        d->type = ListenerType::UnixSocket;
        d->pszPath = pszPath;
        d->isAbstract = isAbstract;
        options->listeners->push_back(d);
        d->id = static_cast<WORD>(options->listeners->size());
    }
    else if (wcscmp(pszArg1, L"c") == 0 || wcscmp(pszArg1, L"cygwin-sockfile") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        auto pszPath = _wcsdup(restArgs[0]);
        ++c;
        if (!pszPath)
            return E_OUTOFMEMORY;
        auto hr = CheckAndPrepareFilePath(pszPath, outErrorReason);
        if (FAILED(hr))
        {
            free(pszPath);
            return hr;
        }

        auto d = static_cast<CygwinSockFileListenerData*>(malloc(sizeof(CygwinSockFileListenerData)));
        if (!d)
        {
            free(pszPath);
            return E_OUTOFMEMORY;
        }
        d->type = ListenerType::CygwinSockFile;
        d->pszCygwinPath = pszPath;
        options->listeners->push_back(d);
        d->id = static_cast<WORD>(options->listeners->size());
    }
    else if (wcscmp(pszArg1, L"p") == 0 || wcscmp(pszArg1, L"pipe") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        auto pszPath = _wcsdup(restArgs[0]);
        ++c;
        if (!pszPath)
            return E_OUTOFMEMORY;
        // replace '/' to '\'
        {
            auto ptr = pszPath;
            while (*ptr)
            {
                if (*ptr == L'/')
                    *ptr = L'\\';
                ++ptr;
            }
        }
        if (wcsncmp(pszPath, L"\\\\.\\pipe\\", 9) != 0)
        {
            MakeFormattedString(outErrorReason, L"The path must start with \\\\.\\pipe\\: '%s'", pszPath);
            free(pszPath);
            return E_INVALIDARG;
        }

        auto d = static_cast<PipeListenerData*>(malloc(sizeof(PipeListenerData)));
        if (!d)
        {
            free(pszPath);
            return E_OUTOFMEMORY;
        }
        d->type = ListenerType::Pipe;
        d->pszPipeName = pszPath;
        options->listeners->push_back(d);
        d->id = static_cast<WORD>(options->listeners->size());
    }
    else if (wcscmp(pszArg1, L"ws") == 0 || wcscmp(pszArg1, L"wt") == 0 || wcscmp(pszArg1, L"wsl-tcp-socket") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        PWSTR pszDistribution = nullptr;
        PCWSTR pszArg2 = nullptr;
        bool isIPv6 = false;
        int x = 0;
        auto hr = _ParseWslDistributionParam(&pszDistribution, &pszArg2, restArgs, argc, &x);
        if (FAILED(hr))
            return hr;
        c += x;
        if (wcscmp(pszArg2, L"-4") == 0 || wcscmp(pszArg2, L"/4") == 0 ||
            wcscmp(pszArg2, L"-6") == 0 || wcscmp(pszArg2, L"/6") == 0)
        {
            if (argc <= c)
                return E_INVALIDARG;
            isIPv6 = pszArg2[1] == L'6';
            pszArg2 = restArgs[c];
            ++c;
        }
        else
        {
            isIPv6 = (pszArg2[0] == L'[');
        }
        long portNum = -1;
        auto pszPortStr = wcsrchr(pszArg2, L':');
        if (!pszPortStr)
        {
            pszPortStr = pszArg2;
            pszArg2 = nullptr;
        }
        else
        {
            if (pszPortStr == pszArg2)
                pszArg2 = nullptr;
            ++pszPortStr;
        }
        {
            PWSTR ptr = nullptr;
            portNum = wcstol(pszPortStr, &ptr, 10);
            if (!ptr || *ptr != L'\0')
                portNum = -1;
            else if (portNum < 0 || portNum > 65535)
                portNum = -1;
        }
        if (portNum < 0)
        {
            if (pszDistribution)
                free(pszDistribution);
            MakeFormattedString(outErrorReason, L"Address format must be '<port>' or '<address>:<port>' (actual: %s)", pszArg2);
            return E_INVALIDARG;
        }
        _Analysis_assume_(pszPortStr != nullptr);
        _Analysis_assume_(portNum >= 0 && portNum <= 65535);

        PWSTR pszAddress = nullptr;
        if (pszArg2)
        {
            pszAddress = _wcsdup(pszArg2);
            if (!pszAddress)
            {
                if (pszDistribution)
                    free(pszDistribution);
                return E_OUTOFMEMORY;
            }
            pszAddress[(pszPortStr - 1) - pszArg2] = L'\0';
        }
        auto d = static_cast<WslTcpSocketListenerData*>(malloc(sizeof(WslTcpSocketListenerData)));
        if (!d)
        {
            free(pszAddress);
            if (pszDistribution)
                free(pszDistribution);
            return E_OUTOFMEMORY;
        }
        d->type = ListenerType::WslTcpSocket;
        d->pszDistribution = pszDistribution;
        d->pszAddress = pszAddress;
        d->isIPv6 = isIPv6;
        d->port = static_cast<WORD>(portNum);
        options->listeners->push_back(d);
        d->id = static_cast<WORD>(options->listeners->size());
    }
    else if (wcscmp(pszArg1, L"wu") == 0 || wcscmp(pszArg1, L"wsl-unix-socket") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        PWSTR pszDistribution = nullptr;
        PCWSTR pszArg2 = nullptr;
        int x = 0;
        auto hr = _ParseWslDistributionParam(&pszDistribution, &pszArg2, restArgs, argc, &x);
        if (FAILED(hr))
            return hr;
        c += x;
        bool isAbstract = false;
        if (wcscmp(pszArg2, L"--abstract") == 0 || wcscmp(pszArg2, L"-a") == 0 ||
            wcscmp(pszArg2, L"/abstract") == 0 || wcscmp(pszArg2, L"/a") == 0)
        {
            if (argc < c + 1)
                return E_INVALIDARG;
            isAbstract = true;
            pszArg2 = restArgs[c - 1];
            ++c;
        }
        auto pszPath = _wcsdup(pszArg2);
        if (!pszPath)
            return E_OUTOFMEMORY;

        auto d = static_cast<WslUnixSocketListenerData*>(malloc(sizeof(WslUnixSocketListenerData)));
        if (!d)
        {
            free(pszPath);
            if (pszDistribution)
                free(pszDistribution);
            return E_OUTOFMEMORY;
        }
        d->type = ListenerType::WslUnixSocket;
        d->pszDistribution = pszDistribution;
        d->pszWslPath = pszPath;
        d->isAbstract = isAbstract;
        options->listeners->push_back(d);
        d->id = static_cast<WORD>(options->listeners->size());
    }
    else
    {
        return E_INVALIDARG;
    }

    *outArgReadCount = c;
    return S_OK;
}

static HRESULT ParseConnector(
    _Inout_ Option* options,
    _In_z_ PCWSTR pszArg1,
    _In_reads_(argc) wchar_t** restArgs,
    _In_ int argc,
    _Out_ int* outArgReadCount,
    _Outptr_result_maybenull_z_ PWSTR* outErrorReason
)
{
    *outErrorReason = nullptr;

    if (options->connector)
    {
        *outErrorReason = _wcsdup(L"The connector can be specified only once.");
        return E_INVALIDARG;
    }

    int c = 1;
    if (wcscmp(pszArg1, L"s") == 0 || wcscmp(pszArg1, L"sock") == 0 || wcscmp(pszArg1, L"socket") == 0 ||
        wcscmp(pszArg1, L"tcp") == 0 || wcscmp(pszArg1, L"tcp-socket") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        auto pszArg2 = restArgs[0];
        long portNum = 0;
        auto pszPortStr = wcsrchr(pszArg2, L':');
        if (pszPortStr)
        {
            ++pszPortStr;
            PWSTR ptr = nullptr;
            portNum = wcstol(pszPortStr, &ptr, 10);
            if (!ptr || *ptr != L'\0')
                portNum = 0;
            else if (portNum <= 0 || portNum > 65535)
                portNum = 0;
        }
        if (!portNum)
        {
            MakeFormattedString(outErrorReason, L"Address format must be '<address>:<port>' (actual: %s)", pszArg2);
            return E_INVALIDARG;
        }
        _Analysis_assume_(pszPortStr != nullptr);
        _Analysis_assume_(portNum > 0 && portNum <= 65535);

        auto pszAddress = _wcsdup(pszArg2);
        if (!pszAddress)
            return E_OUTOFMEMORY;
        pszAddress[(pszPortStr - 1) - pszArg2] = L'\0';
        auto d = static_cast<TcpSocketConnectorData*>(malloc(sizeof(TcpSocketConnectorData)));
        if (!d)
        {
            free(pszAddress);
            return E_OUTOFMEMORY;
        }
        d->type = ConnectorType::TcpSocket;
        d->pszAddress = pszAddress;
        d->port = static_cast<WORD>(portNum);
        options->connector = d;
    }
    else if (wcscmp(pszArg1, L"u") == 0 || wcscmp(pszArg1, L"unix") == 0 || wcscmp(pszArg1, L"unix-socket") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        auto pszFileArg = restArgs[0];
        bool isAbstract = false;
        if (wcscmp(pszFileArg, L"--abstract") == 0 || wcscmp(pszFileArg, L"-a") == 0 ||
            wcscmp(pszFileArg, L"/abstract") == 0 || wcscmp(pszFileArg, L"/a") == 0)
        {
            if (argc < 2)
                return E_INVALIDARG;
            ++c;
            isAbstract = true;
            pszFileArg = restArgs[1];
        }
        auto pszPath = _wcsdup(pszFileArg);
        if (!pszPath)
            return E_OUTOFMEMORY;
        // replace '/' to '\'
        {
            auto ptr = pszPath;
            while (*ptr)
            {
                if (*ptr == L'/')
                    *ptr = L'\\';
                ++ptr;
            }
        }
        auto d = static_cast<UnixSocketConnectorData*>(malloc(sizeof(UnixSocketConnectorData)));
        if (!d)
        {
            free(pszPath);
            return E_OUTOFMEMORY;
        }
        d->type = ConnectorType::UnixSocket;
        d->pszFileName = pszPath;
        d->isAbstract = isAbstract;
        options->connector = d;
    }
    else if (wcscmp(pszArg1, L"p") == 0 || wcscmp(pszArg1, L"pipe") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        auto pszPath = _wcsdup(restArgs[0]);
        if (!pszPath)
            return E_OUTOFMEMORY;
        // replace '/' to '\'
        {
            auto ptr = pszPath;
            while (*ptr)
            {
                if (*ptr == L'/')
                    *ptr = L'\\';
                ++ptr;
            }
        }
        if (wcsncmp(pszPath, L"\\\\.\\pipe\\", 9) != 0)
        {
            MakeFormattedString(outErrorReason, L"The path must start with \\\\.\\pipe\\: '%s'", pszPath);
            free(pszPath);
            return E_INVALIDARG;
        }

        auto d = static_cast<PipeConnectorData*>(malloc(sizeof(PipeConnectorData)));
        if (!d)
        {
            free(pszPath);
            return E_OUTOFMEMORY;
        }
        d->type = ConnectorType::Pipe;
        d->pszPipeName = pszPath;
        options->connector = d;
    }
    else if (wcscmp(pszArg1, L"ws") == 0 || wcscmp(pszArg1, L"wt") == 0 || wcscmp(pszArg1, L"wsl-tcp-socket") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        PWSTR pszDistribution = nullptr;
        PCWSTR pszArg2 = nullptr;
        int x = 0;
        auto hr = _ParseWslDistributionParam(&pszDistribution, &pszArg2, restArgs, argc, &x);
        if (FAILED(hr))
            return hr;
        c += x;
        long portNum = 0;
        auto pszPortStr = wcsrchr(pszArg2, L':');
        if (pszPortStr)
        {
            ++pszPortStr;
            PWSTR ptr = nullptr;
            portNum = wcstol(pszPortStr, &ptr, 10);
            if (!ptr || *ptr != L'\0')
                portNum = 0;
            else if (portNum < 0 || portNum > 65535)
                portNum = 0;
        }
        if (!portNum)
        {
            if (pszDistribution)
                free(pszDistribution);
            MakeFormattedString(outErrorReason, L"Address format must be '<address>:<port>' (actual: %s)", pszArg2);
            return E_INVALIDARG;
        }
        _Analysis_assume_(pszPortStr != nullptr);
        _Analysis_assume_(portNum > 0 && portNum <= 65535);

        auto pszAddress = _wcsdup(pszArg2);
        if (!pszAddress)
        {
            if (pszDistribution)
                free(pszDistribution);
            return E_OUTOFMEMORY;
        }
        pszAddress[(pszPortStr - 1) - pszArg2] = L'\0';
        auto d = static_cast<WslTcpSocketConnectorData*>(malloc(sizeof(WslTcpSocketConnectorData)));
        if (!d)
        {
            free(pszAddress);
            if (pszDistribution)
                free(pszDistribution);
            return E_OUTOFMEMORY;
        }
        d->type = ConnectorType::WslTcpSocket;
        d->pszDistribution = pszDistribution;
        d->pszAddress = pszAddress;
        d->port = static_cast<WORD>(portNum);
        options->connector = d;
    }
    else if (wcscmp(pszArg1, L"wu") == 0 || wcscmp(pszArg1, L"wsl-unix-socket") == 0)
    {
        if (argc < 1)
            return E_INVALIDARG;
        PWSTR pszDistribution = nullptr;
        PCWSTR pszFileArg = nullptr;
        int x = 0;
        auto hr = _ParseWslDistributionParam(&pszDistribution, &pszFileArg, restArgs, argc, &x);
        if (FAILED(hr))
            return hr;
        c += x;
        bool isAbstract = false;
        if (wcscmp(pszFileArg, L"--abstract") == 0 || wcscmp(pszFileArg, L"-a") == 0 ||
            wcscmp(pszFileArg, L"/abstract") == 0 || wcscmp(pszFileArg, L"/a") == 0)
        {
            if (argc < c + 1)
                return E_INVALIDARG;
            isAbstract = true;
            pszFileArg = restArgs[c - 1];
            ++c;
        }
        auto pszPath = _wcsdup(pszFileArg);
        if (!pszPath)
        {
            if (pszDistribution)
                free(pszDistribution);
            return E_OUTOFMEMORY;
        }
        // replace '/' to '\'
        {
            auto ptr = pszPath;
            while (*ptr)
            {
                if (*ptr == L'/')
                    *ptr = L'\\';
                ++ptr;
            }
        }
        auto d = static_cast<WslUnixSocketConnectorData*>(malloc(sizeof(WslUnixSocketConnectorData)));
        if (!d)
        {
            free(pszPath);
            if (pszDistribution)
                free(pszDistribution);
            return E_OUTOFMEMORY;
        }
        d->type = ConnectorType::WslUnixSocket;
        d->pszDistribution = pszDistribution;
        d->pszFileName = pszPath;
        d->isAbstract = isAbstract;
        options->connector = d;
    }
    else
    {
        return E_INVALIDARG;
    }
    *outArgReadCount = c;

    return S_OK;
}

_Use_decl_annotations_
HRESULT ParseOptions(Option* outOptions)
{
    HRESULT hr = S_OK;
    PWSTR errorReason = nullptr;
    ZeroMemory(outOptions, sizeof(Option));
    outOptions->logLevel = LogLevel::Error;
    for (int i = 1; i < __argc;)
    {
        auto arg = __wargv[i];
        ++i;
        auto isSingleCharOption = false;
        auto isMultipleCharOption = false;
        if (*arg == L'-' || *arg == L'/')
        {
            if (*arg == L'/')
            {
                ++arg;
                isSingleCharOption = true;
                isMultipleCharOption = true;
            }
            else if (*arg == L'-' && arg[1] == L'-')
            {
                arg += 2;
                isMultipleCharOption = true;
            }
            else
            {
                ++arg;
                isSingleCharOption = true;
            }

            if (
                (isSingleCharOption && (wcscmp(arg, L"h") == 0 || wcscmp(arg, L"?") == 0)) ||
                (isMultipleCharOption && wcscmp(arg, L"help") == 0)
            )
            {
                hr = S_FALSE;
                break;
            }
            else if (
                (isSingleCharOption && wcscmp(arg, L"x") == 0) ||
                (isMultipleCharOption && wcscmp(arg, L"proxy") == 0)
            )
            {
                if (i >= __argc)
                {
                    hr = E_INVALIDARG;
                    break;
                }
                else
                {
                    auto p = _wcsdup(__wargv[i]);
                    outOptions->proxyPipeId = p;
                }
            }
            else if (
                (isSingleCharOption && wcscmp(arg, L"n") == 0) ||
                (isMultipleCharOption && wcscmp(arg, L"name") == 0)
                )
            {
                if (i >= __argc)
                {
                    hr = E_INVALIDARG;
                    break;
                }
                else
                {
                    auto nameParam = __wargv[i];
                    auto len = wcslen(nameParam);
                    if (len >= 64)
                        len = 63;
                    auto p = static_cast<PWSTR>(malloc(sizeof(WCHAR) * (len + 1)));
                    if (!p)
                    {
                        hr = E_OUTOFMEMORY;
                        break;
                    }
                    memcpy(p, nameParam, sizeof(WCHAR) * len);
                    p[len] = 0;
                    outOptions->pszName = p;
                }
            }
            else if (
                (isSingleCharOption && wcscmp(arg, L"l") == 0) ||
                (isMultipleCharOption && wcscmp(arg, L"listener") == 0)
            )
            {
                std::remove_reference<decltype(__wargv[0])>::type arg1 = nullptr;
                if (i >= __argc)
                {
                    hr = E_INVALIDARG;
                    break;
                }
                else
                {
                    arg1 = __wargv[i];
                    int c = 0;
                    hr = AddListener(outOptions, arg1, &__wargv[i + 1], __argc - (i + 1), &c, &errorReason);
                    i += c;
                }
                if (FAILED(hr))
                {
                    if (hr == E_INVALIDARG && !errorReason)
                    {
                        MakeFormattedString(
                            &errorReason,
                            L"The listener must be either of followings (actual: '%s'):\n"
                            L"  tcp-socket [<ipv4-address>:]<port>\n"
                            L"  unix-socket <file-name>\n"
                            L"  cygwin-sockfile <win-file-path>\n"
                            L"  pipe <pipe-name>\n"
                            L"  wsl-tcp-socket [<ipv4-address>:]<port>\n"
                            L"  wsl-unix-socket <wsl-path-name>",
                            arg1 ? arg1 : L"(null)"
                        );
                    }
                    break;
                }
            }
            else if (
                (isSingleCharOption && wcscmp(arg, L"c") == 0) ||
                (isMultipleCharOption && wcscmp(arg, L"connector") == 0)
            )
            {
                std::remove_reference<decltype(__wargv[0])>::type arg1 = nullptr;
                if (i >= __argc)
                {
                    hr = E_INVALIDARG;
                    break;
                }
                else
                {
                    arg1 = __wargv[i];
                    int c = 0;
                    hr = ParseConnector(outOptions, arg1, &__wargv[i + 1], __argc - (i + 1), &c, &errorReason);
                    i += c;
                }
                if (FAILED(hr))
                {
                    if (hr == E_INVALIDARG && !errorReason)
                    {
                        MakeFormattedString(
                            &errorReason,
                            L"The connector must be either of followings (actual: '%s'):\n"
                            L"  tcp-socket <ipv4-address>:<port>\n"
                            L"  unix-socket <file-name>\n"
                            L"  pipe <pipe-name>\n"
                            L"  wsl-tcp-socket <address>:<port>\n"
                            L"  wsl-unix-socket <wsl-path-name>",
                            arg1 ? arg1 : L"(null)"
                        );
                    }
                }
            }
            else if (
                (isMultipleCharOption && wcscmp(arg, L"log") == 0)
            )
            {
                if (i >= __argc)
                {
                    hr = E_INVALIDARG;
                    MakeFormattedString(
                        &errorReason,
                        L"Log level value is missing"
                    );
                    break;
                }
                else
                {
                    auto arg1 = __wargv[i];
                    wchar_t* p;
                    auto i = wcstol(arg1, &p, 10);
                    if (!p || *p)
                    {
                        if (wcscmp(arg1, L"error") == 0)
                            outOptions->logLevel = LogLevel::Error;
                        else if (wcscmp(arg1, L"info") == 0)
                            outOptions->logLevel = LogLevel::Info;
                        else if (wcscmp(arg1, L"debug") == 0)
                            outOptions->logLevel = LogLevel::Debug;
                        else
                        {
                            hr = E_INVALIDARG;
                            MakeFormattedString(
                                &errorReason,
                                L"Log level value is invalid (actual: %s)",
                                arg1
                            );
                            break;
                        }
                    }
                    else
                    {
                        if (i < static_cast<decltype(i)>(LogLevel::Error) || i >= static_cast<decltype(i)>(LogLevel::_Count))
                        {
                            hr = E_INVALIDARG;
                            MakeFormattedString(
                                &errorReason,
                                L"Log level value is invalid (actual: %s)",
                                arg1
                            );
                            break;
                        }
                        else
                        {
                            outOptions->logLevel = static_cast<LogLevel>(i);
                        }
                    }
                }
            }
            else
            {
                hr = E_INVALIDARG;
                break;
            }
        }
    }
    if (hr == S_OK)
    {
        if (outOptions->proxyPipeId)
        {
            if (outOptions->listeners || outOptions->connector)
                hr = E_UNEXPECTED;
        }
        else
        {
            if (!outOptions->listeners || !outOptions->connector)
                hr = E_INVALIDARG;
        }
    }
    if (hr == S_FALSE || FAILED(hr))
    {
        ClearOptions(outOptions);
        if (hr == S_FALSE || hr == E_INVALIDARG)
        {
            ShowHelp(errorReason);
        }
    }
    if (errorReason)
        free(errorReason);
    return hr;
}

_Use_decl_annotations_
void ClearOptions(Option* options)
{
    if (options->listeners)
    {
        for (auto iter : *options->listeners)
        {
            if (!iter)
                continue;
            switch (iter->type)
            {
                case ListenerType::TcpSocket:
                {
                    auto p = static_cast<TcpSocketListenerData*>(iter);
                    if (p->pszAddress)
                        free(p->pszAddress);
                }
                break;
                case ListenerType::CygwinSockFile:
                    free(static_cast<CygwinSockFileListenerData*>(iter)->pszCygwinPath);
                    break;
                case ListenerType::Pipe:
                    free(static_cast<PipeListenerData*>(iter)->pszPipeName);
                    break;
                case ListenerType::WslTcpSocket:
                {
                    auto p = static_cast<WslTcpSocketListenerData*>(iter);
                    if (p->pszDistribution)
                        free(p->pszDistribution);
                    if (p->pszAddress)
                        free(p->pszAddress);
                }
                break;
                case ListenerType::WslUnixSocket:
                {
                    auto p = static_cast<WslUnixSocketListenerData*>(iter);
                    if (p->pszDistribution)
                        free(p->pszDistribution);
                    free(p->pszWslPath);
                }
                break;
            }
            free(iter);
        }
        delete options->listeners;
        options->listeners = nullptr;
    }
    auto conn = options->connector;
    if (conn)
    {
        switch (conn->type)
        {
            case ConnectorType::TcpSocket:
                free(static_cast<TcpSocketConnectorData*>(conn)->pszAddress);
                break;
            case ConnectorType::UnixSocket:
                free(static_cast<UnixSocketConnectorData*>(conn)->pszFileName);
                break;
            case ConnectorType::Pipe:
                free(static_cast<PipeConnectorData*>(conn)->pszPipeName);
                break;
            case ConnectorType::WslTcpSocket:
            {
                auto p = static_cast<WslTcpSocketConnectorData*>(conn);
                if (p->pszDistribution)
                    free(p->pszDistribution);
                free(p->pszAddress);
            }
            break;
        }
        free(conn);
        options->connector = nullptr;
    }
    if (options->proxyPipeId)
    {
        free(options->proxyPipeId);
        options->proxyPipeId = nullptr;
    }
    if (options->pszName)
    {
        free(options->pszName);
        options->pszName = nullptr;
    }
}
