#include "../framework.h"
#include "../common.h"
#include "../resource.h"

#include "../listeners/listener.h"
#include "../listeners/tcp_socket_listener.h"
#include "../listeners/unix_socket_listener.h"
#include "../listeners/cygwin_sockfile_listener.h"
#include "../listeners/namedpipe_listener.h"
#include "../listeners/wsl_tcp_socket_listener.h"
#include "../listeners/wsl_unix_socket_listener.h"

#include "../connectors/connector.h"
#include "../connectors/tcp_socket_connector.h"
#include "../connectors/pipe_connector.h"
#include "../connectors/unix_socket_connector.h"
#include "../connectors/wsl_tcp_socket_connector.h"
#include "../connectors/wsl_unix_socket_connector.h"

#include "../duplex/duplex.h"

#include "../logger/logger.h"

#include "../util/event_handler.h"
#include "../util/functions.h"
#include "../util/wsl_util.h"

#include "app.h"
#include "worker.h"
#include "window.h"

const Option* g_pOption = nullptr;
WSADATA g_wsaData = { 0 };
HINSTANCE g_hInstance = nullptr;
std::wstring* g_pAppTitle = nullptr;
HANDLE g_hEventQuit = nullptr;
HWND g_hWnd = nullptr;

////////////////////////////////////////////////////////////////////////////////

class AppLogger : public Logger
{
private:
    AppLogger(HANDLE hEvent)
    {
        ::InitializeCriticalSection(&m_csLog);
        m_hEventLogUpdated = hEvent;
        RegisterEventHandler(hEvent, OnLogUpdated, this);
    }
public:
    virtual ~AppLogger()
    {
        ::CloseHandle(m_hEventLogUpdated);
        ::DeleteCriticalSection(&m_csLog);
    }
    static AppLogger* Instantiate()
    {
        auto h = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!h)
            return nullptr;
        auto p = new AppLogger(h);
        if (!p)
        {
            ::CloseHandle(h);
            return nullptr;
        }
        return p;
    }

protected:
    virtual void OnLog(_In_ LogLevel level, _In_z_ PCWSTR pszLog)
    {
        if (level > GetLogLevel())
            return;
        ::EnterCriticalSection(&m_csLog);
        m_logBuffer += pszLog;
        ::LeaveCriticalSection(&m_csLog);
        //::PostMessageW(g_hWnd, MY_WM_UPDATELOG, 0, 0);
        ::SetEvent(m_hEventLogUpdated);
    }

private:
    static void CALLBACK OnLogUpdated(void* data)
    {
        auto pThis = static_cast<AppLogger*>(data);
        ::EnterCriticalSection(&pThis->m_csLog);
        ::ResetEvent(pThis->m_hEventLogUpdated);
        std::wstring str(pThis->m_logBuffer);
        ::LeaveCriticalSection(&pThis->m_csLog);
        ::SendMessageW(g_hWnd, MY_WM_UPDATELOG, 0, reinterpret_cast<LPARAM>(str.c_str()));
    }

private:
    CRITICAL_SECTION m_csLog;
    HANDLE m_hEventLogUpdated;
    std::wstring m_logBuffer;
};

LogLevel GetLogLevel()
{
    return g_pOption ? g_pOption->logLevel : LogLevel::Error;
}

////////////////////////////////////////////////////////////////////////////////

static const PCWSTR g_listenerTypeNames[] = {
    L"tcp-socket", // ListenerType::Socket
    L"unix-socket", // ListenerType::Unix
    L"cygwin-sockfile", // ListenerType::CygwinSockFile
    L"pipe", // ListenerType::Pipe
    L"wsl-tcp-socket", // ListenerType::WslSocket
    L"wsl-unix-socket", // ListenerType::WslUnixSocket
};
static_assert(std::extent<decltype(g_listenerTypeNames)>::value == static_cast<size_t>(ListenerType::_Count), "g_listenerTypeNames is not valid");

