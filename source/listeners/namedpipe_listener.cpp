#include "../framework.h"

#include "namedpipe_listener.h"

#include "../util/event_handler.h"
#include "../duplex/pipe_duplex.h"

NamedPipeListener::NamedPipeListener()
    : m_hPipeCurrent(INVALID_HANDLE_VALUE)
    , m_hEvent(INVALID_HANDLE_VALUE)
    , m_pszPipeName(nullptr)
    , m_pfnOnAccept(nullptr)
    , m_callbackData(nullptr)
{
}

void NamedPipeListener::Close()
{
    if (m_hPipeCurrent != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hPipeCurrent);
        m_hPipeCurrent = INVALID_HANDLE_VALUE;
    }
    if (m_hEvent != INVALID_HANDLE_VALUE)
    {
        UnregisterEventHandler(m_hEvent);
        ::CloseHandle(m_hEvent);
        m_hEvent = INVALID_HANDLE_VALUE;
    }
    if (m_pszPipeName)
    {
        free(m_pszPipeName);
        m_pszPipeName = nullptr;
    }
    m_pfnOnAccept = nullptr;
    m_callbackData = nullptr;
}

_Use_decl_annotations_
HRESULT NamedPipeListener::Initialize(
    PCWSTR pszPipeName,
    PAcceptHandler pfnOnAccept,
    void* callbackData
)
{
    if (m_hPipeCurrent != INVALID_HANDLE_VALUE)
        return S_OK;

    auto pszPipeNameDup = _wcsdup(pszPipeName);
    if (!pszPipeNameDup)
        return E_OUTOFMEMORY;

    auto hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEvent)
    {
        auto err = ::GetLastError();
        free(pszPipeNameDup);
        return HRESULT_FROM_WIN32(err);
    }

    HANDLE hPipe;
    auto hr = _CreatePipe(pszPipeNameDup, hEvent, &hPipe);
    if (FAILED(hr))
    {
        ::CloseHandle(hEvent);
        free(pszPipeNameDup);
        return hr;
    }

    m_hPipeCurrent = hPipe;
    m_pszPipeName = pszPipeNameDup;
    m_hEvent = hEvent;
    m_pfnOnAccept = pfnOnAccept;
    m_callbackData = callbackData;
    RegisterEventHandler(hEvent, NamedPipeListener::EventListener, this);
    return S_OK;
}

_Use_decl_annotations_
HRESULT NamedPipeListener::_CreatePipe(PCWSTR pszPipeName, HANDLE hEvent, HANDLE* outPipe)
{
    auto hPipe = ::CreateNamedPipeW(
        pszPipeName,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        4096,
        4096,
        0,
        nullptr
    );
    if (hPipe == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32(::GetLastError());

    OVERLAPPED ol = { 0 };
    ol.hEvent = hEvent;
    if (::ConnectNamedPipe(hPipe, &ol))
    {
        // (may not reach here)
        ::SetEvent(hEvent);
    }
    else
    {
        auto err = ::GetLastError();
        if (err == ERROR_PIPE_CONNECTED)
        {
            // already connected, so set event signaled
            ::SetEvent(hEvent);
        }
        else if (err != ERROR_IO_PENDING)
        {
            ::CloseHandle(hPipe);
            return HRESULT_FROM_WIN32(err);
        }
    }

    *outPipe = hPipe;
    return S_OK;
}

_Use_decl_annotations_
void CALLBACK NamedPipeListener::EventListener(void* data)
{
    auto pThis = static_cast<NamedPipeListener*>(data);
    ::ResetEvent(pThis->m_hEvent);

    auto hPipeToUse = pThis->m_hPipeCurrent;

    PipeDuplex* duplex = nullptr;
    auto hr = pThis->CheckConnectedPipe(hPipeToUse);
    if (FAILED(hr))
    {
        // TODO: error
        ::CloseHandle(hPipeToUse);
    }
    else
    {
        duplex = new PipeDuplex(hPipeToUse, hPipeToUse);
        if (!duplex)
        {
            // TODO: error
        }
    }

    // renew pipe to new connection
    // (even if failed to create duplex)
    HANDLE hPipeNew;
    hr = pThis->_CreatePipe(pThis->m_pszPipeName, pThis->m_hEvent, &hPipeNew);
    if (FAILED(hr))
    {
        // TODO: error
        pThis->Close();
        return;
    }
    pThis->m_hPipeCurrent = hPipeNew;

    if (duplex)
        pThis->m_pfnOnAccept(duplex, pThis->m_callbackData);
}
