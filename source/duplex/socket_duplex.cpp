#include "../framework.h"
#include "../util/functions.h"
#include "../util/socket.h"

#include "socket_duplex.h"

constexpr DWORD BUFFER_SIZE = 1024;

_Use_decl_annotations_
SocketDuplex::SocketDuplex(SOCKET socket)
    : m_socket(socket)
    , m_ol{ 0 }
    , m_buf{ 0 }
    , m_dwReceived(0)
    , m_isReceived(false)
{
    m_ol.hEvent = INVALID_HANDLE_VALUE;
}

SocketDuplex::~SocketDuplex()
{
    ::shutdown(m_socket, 0);
    ::closesocket(m_socket);
    if (m_buf.buf != nullptr)
    {
        free(m_buf.buf);
    }
    if (m_ol.hEvent != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_ol.hEvent);
    }
}

_Use_decl_annotations_
HRESULT SocketDuplex::StartRead(HANDLE* outEvent)
{
    if (m_ol.hEvent == INVALID_HANDLE_VALUE)
    {
        auto hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!hEvent)
            return HRESULT_FROM_WIN32(::GetLastError());
        m_ol.hEvent = hEvent;
        ResetOverlapped(&m_ol);
    }
    if (!m_buf.buf)
    {
        auto ptr = static_cast<decltype(m_buf.buf)>(malloc(BUFFER_SIZE));
        if (!ptr)
            return E_OUTOFMEMORY;
        m_buf.buf = ptr;
    }

    ::ResetEvent(m_ol.hEvent);
    m_buf.len = BUFFER_SIZE;
    m_dwReceived = 0;
    m_isReceived = false;
    DWORD dwFlags = 0;
    auto r = ::WSARecv(m_socket, &m_buf, 1, &m_dwReceived, &dwFlags, &m_ol, nullptr);
    if (r == 0)
    {
        m_isReceived = true;
        ::SetEvent(m_ol.hEvent);
        *outEvent = m_ol.hEvent;
        return S_OK;
    }
    else if (r != SOCKET_ERROR)
    {
        return E_UNEXPECTED;
    }
    auto err = ::WSAGetLastError();
    if (err == WSA_IO_PENDING)
    {
        *outEvent = m_ol.hEvent;
        return S_OK;
    }
    else
    {
        return GetWSAErrorAsHResult(err);
    }
}

_Use_decl_annotations_
HRESULT SocketDuplex::FinishRead(void** outBuffer, DWORD* outSize)
{
    DWORD dw = 0;
    if (m_isReceived)
    {
        if (!m_dwReceived)
        {
            *outBuffer = nullptr;
            *outSize = 0;
            return S_FALSE;
        }
        dw = m_dwReceived;
    }
    else
    {
        DWORD dwFlags;
        if (!::WSAGetOverlappedResult(m_socket, &m_ol, &dw, FALSE, &dwFlags))
        {
            return GetLastWSAErrorAsHResult();
        }
        if (!dw)
        {
            *outBuffer = nullptr;
            *outSize = 0;
            return S_FALSE;
        }
        if (dw > m_buf.len)
        {
            return E_UNEXPECTED;
        }
        m_dwReceived = dw;
        m_isReceived = true;
    }
    auto ptr = malloc(dw);
    if (!ptr)
    {
        return E_OUTOFMEMORY;
    }
    memcpy(ptr, m_buf.buf, dw);
    *outBuffer = ptr;
    *outSize = dw;
    return S_OK;
}

_Use_decl_annotations_
HRESULT SocketDuplex::Write(const void* buffer, DWORD size, DWORD* outWrittenSize)
{
    DWORD writtenSize = 0;
    while (size > 0)
    {
        auto sendSize = size;
        if (sendSize >= 0x80000000)
            sendSize = 0x7FFFFFE0; // to keep alignment
        auto r = ::send(m_socket, static_cast<const char*>(buffer), static_cast<int>(sendSize), 0);
        if (r == SOCKET_ERROR)
            return GetLastWSAErrorAsHResult();
        if (r < 0)
            return E_UNEXPECTED;
        writtenSize += r;
        if (static_cast<DWORD>(r) < sendSize)
            break;
        size -= sendSize;
        buffer = static_cast<const BYTE*>(buffer) + sendSize;
    }
    if (outWrittenSize)
        *outWrittenSize = writtenSize;
    return S_OK;
}
