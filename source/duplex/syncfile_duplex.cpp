#include "../framework.h"
#include "../util/functions.h"

#include "syncfile_duplex.h"

constexpr DWORD BUFFER_SIZE = 256;

_Use_decl_annotations_
SyncFileDuplex::SyncFileDuplex(HANDLE hFileIn, HANDLE hFileOut, bool closeOnDispose)
    : m_hThread(INVALID_HANDLE_VALUE)
    , m_hFileIn(hFileIn)
    , m_hFileOut(hFileOut)
    , m_hEventRead(INVALID_HANDLE_VALUE)
    , m_hEventBufferAvailable(INVALID_HANDLE_VALUE)
    , m_workBuffer(nullptr)
    , m_dwBufferSize(0)
    , m_dwReceived(0)
    , m_closeOnDispose(closeOnDispose)
    , m_isClosing(false)
{
    ::InitializeCriticalSection(&m_csRead);
}

SyncFileDuplex::~SyncFileDuplex()
{
    ::CancelIo(m_hFileIn);
    if (m_closeOnDispose)
    {
        ::CloseHandle(m_hFileIn);
        if (m_hFileIn != m_hFileOut && m_hFileOut != INVALID_HANDLE_VALUE)
            ::CloseHandle(m_hFileOut);
    }
    m_isClosing = true;
    if (m_hThread != INVALID_HANDLE_VALUE)
    {
        if (m_hEventBufferAvailable != INVALID_HANDLE_VALUE)
            ::SetEvent(m_hEventBufferAvailable);
        ::CancelSynchronousIo(m_hThread);
        if (::WaitForSingleObject(m_hThread, 1000) != WAIT_OBJECT_0)
        {
#pragma warning(push)
#pragma warning(disable:6258) // use of TerminateThread
            ::TerminateThread(m_hThread, 1);
#pragma warning(pop)
        }
        ::CloseHandle(m_hThread);
    }
    if (m_workBuffer != nullptr)
    {
        free(m_workBuffer);
    }
    if (m_hEventBufferAvailable != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEventBufferAvailable);
    }
    if (m_hEventRead != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEventRead);
    }
    ::DeleteCriticalSection(&m_csRead);
}

_Use_decl_annotations_
HRESULT SyncFileDuplex::StartRead(HANDLE* outEvent)
{
    auto hr = InitEvent();
    if (FAILED(hr))
        return hr;
    ::EnterCriticalSection(&m_csRead);
    ::ResetEvent(m_hEventRead);
    if (m_dwReceived > 0 || ::WaitForSingleObject(m_hThread, 0) == WAIT_OBJECT_0)
        ::SetEvent(m_hEventRead);
    ::LeaveCriticalSection(&m_csRead);
    *outEvent = m_hEventRead;
    return S_OK;
}

_Use_decl_annotations_
HRESULT SyncFileDuplex::FinishRead(void** outBuffer, DWORD* outSize)
{
    if (::WaitForSingleObject(m_hThread, 0) == WAIT_OBJECT_0)
    {
        *outBuffer = nullptr;
        *outSize = 0;
        return S_FALSE;
    }
    ::EnterCriticalSection(&m_csRead);
    if (!m_dwReceived)
    {
        ::LeaveCriticalSection(&m_csRead);
        *outBuffer = nullptr;
        *outSize = 0;
        return S_FALSE;
    }
    auto size = m_dwReceived;
    auto ptr = malloc(size);
    if (!ptr)
    {
        ::LeaveCriticalSection(&m_csRead);
        return E_OUTOFMEMORY;
    }
    memcpy(ptr, m_workBuffer, size);
    m_dwReceived = 0;
    ::SetEvent(m_hEventBufferAvailable);
    ::LeaveCriticalSection(&m_csRead);

    *outBuffer = ptr;
    *outSize = size;
    return S_OK;
}

_Use_decl_annotations_
HRESULT SyncFileDuplex::Write(const void* buffer, DWORD size, DWORD* outWrittenSize)
{
    if (m_hFileOut == INVALID_HANDLE_VALUE)
    {
        // act as writing to NUL device
        if (outWrittenSize)
            *outWrittenSize = size;
        return S_OK;
    }
    if (!::WriteFile(m_hFileOut, buffer, size, outWrittenSize, nullptr))
        return HRESULT_FROM_WIN32(::GetLastError());
    return S_OK;
}

_Use_decl_annotations_
HRESULT SyncFileDuplex::InitEvent()
{
    if (m_workBuffer == nullptr)
    {
        auto ptr = malloc(BUFFER_SIZE);
        if (!ptr)
            return E_OUTOFMEMORY;
        m_workBuffer = ptr;
        m_dwBufferSize = BUFFER_SIZE;
    }
    if (m_hEventRead == INVALID_HANDLE_VALUE)
    {
        auto hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (hEvent == nullptr)
            return HRESULT_FROM_WIN32(::GetLastError());
        m_hEventRead = hEvent;
    }
    if (m_hEventBufferAvailable == INVALID_HANDLE_VALUE)
    {
        // 'signalled' state on init
        auto hEvent = ::CreateEventW(nullptr, TRUE, TRUE, nullptr);
        if (hEvent == nullptr)
            return HRESULT_FROM_WIN32(::GetLastError());
        m_hEventBufferAvailable = hEvent;
    }
    if (m_hThread == INVALID_HANDLE_VALUE)
    {
        auto hThread = reinterpret_cast<HANDLE>(
            _beginthreadex(nullptr, 0, reinterpret_cast<_beginthreadex_proc_type>(_ThreadProc), this, 0, nullptr)
        );
        if (!hThread)
            return HRESULT_FROM_WIN32(_doserrno);
        m_hThread = hThread;
    }
    return S_OK;
}

DWORD WINAPI SyncFileDuplex::_ThreadProc(void* data)
{
    auto pThis = static_cast<SyncFileDuplex*>(data);

    DWORD dwExitCode = 0;
    while (true)
    {
        ::WaitForSingleObject(pThis->m_hEventBufferAvailable, INFINITE);
        if (pThis->m_isClosing)
            break;
        BYTE b;
        DWORD dw;
        if (!::ReadFile(pThis->m_hFileIn, &b, 1, &dw, nullptr))
        {
            auto err = ::GetLastError();
            if (err != ERROR_BROKEN_PIPE)
                dwExitCode = static_cast<DWORD>(HRESULT_FROM_WIN32(err));
            break;
        }
        ::EnterCriticalSection(&pThis->m_csRead);
        static_cast<BYTE*>(pThis->m_workBuffer)[pThis->m_dwReceived] = b;
        ++pThis->m_dwReceived;
        if (pThis->m_dwReceived < pThis->m_dwBufferSize)
            ::SetEvent(pThis->m_hEventBufferAvailable);
        else
            ::ResetEvent(pThis->m_hEventBufferAvailable);
        ::SetEvent(pThis->m_hEventRead);
        ::LeaveCriticalSection(&pThis->m_csRead);
    }
    ::SetEvent(pThis->m_hEventRead);
    return dwExitCode;
}
