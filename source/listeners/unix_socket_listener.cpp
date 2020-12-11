#include "../framework.h"
#include "../util/socket.h"

#include "unix_socket_listener.h"

#include <afunix.h>

UnixSocketListener::UnixSocketListener()
    : m_pszSocketFile(nullptr)
{
}

_Use_decl_annotations_
HRESULT UnixSocketListener::InitializeSocket(PCWSTR pszPath, bool isAbstract, PAcceptHandler pfnOnAccept, void* callbackData)
{
    if (m_socket != INVALID_SOCKET)
        return E_UNEXPECTED;

    auto lenWithNull = ::WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
    if (isAbstract)
        ++lenWithNull;
    if (lenWithNull > UNIX_PATH_MAX)
        return E_INVALIDARG;

    auto p = _wcsdup(pszPath);
    if (!p)
        return E_OUTOFMEMORY;

    auto socket = ::socket(PF_UNIX, SOCK_STREAM, 0);
    if (socket == INVALID_SOCKET)
    {
        free(p);
        return GetLastWSAErrorAsHResult();
    }

    sockaddr_un sin = { 0 };
    sin.sun_family = AF_UNIX;
    if (isAbstract)
    {
        ::WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, sin.sun_path + 1, UNIX_PATH_MAX - 1, nullptr, nullptr);
        sin.sun_path[0] = 0;
    }
    else
        ::WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, sin.sun_path, UNIX_PATH_MAX, nullptr, nullptr);

    if (::bind(socket, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == SOCKET_ERROR)
    {
        auto hr = GetLastWSAErrorAsHResult();
        ::closesocket(socket);
        free(p);
        return hr;
    }
    if (::listen(socket, SOMAXCONN) == SOCKET_ERROR)
    {
        auto hr = GetLastWSAErrorAsHResult();
        ::closesocket(socket);
        free(p);
        return hr;
    }

    {
        auto hr = InitSocketImpl(socket, pfnOnAccept, callbackData);
        if (FAILED(hr))
        {
            free(p);
            return hr;
        }
    }

    m_socket = socket;
    m_pszSocketFile = p;
    return S_OK;
}

//_Use_decl_annotations_
void UnixSocketListener::Close()
{
    SocketListener::Close();
    if (m_pszSocketFile)
    {
        ::SetFileAttributesW(m_pszSocketFile, 0);
        ::DeleteFileW(m_pszSocketFile);
        free(m_pszSocketFile);
        m_pszSocketFile = nullptr;
    }
    _Analysis_assume_(m_pszSocketFile == nullptr);
}