std::vector<HANDLE>* g_pThreads = nullptr;
Connector* g_pConnector = nullptr;
std::vector<Listener*>* g_pListeners = nullptr;

static void CALLBACK OnFinishHandler(_In_ ListenerData* data, _In_ HRESULT hr)
{
    auto typeName = g_listenerTypeNames[static_cast<size_t>(data->type)];

    if (FAILED(hr))
    {
        PWSTR psz;
        GetErrorString(hr, &psz);
        if (psz)
            AddLogFormatted(LogLevel::Info, L"[%s %hu] Finished with error: [0x%08lX] %s",
                typeName, data->id, hr, psz);
        else
            AddLogFormatted(LogLevel::Info, L"[%s %hu] Finished with error: [0x%08lX]",
                typeName, data->id, hr);
    }
    else
        AddLogFormatted(LogLevel::Info, L"[%s %hu] Finished", typeName, data->id);
}

static void CALLBACK OnAcceptHandler(_In_ Duplex* duplex, _In_ ListenerData* data)
{
    _Analysis_assume_(g_pThreads != nullptr);

    auto typeName = g_listenerTypeNames[static_cast<size_t>(data->type)];
    AddLogFormatted(LogLevel::Info, L"[%s %hu] Accepted", typeName, data->id);

    HANDLE hThread;
    auto hr = StartWorker(&hThread, g_hEventQuit, duplex, data->id, typeName, g_pConnector,
        reinterpret_cast<PFinishHandler>(OnFinishHandler), data);
    if (FAILED(hr))
    {
        delete duplex;
        PWSTR psz;
        if (SUCCEEDED(GetErrorString(hr, &psz)))
        {
            AddLogFormatted(LogLevel::Error, L"[%s %hu] Failed to create thread: [0x%08lX] %s", typeName, data->id,
                hr, psz);
            free(psz);
        }
        else
        {
            AddLogFormatted(LogLevel::Error, L"[%s %hu] Failed to create thread: [0x%08lX]", typeName, data->id,
                hr);
        }
        return;
    }
    g_pThreads->push_back(hThread);
}

