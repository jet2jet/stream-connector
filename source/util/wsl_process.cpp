#include "../framework.h"
#include "wsl_process.h"

#include "functions.h"
#include "wsl_util.h"
#include "event_handler.h"

#ifdef _WIN64

constexpr auto BUFFER_SIZE = 1024;

WslProcess::WslProcess()
    : m_hProcess(INVALID_HANDLE_VALUE)
    , m_hStdInWrite(INVALID_HANDLE_VALUE)
    , m_hStdInRead(INVALID_HANDLE_VALUE)
    , m_hStdOutWrite(INVALID_HANDLE_VALUE)
    , m_hStdOutRead(INVALID_HANDLE_VALUE)
    , m_hStdErrWrite(INVALID_HANDLE_VALUE)
    , m_hStdErrRead(INVALID_HANDLE_VALUE)
    , m_olStdIn{ 0 }
    , m_olStdOut{ 0 }
    , m_olStdErr{ 0 }
    , m_bufferStdOut(nullptr)
    , m_bufferStdErr(nullptr)
    , m_receivedStdOut(0)
    , m_receivedStdErr(0)
    , m_isReceivedStdOut(false)
    , m_isReceivedStdErr(false)
    , m_callbackData(nullptr)
    , m_pfnExitHandler(nullptr)
    , m_pfnStdOutHandler(nullptr)
    , m_pfnStdErrHandler(nullptr)
{
    m_olStdIn.hEvent = INVALID_HANDLE_VALUE;
    m_olStdOut.hEvent = INVALID_HANDLE_VALUE;
    m_olStdErr.hEvent = INVALID_HANDLE_VALUE;
}

WslProcess::~WslProcess()
{
    Close();
}

_Use_decl_annotations_
HRESULT WslProcess::StartProcess(PCWSTR pszDistribution, PCWSTR pszCommandLine, void* callbackData,
    PWslProcessExitedHandler pfnExitHandler, PWslStdOutHandler pfnStdOutHandler, PWslStdErrHandler pfnStdErrHandler)
{
    auto hEventStdIn = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEventStdIn)
        return HRESULT_FROM_WIN32(::GetLastError());
    HANDLE hEventStdOut = INVALID_HANDLE_VALUE;
    void* bufferStdOut = nullptr;
    HANDLE hEventStdErr = INVALID_HANDLE_VALUE;
    void* bufferStdErr = nullptr;
    if (pfnStdOutHandler)
    {
        bufferStdOut = malloc(BUFFER_SIZE);
        if (!bufferStdOut)
        {
            ::CloseHandle(hEventStdIn);
            return E_OUTOFMEMORY;
        }
    }
    hEventStdOut = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEventStdOut)
    {
        free(bufferStdOut);
        ::CloseHandle(hEventStdIn);
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    if (pfnStdErrHandler)
    {
        bufferStdErr = malloc(BUFFER_SIZE);
        if (!bufferStdErr)
        {
            if (bufferStdOut)
                free(bufferStdOut);
            ::CloseHandle(hEventStdOut);
            ::CloseHandle(hEventStdIn);
            return E_OUTOFMEMORY;
        }
    }
    hEventStdErr = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEventStdErr)
    {
        auto err = ::GetLastError();
        free(bufferStdErr);
        if (bufferStdOut)
            free(bufferStdOut);
        ::CloseHandle(hEventStdOut);
        ::CloseHandle(hEventStdIn);
        return HRESULT_FROM_WIN32(err);
    }
    HANDLE hProcess;
    PipeData stdIn, stdOut, stdErr;
    auto hr = WslExecute(pszDistribution, pszCommandLine, true, &hProcess, &stdIn, &stdOut, &stdErr);
    if (FAILED(hr))
    {
        if (bufferStdErr)
            free(bufferStdErr);
        if (bufferStdOut)
            free(bufferStdOut);
        ::CloseHandle(hEventStdErr);
        ::CloseHandle(hEventStdOut);
        ::CloseHandle(hEventStdIn);
        return hr;
    }
    m_hProcess = hProcess;
    m_hStdInWrite = stdIn.hWrite;
    m_hStdInRead = stdIn.hRead;
    m_hStdOutWrite = stdOut.hWrite;
    m_hStdOutRead = stdOut.hRead;
    m_hStdErrWrite = stdErr.hWrite;
    m_hStdErrRead = stdErr.hRead;
    ResetOverlapped(&m_olStdIn);
    m_olStdIn.hEvent = hEventStdIn;
    ResetOverlapped(&m_olStdOut);
    m_olStdOut.hEvent = hEventStdOut;
    ResetOverlapped(&m_olStdErr);
    m_olStdErr.hEvent = hEventStdErr;
    m_bufferStdOut = bufferStdOut;
    m_bufferStdErr = bufferStdErr;
    m_receivedStdOut = 0;
    m_receivedStdErr = 0;
    m_isReceivedStdOut = false;
    m_isReceivedStdErr = false;
    m_callbackData = callbackData;
    m_pfnExitHandler = pfnExitHandler;
    m_pfnStdOutHandler = pfnStdOutHandler;
    m_pfnStdErrHandler = pfnStdErrHandler;

    if (pfnStdOutHandler)
        RegisterEventHandler(m_olStdOut.hEvent, _StdOutEventHandler, this);
    if (pfnStdErrHandler)
        RegisterEventHandler(m_olStdErr.hEvent, _StdErrEventHandler, this);
    // listen hProcess event after OVERLAPPED events
    RegisterEventHandler(hProcess, _ProcessEventHandler, this);
    _StartReceiveStdOut();
    _StartReceiveStdErr();

    return S_OK;
}

