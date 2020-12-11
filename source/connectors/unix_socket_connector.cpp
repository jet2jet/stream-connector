#include "../framework.h"
#include "../util/socket.h"

#include "unix_socket_connector.h"

#include "../duplex/socket_duplex.h"

#include <afunix.h>

UnixSocketConnector::UnixSocketConnector()
    : m_pszFileName(nullptr)
    , m_isAbstract(false)
{
}

UnixSocketConnector::~UnixSocketConnector()
{
    if (m_pszFileName)
        free(m_pszFileName);
}

_Use_decl_annotations_
HRESULT UnixSocketConnector::Initialize(PCWSTR pszFileName, bool isAbstract)
{
    if (m_pszFileName)
        return E_UNEXPECTED;
    int maxLen = static_cast<int>(std::extent<decltype(sockaddr_un::sun_path)>::value);
    if (isAbstract)
        --maxLen;
    // length limit
    if (::WideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, nullptr, 0, nullptr, nullptr) > maxLen)
        return E_INVALIDARG;
    auto psz = _wcsdup(pszFileName);
    if (!psz)
        return E_OUTOFMEMORY;
    m_pszFileName = psz;
    m_isAbstract = isAbstract;
    return S_OK;
}

_Use_decl_annotations_
HRESULT UnixSocketConnector::MakeConnection(Duplex** outDuplex) const
{
    auto sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        return GetLastWSAErrorAsHResult();
    }
    sockaddr_un sun;
    sun.sun_family = AF_UNIX;
    auto p = sun.sun_path;
    int maxLen = static_cast<int>(std::extent<decltype(sun.sun_path)>::value);
    if (m_isAbstract)
    {
        *p++ = 0;
        --maxLen;
    }
    ::WideCharToMultiByte(CP_UTF8, 0, m_pszFileName, -1, p, maxLen, nullptr, nullptr);
    if (::connect(sock, reinterpret_cast<const sockaddr*>(&sun), static_cast<int>(sizeof(sun))) == SOCKET_ERROR)
    {
        return GetLastWSAErrorAsHResult();
    }

    auto duplex = new SocketDuplex(sock);
    if (!duplex)
    {
        ::closesocket(sock);
        return E_OUTOFMEMORY;
    }
    *outDuplex = duplex;
    return S_OK;
}