static HRESULT MakeListenersAndConnector(const Option& options)
{
    if (!options.connector || !options.listeners)
    {
        g_pListeners = nullptr;
        g_pThreads = nullptr;
        g_pConnector = nullptr;
        return S_OK;
    }
    g_pListeners = new std::vector<Listener*>();
    if (!g_pListeners)
        return E_OUTOFMEMORY;
    try
    {
        g_pListeners->reserve(options.listeners->size());
    }
    catch (...)
    {
        return E_OUTOFMEMORY;
    }
    g_pThreads = new std::vector<HANDLE>();
    if (!g_pThreads)
        return E_OUTOFMEMORY;

    HRESULT hr = S_OK;
    switch (options.connector->type)
    {
        case ConnectorType::TcpSocket:
        {
            auto d = static_cast<TcpSocketConnectorData*>(options.connector);
            auto p = new TcpSocketConnector();
            hr = p->Initialize(d->pszAddress, d->port);
            if (FAILED(hr))
            {
                delete p;
                return hr;
            }
            g_pConnector = p;
        }
        break;
        case ConnectorType::UnixSocket:
        {
            auto d = static_cast<UnixSocketConnectorData*>(options.connector);
            auto p = new UnixSocketConnector();
            hr = p->Initialize(d->pszFileName, d->isAbstract);
            if (FAILED(hr))
            {
                delete p;
                return hr;
            }
            g_pConnector = p;
        }
        break;
        case ConnectorType::Pipe:
        {
            auto d = static_cast<PipeConnectorData*>(options.connector);
            auto p = new PipeConnector();
            hr = p->Initialize(d->pszPipeName);
            if (FAILED(hr))
            {
                delete p;
                return hr;
            }
            g_pConnector = p;
        }
        break;
#ifdef _WIN64
        case ConnectorType::WslTcpSocket:
        {
            auto d = static_cast<WslTcpSocketConnectorData*>(options.connector);
            auto p = new WslTcpSocketConnector();
            hr = p->Initialize(d->pszDistribution, d->pszAddress, d->port);
            if (FAILED(hr))
            {
                delete p;
                return hr;
            }
            g_pConnector = p;
        }
        break;
        case ConnectorType::WslUnixSocket:
        {
            auto d = static_cast<WslUnixSocketConnectorData*>(options.connector);
            auto p = new WslUnixSocketConnector();
            hr = p->Initialize(d->pszDistribution, d->pszFileName, d->isAbstract);
            if (FAILED(hr))
            {
                delete p;
                return hr;
            }
            g_pConnector = p;
        }
        break;
#endif
        default:
            return E_UNEXPECTED;
    }
    for (auto listener : *options.listeners)
    {
        switch (listener->type)
        {
            case ListenerType::TcpSocket:
            {
                auto d = static_cast<TcpSocketListenerData*>(listener);
                auto p = new TcpSocketListener();
                USHORT port = 0;
                hr = p->InitializeSocket(d->port, d->isIPv6, d->pszAddress, reinterpret_cast<PAcceptHandler>(OnAcceptHandler), listener, &port);
                if (FAILED(hr))
                {
                    delete p;
                    break;
                }
                d->port = port;
                g_pListeners->push_back(p);
                AddLogFormatted(LogLevel::Info, L"[tcp-socket %hu] Listening on %s:%hu",
                    d->id, d->pszAddress ? d->pszAddress : L"", d->port);
            }
            break;
            case ListenerType::UnixSocket:
            {
                auto d = static_cast<UnixSocketListenerData*>(listener);
                auto p = new UnixSocketListener();
                hr = p->InitializeSocket(d->pszPath, d->isAbstract, reinterpret_cast<PAcceptHandler>(OnAcceptHandler), listener);
                if (FAILED(hr))
                {
                    delete p;
                    break;
                }
                g_pListeners->push_back(p);
                AddLogFormatted(LogLevel::Info, L"[unix-socket %hu] Listening on %s%s",
                    d->id, d->isAbstract ? L"<abstract> " : L"", d->pszPath);
            }
            break;
            case ListenerType::CygwinSockFile:
            {
                auto d = static_cast<CygwinSockFileListenerData*>(listener);
                auto p = new CygwinSockFileListener();
                hr = p->InitializeSocket(d->pszCygwinPath, reinterpret_cast<PAcceptHandler>(OnAcceptHandler), listener);
                if (FAILED(hr))
                {
                    delete p;
                    break;
                }
                g_pListeners->push_back(p);
                AddLogFormatted(LogLevel::Info, L"[cygwin-sockfile %hu] Listening on %s",
                    d->id, d->pszCygwinPath);
            }
            break;
            case ListenerType::Pipe:
            {
                auto d = static_cast<PipeListenerData*>(listener);
                auto p = new NamedPipeListener();
                hr = p->Initialize(d->pszPipeName, reinterpret_cast<PAcceptHandler>(OnAcceptHandler), listener);
                if (FAILED(hr))
                {
                    delete p;
                    break;
                }
                g_pListeners->push_back(p);
                AddLogFormatted(LogLevel::Info, L"[pipe %hu] Listening for %s",
                    d->id, d->pszPipeName);
            }
            break;
#ifdef _WIN64
            case ListenerType::WslTcpSocket:
            {
                auto d = static_cast<WslTcpSocketListenerData*>(listener);
                auto p = new WslTcpSocketListener();
                hr = p->Initialize(d->pszDistribution, d->pszAddress, d->isIPv6, d->port, reinterpret_cast<PAcceptHandler>(OnAcceptHandler), listener);
                if (FAILED(hr))
                {
                    delete p;
                    break;
                }
                g_pListeners->push_back(p);
                AddLogFormatted(LogLevel::Info, L"[wsl-tcp-socket %hu] Listening on %s:%hu (distro = %s)",
                    d->id, d->pszAddress ? d->pszAddress : L"", d->port,
                    d->pszDistribution ? d->pszDistribution : L"[default]");
            }
            break;
            case ListenerType::WslUnixSocket:
            {
                auto d = static_cast<WslUnixSocketListenerData*>(listener);
                auto p = new WslUnixSocketListener();
                hr = p->Initialize(d->pszDistribution, d->pszWslPath, d->isAbstract, reinterpret_cast<PAcceptHandler>(OnAcceptHandler), listener);
                if (FAILED(hr))
                {
                    delete p;
                    break;
                }
                g_pListeners->push_back(p);
                AddLogFormatted(LogLevel::Info, L"[wsl-unix-socket %hu] Listening on %s%s (distro = %s)",
                    d->id, d->isAbstract ? L"<abstract> " : L"", d->pszWslPath, d->pszDistribution ? d->pszDistribution : L"[default]");
            }
            break;
#endif
        }
    }
    return hr;
}

