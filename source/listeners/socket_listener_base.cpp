#include "../framework.h"
#include "../util/socket.h"

#include "socket_listener_base.h"

#include "../util/event_handler.h"
#include "../duplex/socket_duplex.h"

SocketListener::SocketListener()
    : m_socket(INVALID_SOCKET)
    , m_hEvent(nullptr)
    , m_pfnOnAccept(nullptr)
    , m_callbackData(nullptr)
{
}

void SocketListener::Close()
{
    if (m_socket != INVALID_SOCKET)
    {
        ::closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    _Analysis_assume_(m_socket == INVALID_SOCKET);
    if (m_hEvent != nullptr)
    {
        UnregisterEventHandler(m_hEvent);
        ::CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }
    _Analysis_assume_(m_hEvent == nullptr);
}

_Use_decl_annotations_
HRESULT SocketListener::InitSocketImpl(SOCKET sock, PAcceptHandler pfnOnAccept, void* callbackData)
{
    auto hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (hEvent == nullptr)
        return HRESULT_FROM_WIN32(::GetLastError());
    if (::WSAEventSelect(sock, hEvent, FD_ACCEPT) == SOCKET_ERROR)
    {
        ::CloseHandle(hEvent);
        return GetLastWSAErrorAsHResult();
    }
    m_socket = sock;
    m_hEvent = hEvent;
    m_pfnOnAccept = pfnOnAccept;
    m_callbackData = callbackData;
    RegisterEventHandler(m_hEvent, SocketListener::EventHandler, this);
    return S_OK;
}

_Use_decl_annotations_
void SocketListener::EventHandler(void* data)
{
    SocketListener* pThis = static_cast<SocketListener*>(data);
    ::ResetEvent(pThis->m_hEvent);

    auto sock = ::accept(pThis->m_socket, nullptr, nullptr);
    if (sock == INVALID_SOCKET)
    {
        return;
    }

    if (pThis->m_pfnOnAccept)
    {
        // reset m_hEvent
        ::WSAEventSelect(sock, pThis->m_hEvent, 0);
        auto hr = pThis->CheckAcceptedSocket(sock);
        if (SUCCEEDED(hr))
        {
            auto duplex = new SocketDuplex(sock);
            pThis->m_pfnOnAccept(duplex, pThis->m_callbackData);
        }
        else
        {
            // TODO: notify error
            ::closesocket(sock);
        }
    }
    else
    {
        // TODO: notify error
        ::closesocket(sock);
    }
}
