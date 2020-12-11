#include "../framework.h"
#include "../util/socket.h"

#include "tcp_socket_listener.h"

_Use_decl_annotations_
HRESULT TcpSocketListener::InitializeSocket(USHORT port, bool isIPv6, PCWSTR pszBindAddress, PAcceptHandler pfnOnAccept, void* callbackData, USHORT* outPort)
{
    if (m_socket != INVALID_SOCKET)
        return E_UNEXPECTED;

    SOCKET socket;
    union
    {
        sockaddr_in sin;
        sockaddr_in6 sin6;
    } u = { 0 };
    int namelen;

    if (pszBindAddress)
    {
        addrinfoW hints = { 0 };
        PADDRINFOW results;
        WCHAR buf[8];
        hints.ai_family = isIPv6 ? AF_INET6 : AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_NUMERICSERV;
        swprintf_s(buf, L"%hu", port);
        auto r = ::GetAddrInfoW(pszBindAddress, buf, &hints, &results);
        if (r != 0)
        {
            return GetWSAErrorAsHResult(r);
        }
        if (results == nullptr)
        {
            return GetWSAErrorAsHResult(WSAEADDRNOTAVAIL);
        }
        socket = ::socket(results->ai_family, results->ai_socktype, results->ai_protocol);
        if (socket == INVALID_SOCKET)
        {
            auto hr = GetLastWSAErrorAsHResult();
            ::FreeAddrInfoW(results);
            return hr;
        }
        if (::bind(socket, results->ai_addr, static_cast<int>(results->ai_addrlen)) == SOCKET_ERROR)
        {
            auto hr = GetLastWSAErrorAsHResult();
            ::closesocket(socket);
            ::FreeAddrInfoW(results);
            return hr;
        }
        ::FreeAddrInfoW(results);
    }
    else
    {
        socket = ::socket(isIPv6 ? PF_INET6 : PF_INET, SOCK_STREAM, 0);
        if (socket == INVALID_SOCKET)
        {
            return GetLastWSAErrorAsHResult();
        }

        if (isIPv6)
        {
            namelen = sizeof(u.sin6);
            u.sin6.sin6_family = AF_INET6;
            ::InetPtonW(AF_INET6, L"[::1]", &u.sin6.sin6_addr);
            u.sin6.sin6_port = htons(port);
        }
        else
        {
            namelen = sizeof(u.sin);
            u.sin.sin_family = AF_INET;
            ::InetPtonW(AF_INET, L"127.0.0.1", &u.sin.sin_addr);
            u.sin.sin_port = htons(port);
        }

        if (::bind(socket, reinterpret_cast<sockaddr*>(&u), namelen) == SOCKET_ERROR)
        {
            auto hr = GetLastWSAErrorAsHResult();
            ::closesocket(socket);
            return hr;
        }
    }
    if (::listen(socket, SOMAXCONN) == SOCKET_ERROR)
    {
        auto hr = GetLastWSAErrorAsHResult();
        ::closesocket(socket);
        return hr;
    }
    namelen = isIPv6 ? sizeof(u.sin6) : sizeof(u.sin);
    if (::getsockname(socket, reinterpret_cast<sockaddr*>(&u), &namelen) == SOCKET_ERROR)
    {
        auto hr = GetLastWSAErrorAsHResult();
        ::closesocket(socket);
        return hr;
    }
    port = ::ntohs(isIPv6 ? u.sin6.sin6_port : u.sin.sin_port);

    {
        auto hr = InitSocketImpl(socket, pfnOnAccept, callbackData);
        if (FAILED(hr))
        {
            return hr;
        }
    }

    if (outPort)
        *outPort = port;

    m_socket = socket;
    return S_OK;
}