_Use_decl_annotations_
void ReportListenersAndConnector(std::wstring& outString)
{
    std::wstring str;

    str = L"Listeners:\n";
    if (g_pOption->listeners && g_pOption->listeners->size() > 0)
    {
        for (auto data : *g_pOption->listeners)
        {
            str += L"* ";
            switch (data->type)
            {
                case ListenerType::TcpSocket:
                {
                    auto d = static_cast<TcpSocketListenerData*>(data);
                    PWSTR psz;
                    if (SUCCEEDED(MakeFormattedString(&psz, L"[tcp-socket %hu] %s:%hu", d->id,
                        d->pszAddress ? d->pszAddress : (d->isIPv6 ? L"[::]" : L"0.0.0.0"), d->port)))
                    {
                        str += psz;
                    }
                }
                break;
                case ListenerType::UnixSocket:
                {
                    auto d = static_cast<UnixSocketListenerData*>(data);
                    PWSTR psz;
                    if (SUCCEEDED(MakeFormattedString(&psz, L"[unix-socket %hu] %s%s", d->id,
                        d->isAbstract ? L"<abstract> " : L"", d->pszPath)))
                    {
                        str += psz;
                    }
                }
                break;
                case ListenerType::CygwinSockFile:
                {
                    auto d = static_cast<CygwinSockFileListenerData*>(data);
                    PWSTR psz;
                    if (SUCCEEDED(MakeFormattedString(&psz, L"[cygwin-sockfile %hu] %s", d->id, d->pszCygwinPath)))
                    {
                        str += psz;
                    }
                }
                break;
                case ListenerType::Pipe:
                {
                    auto d = static_cast<PipeListenerData*>(data);
                    PWSTR psz;
                    if (SUCCEEDED(MakeFormattedString(&psz, L"[pipe %hu] %s", d->id, d->pszPipeName)))
                    {
                        str += psz;
                    }
                }
                break;
                case ListenerType::WslTcpSocket:
                {
                    auto d = static_cast<WslTcpSocketListenerData*>(data);
                    PWSTR psz;
                    if (SUCCEEDED(MakeFormattedString(&psz, L"[wsl-tcp-socket %hu] %s:%hu (distro = %s)",
                        d->id, d->pszAddress ? d->pszAddress : (d->isIPv6 ? L"[::]" : L"0.0.0.0"), d->port,
                        d->pszDistribution ? d->pszDistribution : L"[default]")))
                    {
                        str += psz;
                    }
                }
                break;
                case ListenerType::WslUnixSocket:
                {
                    auto d = static_cast<WslUnixSocketListenerData*>(data);
                    PWSTR psz;
                    if (SUCCEEDED(MakeFormattedString(&psz, L"[wsl-unix-socket %hu] %s%s (distro = %s)",
                        d->id, d->isAbstract ? L"<abstract> " : L"", d->pszWslPath,
                        d->pszDistribution ? d->pszDistribution : L"[default]")))
                    {
                        str += psz;
                    }
                }
                break;
            }
            str += L'\n';
        }
    }
    str += L'\n';
    str += L"Connector: ";
    if (g_pOption->connector)
    {
        switch (g_pOption->connector->type)
        {
            case ConnectorType::TcpSocket:
            {
                auto d = static_cast<TcpSocketConnectorData*>(g_pOption->connector);
                PWSTR psz;
                if (SUCCEEDED(MakeFormattedString(&psz, L"tcp-socket %s:%hu", d->pszAddress, d->port)))
                {
                    str += psz;
                }
            }
            break;
            case ConnectorType::UnixSocket:
            {
                auto d = static_cast<UnixSocketConnectorData*>(g_pOption->connector);
                PWSTR psz;
                if (SUCCEEDED(MakeFormattedString(&psz, L"unix-socket %s%s", d->isAbstract ? L"<abstract> " : L"", d->pszFileName)))
                {
                    str += psz;
                }
            }
            break;
            case ConnectorType::Pipe:
            {
                auto d = static_cast<PipeConnectorData*>(g_pOption->connector);
                PWSTR psz;
                if (SUCCEEDED(MakeFormattedString(&psz, L"pipe %s", d->pszPipeName)))
                {
                    str += psz;
                }
            }
            break;
            case ConnectorType::WslTcpSocket:
            {
                auto d = static_cast<WslTcpSocketConnectorData*>(g_pOption->connector);
                PWSTR psz;
                if (SUCCEEDED(MakeFormattedString(&psz, L"wsl-tcp-socket %s:%hu (distro = %s)",
                    d->pszAddress, d->port, d->pszDistribution ? d->pszDistribution : L"[default]")))
                {
                    str += psz;
                }
            }
            break;
            case ConnectorType::WslUnixSocket:
            {
                auto d = static_cast<WslUnixSocketConnectorData*>(g_pOption->connector);
                PWSTR psz;
                if (SUCCEEDED(MakeFormattedString(&psz, L"wsl-unix-socket %s%s (distro = %s)",
                    d->isAbstract ? L"<abstract> " : L"", d->pszFileName, d->pszDistribution ? d->pszDistribution : L"[default]")))
                {
                    str += psz;
                }
            }
            break;
        }
    }
    outString = str;
}

