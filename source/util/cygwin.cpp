#include "../framework.h"
#include "../common.h"
#include "cygwin.h"
#include "socket.h"

namespace
{
    HRESULT _IsPortNumberUnused(USHORT port)
    {
        auto hr = InitializeWinsock();
        if (FAILED(hr))
        {
            return hr;
        }

        auto socket = ::socket(PF_INET, SOCK_STREAM, 0);
        if (socket == INVALID_SOCKET)
        {
            return GetLastWSAErrorAsHResult();
        }

        sockaddr_in sin = { 0 };
        sin.sin_family = AF_INET;
        ::InetPtonW(AF_INET, L"127.0.0.1", &sin.sin_addr);
        sin.sin_port = ::htons(port);

        if (::bind(socket, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == SOCKET_ERROR)
        {
            auto err = ::WSAGetLastError();
            if (err == WSAEACCES || err == WSAEADDRINUSE)
            {
                ::closesocket(socket);
                return S_FALSE;
            }
            auto hr = GetWSAErrorAsHResult(err);
            ::closesocket(socket);
            return hr;
        }
        ::closesocket(socket);
        return S_OK;
    }
}

_Use_decl_annotations_
HRESULT CreateCygwinSockFile(LPCWSTR lpFileName, SOCKET socket, const DWORD id[4], bool* pbIsOverwritten)
{
    if (pbIsOverwritten)
        *pbIsOverwritten = false;
    sockaddr_in sin = { 0 };
    int namelen = sizeof(sin);
    if (::getsockname(socket, reinterpret_cast<sockaddr*>(&sin), &namelen) == SOCKET_ERROR)
    {
        return GetLastWSAErrorAsHResult();
    }
    auto port = ::ntohs(sin.sin_port);

    // at least 18 + (8 * 4) + 3
    char buffer[56];
    auto dataSize = _snprintf_s(
        buffer,
        56,
        "!<socket >%hu s %08lX-%08lX-%08lX-%08lX",
        port,
        id[0],
        id[1],
        id[2],
        id[3]
    );

    DWORD dw;
    if (::GetFileAttributesW(lpFileName) != INVALID_FILE_ATTRIBUTES)
    {
        if (!::SetFileAttributesW(lpFileName, 0))
        {
            dw = ::GetLastError();
            _Analysis_assume_(dw != 0);
            return HRESULT_FROM_WIN32(dw);
        }
        if (pbIsOverwritten)
            *pbIsOverwritten = true;
    }
    auto hFile = ::CreateFileW(
        lpFileName,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE)
    {
        dw = ::GetLastError();
        _Analysis_assume_(dw != 0);
        return HRESULT_FROM_WIN32(dw);
    }
    if (!::WriteFile(hFile, buffer, dataSize, &dw, nullptr))
    {
        dw = ::GetLastError();
        _Analysis_assume_(dw != 0);
        ::CloseHandle(hFile);
        ::SetFileAttributesW(lpFileName, 0);
        ::DeleteFileW(lpFileName);
        return HRESULT_FROM_WIN32(dw);
    }
    ::CloseHandle(hFile);
    return S_OK;
}

_Use_decl_annotations_
HRESULT IsWritableAsCygwinSockFile(LPCWSTR lpFileName)
{
    auto curAttr = ::GetFileAttributesW(lpFileName);
    if (curAttr == INVALID_FILE_ATTRIBUTES || !(
        (curAttr & FILE_ATTRIBUTE_READONLY) ||
        (curAttr & FILE_ATTRIBUTE_DEVICE) ||
        (curAttr & FILE_ATTRIBUTE_DIRECTORY)
        ))
    {
        return S_OK;
    }

    DWORD dw = 0;
    auto hFile = ::CreateFileW(lpFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        dw = ::GetLastError();
        _Analysis_assume_(dw != 0);
        return HRESULT_FROM_WIN32(dw);
    }

    // at most 18 + (8 * 4) + 3
    char buffer[56];
    if (!::ReadFile(hFile, buffer, sizeof(buffer), &dw, nullptr))
    {
        dw = ::GetLastError();
        _Analysis_assume_(dw != 0);
        ::CloseHandle(hFile);
        return HRESULT_FROM_WIN32(dw);
    }
    ::CloseHandle(hFile);
    if (dw < 10 || dw >= 56)
    {
        return S_FALSE;
    }
    buffer[dw] = 0;

    if (memcmp(buffer, "!<socket >", sizeof(char) * 10) != 0)
    {
        return S_FALSE;
    }
    // retrieve port number (ushort)
    char* p = nullptr;
    auto val = strtoul(buffer + 10, &p, 10);
    if (p == nullptr || p == buffer + 10 || *p != ' ' || val == 0 || val > 65535)
    {
        return S_FALSE;
    }
    // check the port
    auto hr = _IsPortNumberUnused(static_cast<USHORT>(val));
    return hr;
}
