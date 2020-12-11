#include "../framework.h"
#include "../util/socket.h"

#include "tcp_socket_connector.h"

#include "../duplex/socket_duplex.h"

TcpSocketConnector::TcpSocketConnector()
    : m_pszAddress(nullptr)
    , m_szPortString{ 0 }
{
}

TcpSocketConnector::~TcpSocketConnector()
{
    if (m_pszAddress)
        free(m_pszAddress);
}

_Use_decl_annotations_
HRESULT TcpSocketConnector::Initialize(PCWSTR pszAddress, USHORT port)
{
    if (m_pszAddress)
        return E_UNEXPECTED;
    auto psz = _wcsdup(pszAddress);
    if (!psz)
        return E_OUTOFMEMORY;
    m_pszAddress = psz;
    swprintf_s(m_szPortString, L"%hu", port);
    return S_OK;
}

_Use_decl_annotations_
HRESULT TcpSocketConnector::MakeConnection(Duplex** outDuplex) const
{
    ADDRINFOW addrinfo = { 0 };
    PADDRINFOW p;
    addrinfo.ai_family = AF_UNSPEC;
    addrinfo.ai_socktype = SOCK_STREAM;
    addrinfo.ai_protocol = IPPROTO_TCP;
    //addrinfo.ai_flags = 0;

    auto r = ::GetAddrInfoW(m_pszAddress, m_szPortString, &addrinfo, &p);
    if (r != 0)
        return GetWSAErrorAsHResult(r);

    auto sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock == INVALID_SOCKET)
    {
        auto hr = GetLastWSAErrorAsHResult();
        ::FreeAddrInfoW(p);
        return hr;
    }
    if (::connect(sock, p->ai_addr, static_cast<int>(p->ai_addrlen)) == SOCKET_ERROR)
    {
        auto hr = GetLastWSAErrorAsHResult();
        ::FreeAddrInfoW(p);
        return hr;
    }
    ::FreeAddrInfoW(p);

    auto duplex = new SocketDuplex(sock);
    if (!duplex)
    {
        ::closesocket(sock);
        return E_OUTOFMEMORY;
    }
    *outDuplex = duplex;
    return S_OK;
}