////////////////////////////////////////////////////////////////////////////////

static AppLogger* g_pAppLogger;

static bool InitInstance(_In_ HINSTANCE hInstance)
{
    if (::WSAStartup(MAKEWORD(2, 2), &g_wsaData) != 0)
        return false;

    g_hEventQuit = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_hEventQuit)
        return false;
    if (!InitEventHandler())
        return false;
    g_pAppLogger = AppLogger::Instantiate();
    if (!g_pAppLogger)
        return false;
    ::logger = g_pAppLogger;

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.lpfnWndProc = WndProc;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    // TODO: icon
    wc.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = nullptr;
    wc.cbWndExtra = sizeof(void*);
    if (!::RegisterClassExW(&wc))
        return false;

    g_hWnd = ::CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        g_pAppTitle->c_str(),
        WS_OVERLAPPED | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        700,
        600,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );
    if (!g_hWnd)
        return false;

    return true;
}

static void PreExitInstance()
{
    if (g_hEventQuit)
        ::SetEvent(g_hEventQuit);
    auto count = g_pThreads ? static_cast<DWORD>(g_pThreads->size()) : 0;
    if (count > 0)
    {
        // wait for thread finish, but terminate if not finished
        auto r = ::WaitForMultipleObjects(count, &g_pThreads->at(0), TRUE, 5000);
        if (r < WAIT_OBJECT_0 || r >= WAIT_OBJECT_0 + count)
        {
            for (auto hThread : *g_pThreads)
            {
#pragma warning(push)
#pragma warning(disable: 6258) // use of TerminateThread
                ::TerminateThread(hThread, static_cast<DWORD>(-1));
#pragma warning(pop)
            }
        }
    }
}

