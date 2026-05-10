#include "../framework.h"
#include "../logger/logger.h"
#include "../util/cygwin.h"
#include "../util/functions.h"
#include "../util/socket.h"

#include "cygwin_sockfile_listener.h"

//_Use_decl_annotations_
CygwinSockFileListener::CygwinSockFileListener()
    : m_pszSocketFile(nullptr)
{
    GenerateRandom16Bytes(reinterpret_cast<BYTE*>(m_idSocket));
}

_Use_decl_annotations_
HRESULT CygwinSockFileListener::InitializeSocket(LPCWSTR pszSocketFilePath, PAcceptHandler pfnOnAccept, void* callbackData)
{
    if (m_socket != INVALID_SOCKET)
        return S_OK;
    auto pszSocketFilePathDup = _wcsdup(pszSocketFilePath);
    if (!pszSocketFilePathDup)
    {
        return E_OUTOFMEMORY;
    }
    auto socket = ::socket(PF_INET, SOCK_STREAM, 0);
    if (socket == INVALID_SOCKET)
    {
        auto hr = GetLastWSAErrorAsHResult();
        free(pszSocketFilePathDup);
        return hr;
    }

    sockaddr_in sin = { 0 };
    sin.sin_family = AF_INET;
    ::InetPtonW(AF_INET, L"127.0.0.1", &sin.sin_addr);
    sin.sin_port = 0;

    if (::bind(socket, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == SOCKET_ERROR)
    {
        auto hr = GetLastWSAErrorAsHResult();
        free(pszSocketFilePathDup);
        ::closesocket(socket);
        return hr;
    }
    if (::listen(socket, SOMAXCONN) == SOCKET_ERROR)
    {
        auto hr = GetLastWSAErrorAsHResult();
        free(pszSocketFilePathDup);
        ::closesocket(socket);
        return hr;
    }

    bool bIsOverwritten = false;
    {
        auto hr = CreateCygwinSockFile(pszSocketFilePath, socket, m_idSocket, &bIsOverwritten);
        if (FAILED(hr))
        {
            free(pszSocketFilePathDup);
            ::closesocket(socket);
            return hr;
        }
    }

    {
        auto hr = InitSocketImpl(socket, pfnOnAccept, callbackData);
        if (FAILED(hr))
        {
            ::SetFileAttributesW(pszSocketFilePathDup, 0);
            ::DeleteFileW(pszSocketFilePathDup);
            free(pszSocketFilePathDup);
            ::closesocket(socket);
            return hr;
        }
    }

    m_pszSocketFile = pszSocketFilePathDup;
    if (bIsOverwritten)
    {
        AddLogFormatted(LogLevel::Info, L"[cygwin-sock] Socket file existed, but reused because the socket is no longer available. (file: %s)", pszSocketFilePathDup);
    }
    return S_OK;
}

//_Use_decl_annotations_
void CygwinSockFileListener::Close()
{
    if (m_pszSocketFile)
    {
        ::SetFileAttributesW(m_pszSocketFile, 0);
        ::DeleteFileW(m_pszSocketFile);
        free(m_pszSocketFile);
        m_pszSocketFile = nullptr;
    }
    _Analysis_assume_(m_pszSocketFile == nullptr);
    SocketListener::Close();
}

_Use_decl_annotations_
HRESULT CygwinSockFileListener::CheckAcceptedSocket(SOCKET sock)
{
    // check data
    DWORD recvData[4];
    static_assert(sizeof(recvData) == sizeof(m_idSocket), "size of 'recvId' is not equal to size of 'm_idSocket'");
    auto len = ::recvTimeout(sock, reinterpret_cast<char*>(recvData), sizeof(recvData), 0, 3000);
    if (len == SOCKET_ERROR)
    {
        return GetLastWSAErrorAsHResult();
    }
    // (first, the cygwin-based client must send socket's ID)
    if (len < sizeof(recvData) || memcmp(recvData, m_idSocket, sizeof(m_idSocket)) != 0)
    {
        // invalid data received or no data available
        return S_FALSE;
    }
    if (::send(sock, reinterpret_cast<char*>(recvData), sizeof(recvData), 0) == SOCKET_ERROR)
    {
        return GetLastWSAErrorAsHResult();
    }

    // retrieve client info
    static_assert(sizeof(recvData) > sizeof(DWORD) * 3, "Unexpected");
    len = ::recvTimeout(sock, reinterpret_cast<char*>(recvData), sizeof(DWORD) * 3, 0, 3000);
    if (len == SOCKET_ERROR)
    {
        return GetLastWSAErrorAsHResult();
    }
    // (second, the cygwin-based client must send three DWORD data [pid, uid, gid])
    if (len < sizeof(DWORD) * 3)
    {
        // invalid data received or no data available
        return S_FALSE;
    }
    // send my [pid, uid, gid]
    // (reuse uid and gid)
    recvData[0] = ::GetProcessId(::GetCurrentProcess());
    if (::send(sock, reinterpret_cast<char*>(recvData), sizeof(DWORD) * 3, 0) == SOCKET_ERROR)
    {
        return GetLastWSAErrorAsHResult();
    }

    // all done
    return S_OK;
}
