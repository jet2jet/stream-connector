#include "../framework.h"
#include "pipe_connector.h"

#include "../duplex/pipe_duplex.h"
#include "../logger/logger.h"

constexpr int RETRY_COUNT = 5;
constexpr DWORD RETRY_MILLISEC = 500;

PipeConnector::PipeConnector()
    : m_pszPipeName(nullptr)
{
}

PipeConnector::~PipeConnector()
{
    if (m_pszPipeName)
        free(m_pszPipeName);
}

_Use_decl_annotations_
HRESULT PipeConnector::Initialize(PCWSTR pszPipeName)
{
    if (m_pszPipeName)
        return E_UNEXPECTED;
    auto psz = _wcsdup(pszPipeName);
    if (!psz)
        return E_OUTOFMEMORY;
    m_pszPipeName = psz;
    return S_OK;
}

_Use_decl_annotations_
HRESULT PipeConnector::MakeConnection(Duplex** outDuplex) const
{
    *outDuplex = nullptr;
    if (!m_pszPipeName)
        return E_UNEXPECTED;
    HANDLE hPipe;
    int retryCount = 1;
    while (true)
    {
        hPipe = ::CreateFileW(
            m_pszPipeName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr
        );
        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        auto err = ::GetLastError();
        if (err != ERROR_PIPE_BUSY)
            return HRESULT_FROM_WIN32(err);
        ++retryCount;
        if (retryCount == RETRY_COUNT)
            return HRESULT_FROM_WIN32(ERROR_PIPE_BUSY);
        AddLogFormatted(LogLevel::Info, L"[pipe] '%s' is busy; retry... (%d/%d)",
            m_pszPipeName, retryCount, RETRY_COUNT);
        ::Sleep(RETRY_MILLISEC);
    }
    if (hPipe == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32(ERROR_PIPE_BUSY);
    auto duplex = new PipeDuplex(hPipe, hPipe);
    if (!duplex)
    {
        ::CloseHandle(hPipe);
        return E_OUTOFMEMORY;
    }
    *outDuplex = duplex;
    return S_OK;
}