static void ExitInstance()
{
    if (g_pThreads)
    {
        for (auto hThread : *g_pThreads)
            ::CloseHandle(hThread);
    }
    if (g_pListeners)
    {
        for (auto listener : *g_pListeners)
            delete listener;
        delete g_pListeners;
        g_pListeners = nullptr;
    }
    if (g_pConnector)
    {
        delete g_pConnector;
        g_pConnector = nullptr;
    }
    CleanupEventHandler();
    if (g_pAppLogger)
    {
        delete g_pAppLogger;
        ::logger = nullptr;
        g_pAppLogger = nullptr;
    }
    if (g_hEventQuit)
    {
        ::CloseHandle(g_hEventQuit);
        g_hEventQuit = nullptr;
    }
    ::WSACleanup();
}

HINSTANCE GetAppInstance()
{
    return g_hInstance;
}

PCWSTR GetAppTitle()
{
    return g_pAppTitle->c_str();
}

void ProcessShutdown()
{
    PreExitInstance();
    ExitInstance();
}

_Use_decl_annotations_
int AppMain(HINSTANCE hInstance, int nCmdShow, const Option& options)
{
    g_pAppTitle = new std::wstring();
    if (!g_pAppTitle)
        return -1;
    if (FAILED(LoadStringToObject(hInstance, IDS_APP_TITLE, *g_pAppTitle)))
        return -1;
    if (options.pszName)
    {
        std::wstring str(options.pszName);
        *g_pAppTitle = str + L" - " + *g_pAppTitle;
    }

    // check WSL if using wsl-socket or wsl-sockfile
    bool isWSLNecessary = false;
    if (options.connector)
    {
        if (options.connector->type == ConnectorType::WslTcpSocket || options.connector->type == ConnectorType::WslUnixSocket)
            isWSLNecessary = true;
    }
    if (!isWSLNecessary && options.listeners)
    {
        for (auto it : *options.listeners)
        {
            if (it->type == ListenerType::WslTcpSocket || it->type == ListenerType::WslUnixSocket)
            {
                isWSLNecessary = true;
                break;
            }
        }
    }
    if (isWSLNecessary)
    {
#ifndef _WIN64
        ::MessageBoxW(nullptr, L"WSL is only available for x64-based system.", nullptr, MB_ICONHAND);
        return -1;
#else
        auto hr = WslGetDefaultDistribution(nullptr);
        if (FAILED(hr))
        {
            ::MessageBoxW(
                nullptr,
                L"WSL-related option is specified but WSL is not available. Please check if WSL and any distribution are installed",
                g_pAppTitle->c_str(),
                MB_ICONHAND
            );
            return static_cast<int>(hr);
        }
#endif
    }

    g_hInstance = hInstance;
    g_pOption = &options;
    if (!InitInstance(hInstance))
    {
        ExitInstance();
        delete g_pAppTitle;
        return -1;
    }

    {
        auto hr = MakeListenersAndConnector(options);
        if (FAILED(hr))
        {
            PWSTR psz = nullptr;
            if (SUCCEEDED(GetErrorString(hr, &psz)))
                AddLogFormatted(LogLevel::Error, L"Error occurred on initializing listeners: [0x%08lX] %s",
                    static_cast<DWORD>(hr), psz);
            else
                AddLogFormatted(LogLevel::Error, L"Error occurred on initializing listeners: [0x%08lX]",
                    static_cast<DWORD>(hr));
        }
    }

    //::ShowWindow(g_hWnd, nCmdShow);
    //::UpdateWindow(g_hWnd);

    MSG msg;
    while (true)
    {
        ProcessEventHandlers();
        if (::PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE))
        {
            if (!::GetMessageW(&msg, nullptr, 0, 0))
                break;
            if (!::IsDialogMessageW(g_hWnd, &msg))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
            }
        }
    }

    PreExitInstance();
    ExitInstance();
    delete g_pAppTitle;
    return static_cast<int>(msg.wParam);
}
