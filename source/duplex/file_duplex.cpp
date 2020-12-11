#include "../framework.h"
#include "../util/functions.h"

#include "file_duplex.h"

constexpr DWORD BUFFER_SIZE = 1024;

_Use_decl_annotations_
FileDuplex::FileDuplex(HANDLE hFileIn, HANDLE hFileOut, bool closeOnDispose)
    : m_hFileIn(hFileIn)
    , m_hFileOut(hFileOut)
    , m_ol{ 0 }
    , m_olWrite{ 0 }
    , m_workBuffer(nullptr)
    , m_dwBufferSize(0)
    , m_dwReceived(0)
    , m_closeOnDispose(closeOnDispose)
    , m_isReceived(false)
{
    m_ol.hEvent = INVALID_HANDLE_VALUE;
    m_olWrite.hEvent = INVALID_HANDLE_VALUE;
    if (::GetFileType(hFileIn) == FILE_TYPE_PIPE)
    {
        // determine if hFileIn is not an actual pipe (e.g. socket)
        DWORD dwDummy = 0;
        if (!::GetNamedPipeInfo(hFileIn, &dwDummy, nullptr, nullptr, nullptr))
        {
            ::SetLastError(0);
            m_isPipeInput = false;
        }
        else
            m_isPipeInput = true;
    }
    else
    {
        m_isPipeInput = false;
    }
}

FileDuplex::~FileDuplex()
{
    ::CancelIo(m_hFileIn);
    if (m_closeOnDispose)
    {
        ::CloseHandle(m_hFileIn);
        if (m_hFileIn != m_hFileOut && m_hFileOut != INVALID_HANDLE_VALUE)
            ::CloseHandle(m_hFileOut);
    }
    if (m_workBuffer != nullptr)
    {
        free(m_workBuffer);
    }
    if (m_olWrite.hEvent != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_olWrite.hEvent);
    }
    if (m_ol.hEvent != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_ol.hEvent);
    }
}

_Use_decl_annotations_
HRESULT FileDuplex::StartRead(HANDLE* outEvent)
{
    if (m_ol.hEvent == INVALID_HANDLE_VALUE)
    {
        auto hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!hEvent)
            return HRESULT_FROM_WIN32(::GetLastError());
        m_ol.hEvent = hEvent;
        ResetOverlapped(&m_ol);
    }
    if (!m_workBuffer)
    {
        auto ptr = static_cast<decltype(m_workBuffer)>(malloc(BUFFER_SIZE));
        if (!ptr)
            return E_OUTOFMEMORY;
        m_workBuffer = ptr;
    }

    ::ResetEvent(m_ol.hEvent);
    m_dwBufferSize = BUFFER_SIZE;
    m_dwReceived = 0;
    m_isReceived = false;
    DWORD dwFlags = 0;
    if (::ReadFile(m_hFileIn, m_workBuffer, m_dwBufferSize, &m_dwReceived, &m_ol))
    {
        m_isReceived = true;
        ::SetEvent(m_ol.hEvent);
        *outEvent = m_ol.hEvent;
        return S_OK;
    }
    auto err = ::GetLastError();
    if (err == ERROR_IO_PENDING)
    {
        *outEvent = m_ol.hEvent;
        return S_OK;
    }
    else if (err == ERROR_BROKEN_PIPE)
    {
        m_dwReceived = 0;
        m_isReceived = true;
        ::SetEvent(m_ol.hEvent);
        *outEvent = m_ol.hEvent;
        return S_OK;
    }
    else
    {
        return HRESULT_FROM_WIN32(err);
    }
}

_Use_decl_annotations_
HRESULT FileDuplex::FinishRead(void** outBuffer, DWORD* outSize)
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
        if (!::GetOverlappedResult(m_hFileIn, &m_ol, &dw, TRUE))
        {
            auto err = ::GetLastError();
            if (err == ERROR_BROKEN_PIPE)
            {
                *outBuffer = nullptr;
                *outSize = 0;
                return S_FALSE;
            }
            return HRESULT_FROM_WIN32(err);
        }
        if (!dw)
        {
            *outBuffer = nullptr;
            *outSize = 0;
            return S_FALSE;
        }
        if (dw > m_dwBufferSize)
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
    memcpy(ptr, m_workBuffer, dw);
    *outBuffer = ptr;
    *outSize = dw;
    return S_OK;
}

_Use_decl_annotations_
HRESULT FileDuplex::Write(const void* buffer, DWORD size, DWORD* outWrittenSize)
{
    if (m_hFileOut == INVALID_HANDLE_VALUE)
    {
        // act as writing to NUL device
        if (outWrittenSize)
            *outWrittenSize = size;
        return S_OK;
    }
    auto hr = InitEvent();
    if (FAILED(hr))
        return hr;
    ResetOverlapped(&m_olWrite);
    ::ResetEvent(m_olWrite.hEvent);
    if (!::WriteFile(m_hFileOut, buffer, size, outWrittenSize, &m_olWrite))
    {
        auto err = ::GetLastError();
        if (err != ERROR_IO_PENDING)
            return HRESULT_FROM_WIN32(err);
        DWORD dw = 0;
        if (!::GetOverlappedResult(m_hFileOut, &m_olWrite, &dw, TRUE))
            return HRESULT_FROM_WIN32(::GetLastError());
        if (outWrittenSize)
            *outWrittenSize = dw;
    }
    ::FlushFileBuffers(m_hFileOut);
    return S_OK;
}

_Use_decl_annotations_
HRESULT FileDuplex::InitEvent()
{
    if (m_ol.hEvent == INVALID_HANDLE_VALUE)
    {
        auto hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (hEvent == nullptr)
            return HRESULT_FROM_WIN32(::GetLastError());
        m_ol.hEvent = hEvent;
    }
    if (m_olWrite.hEvent == INVALID_HANDLE_VALUE)
    {
        auto hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (hEvent == nullptr)
            return HRESULT_FROM_WIN32(::GetLastError());
        m_olWrite.hEvent = hEvent;
    }
    return S_OK;
}
