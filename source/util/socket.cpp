#include "../framework.h"
#include "socket.h"

_Use_decl_annotations_
HRESULT GetWSAErrorAsHResult(int result)
{
    return result == 0 ? E_UNEXPECTED : HRESULT_FROM_WIN32(result);
}

_Use_decl_annotations_
HRESULT GetLastWSAErrorAsHResult()
{
    return GetWSAErrorAsHResult(::WSAGetLastError());
}

_Use_decl_annotations_
int recvTimeout(
    SOCKET s,
    void* buf,
    int len,
    int flags,
    DWORD dwTimeoutMillisec
)
{
    if (!len)
        return 0;
    char* curPtr = static_cast<char*>(buf);
    int recvBytes = 0;
    auto dwStart = ::GetTickCount64();
    while (true)
    {
        timeval t;
        fd_set fds[1];
        t.tv_sec = dwTimeoutMillisec / 1000;
        t.tv_usec = (dwTimeoutMillisec % 1000) * 1000;
        FD_ZERO(fds);
        FD_SET(s, fds);
        auto r = ::select(1, fds, nullptr, nullptr, &t);
        if (r == SOCKET_ERROR)
            return SOCKET_ERROR;
        if (r == 0)
            break;
        auto x = ::recv(s, curPtr, 1, 0);
        if (r == SOCKET_ERROR)
            return SOCKET_ERROR;
        if (r == 0)
            break;
        ++curPtr;
        ++recvBytes;
        if (recvBytes == len)
            break;

        auto dwCur = ::GetTickCount64();
        if (dwCur - dwStart > 0xFFFFFFFF || static_cast<DWORD>(dwCur - dwStart) > dwTimeoutMillisec)
            break;
        dwTimeoutMillisec -= static_cast<DWORD>(dwCur - dwStart);
        dwStart = dwCur;
    }
    return recvBytes;
}