void WslProcess::Close()
{
    UnregisterEventHandler(m_olStdOut.hEvent);
    UnregisterEventHandler(m_olStdErr.hEvent);
    m_pfnStdOutHandler = nullptr;
    m_pfnStdErrHandler = nullptr;
    if (m_hProcess != INVALID_HANDLE_VALUE)
    {
        _TerminateProcess(static_cast<DWORD>(-1), true);
        UnregisterEventHandler(m_hProcess);
        ::CloseHandle(m_hProcess);
        m_hProcess = INVALID_HANDLE_VALUE;
    }
    if (m_hStdInRead != INVALID_HANDLE_VALUE)
    {
        _Analysis_assume_(m_hStdInWrite != INVALID_HANDLE_VALUE);
        ::CloseHandle(m_hStdInRead);
        ::CancelIo(m_hStdInWrite);
        ::CloseHandle(m_hStdInWrite);
        m_hStdInWrite = INVALID_HANDLE_VALUE;
        m_hStdInRead = INVALID_HANDLE_VALUE;
    }
    if (m_hStdOutRead != INVALID_HANDLE_VALUE)
    {
        _Analysis_assume_(m_hStdOutWrite != INVALID_HANDLE_VALUE);
        ::CloseHandle(m_hStdOutWrite);
        ::CancelIo(m_hStdOutRead);
        ::CloseHandle(m_hStdOutRead);
        m_hStdOutWrite = INVALID_HANDLE_VALUE;
        m_hStdOutRead = INVALID_HANDLE_VALUE;
    }
    if (m_hStdErrRead != INVALID_HANDLE_VALUE)
    {
        _Analysis_assume_(m_hStdErrWrite != INVALID_HANDLE_VALUE);
        ::CloseHandle(m_hStdErrWrite);
        ::CancelIo(m_hStdErrRead);
        ::CloseHandle(m_hStdErrRead);
        m_hStdErrWrite = INVALID_HANDLE_VALUE;
        m_hStdErrRead = INVALID_HANDLE_VALUE;
    }
    if (m_bufferStdErr)
    {
        free(m_bufferStdErr);
        m_bufferStdErr = nullptr;
    }
    if (m_bufferStdOut)
    {
        free(m_bufferStdOut);
        m_bufferStdOut = nullptr;
    }
    if (m_olStdIn.hEvent != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_olStdIn.hEvent);
        m_olStdIn.hEvent = INVALID_HANDLE_VALUE;
    }
    if (m_olStdErr.hEvent != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_olStdErr.hEvent);
        m_olStdErr.hEvent = INVALID_HANDLE_VALUE;
    }
    if (m_olStdOut.hEvent != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_olStdOut.hEvent);
        m_olStdOut.hEvent = INVALID_HANDLE_VALUE;
    }
    m_pfnExitHandler = nullptr;
}

_Use_decl_annotations_
HRESULT WslProcess::SendToStdIn(const void* data, DWORD size)
{
    DWORD len = 0;
    auto hr = ::WriteFileTimeout(m_hStdInWrite, data, size, &len, &m_olStdIn, INFINITE);
    if (FAILED(hr))
        return hr;
    //::FlushFileBuffers(m_hStdInWrite);
    return S_OK;
}

_Use_decl_annotations_
void WslProcess::_OnExitProcess(DWORD dwExitCode)
{
    _Analysis_assume_(m_pfnExitHandler != nullptr);
    m_pfnExitHandler(m_callbackData, dwExitCode);
    Close();
}

_Use_decl_annotations_
void WslProcess::_TerminateProcess(DWORD dwExitCode, bool noCallHandler)
{
    SendToStdIn("\3", 1);
    if (m_hStdInRead != INVALID_HANDLE_VALUE)
    {
        _Analysis_assume_(m_hStdInWrite != INVALID_HANDLE_VALUE);
        ::CloseHandle(m_hStdInRead);
        ::CancelIo(m_hStdInWrite);
        ::CloseHandle(m_hStdInWrite);
        m_hStdInWrite = INVALID_HANDLE_VALUE;
        m_hStdInRead = INVALID_HANDLE_VALUE;
    }

    if (::WaitForSingleObject(m_hProcess, 3000) == WAIT_OBJECT_0)
    {
        if (dwExitCode == 0)
            ::GetExitCodeProcess(m_hProcess, &dwExitCode);
    }
    else
        ::TerminateProcess(m_hProcess, static_cast<UINT>(dwExitCode));
    if (!noCallHandler)
        _OnExitProcess(dwExitCode);
}

void WslProcess::_StartReceiveStdOut()
{
    if (!m_pfnStdOutHandler)
        return;
    ::ResetEvent(m_olStdOut.hEvent);
    //ResetOverlapped(&m_olStdOut);
    m_isReceivedStdOut = false;
    if (::ReadFile(m_hStdOutRead, m_bufferStdOut, BUFFER_SIZE, &m_receivedStdOut, &m_olStdOut))
    {
        m_isReceivedStdOut = true;
        ::SetEvent(m_olStdOut.hEvent);
        return;
    }
    auto err = ::GetLastError();
    if (err != ERROR_IO_PENDING)
    {
        _TerminateProcess(static_cast<DWORD>(HRESULT_FROM_WIN32(err)), false);
        return;
    }
}

void WslProcess::_StartReceiveStdErr()
{
    if (!m_pfnStdErrHandler)
        return;
    ::ResetEvent(m_olStdErr.hEvent);
    //ResetOverlapped(&m_olStdErr);
    m_isReceivedStdErr = false;
    if (::ReadFile(m_hStdErrRead, m_bufferStdErr, BUFFER_SIZE, &m_receivedStdErr, &m_olStdErr))
    {
        m_isReceivedStdErr = true;
        ::SetEvent(m_olStdErr.hEvent);
        return;
    }
    auto err = ::GetLastError();
    if (err != ERROR_IO_PENDING)
    {
        _OnFail(static_cast<DWORD>(HRESULT_FROM_WIN32(err)));
        return;
    }
}

_Use_decl_annotations_
HRESULT WslProcess::_ReadLine(PWSTR* outLine, HANDLE h, OVERLAPPED* pol, DWORD dwTimeoutMillisec)
{
    PSTR pszUtf8;
    auto hr = ReadFileLineUtf8(h, &pszUtf8, pol, dwTimeoutMillisec);
    if (FAILED(hr))
        return hr;
    auto r = ::MultiByteToWideChar(CP_UTF8, 0, pszUtf8, -1, nullptr, 0);
    if (r < 0)
    {
        free(pszUtf8);
        return E_UNEXPECTED;
    }
    auto pOut = static_cast<PWSTR>(malloc(sizeof(WCHAR) * r));
    if (!pOut)
    {
        free(pszUtf8);
        return E_OUTOFMEMORY;
    }
    ::MultiByteToWideChar(CP_UTF8, 0, pszUtf8, -1, pOut, r);
    *outLine = pOut;
    return S_OK;
}

void CALLBACK WslProcess::_ProcessEventHandler(void* data)
{
    auto pThis = static_cast<WslProcess*>(data);
    DWORD dwExitCode = 0;
    if (!::GetExitCodeProcess(pThis->m_hProcess, &dwExitCode))
        dwExitCode = static_cast<DWORD>(HRESULT_FROM_WIN32(::GetLastError()));
    pThis->_OnExitProcess(dwExitCode);
}

void CALLBACK WslProcess::_StdOutEventHandler(void* data)
{
    auto pThis = static_cast<WslProcess*>(data);

    DWORD dw = 0;
    if (pThis->m_isReceivedStdOut)
    {
        dw = pThis->m_receivedStdOut;
    }
    else
    {
        if (!::GetOverlappedResult(pThis->m_hStdOutRead, &pThis->m_olStdOut, &dw, TRUE))
        {
            pThis->_OnFail(HRESULT_FROM_WIN32(::GetLastError()));
            return;
        }
    }
    if (pThis->m_pfnStdOutHandler)
    {
        auto hr = pThis->m_pfnStdOutHandler(pThis->m_callbackData, pThis->m_bufferStdOut, dw);
        if (FAILED(hr))
        {
            pThis->_OnFail(hr);
            return;
        }
        pThis->_StartReceiveStdOut();
    }
}

void CALLBACK WslProcess::_StdErrEventHandler(void* data)
{
    auto pThis = static_cast<WslProcess*>(data);

    DWORD dw = 0;
    if (pThis->m_isReceivedStdErr)
    {
        dw = pThis->m_receivedStdErr;
    }
    else
    {
        if (!::GetOverlappedResult(pThis->m_hStdErrRead, &pThis->m_olStdErr, &dw, TRUE))
        {
            pThis->_OnFail(HRESULT_FROM_WIN32(::GetLastError()));
            return;
        }
    }
    if (pThis->m_pfnStdErrHandler)
    {
        auto hr = pThis->m_pfnStdErrHandler(pThis->m_callbackData, pThis->m_bufferStdErr, dw);
        if (FAILED(hr))
        {
            pThis->_OnFail(hr);
            return;
        }
        pThis->_StartReceiveStdErr();
    }
}

#endif
