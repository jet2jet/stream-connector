#pragma once

#ifdef _WIN64

typedef void (CALLBACK* PWslProcessExitedHandler)(_In_ void* data, _In_ DWORD dwExitCode);
typedef HRESULT (CALLBACK* PWslStdOutHandler)(_In_ void* data, _In_bytecount_(size) const void* receivedData, _In_ DWORD size);
typedef HRESULT (CALLBACK* PWslStdErrHandler)(_In_ void* data, _In_bytecount_(size) const void* receivedData, _In_ DWORD size);

class WslProcess
{
public:
    WslProcess();
    ~WslProcess();

    _Check_return_
    HRESULT StartProcess(
        _In_opt_z_ PCWSTR pszDistribution,
        _In_z_ PCWSTR pszCommandLine,
        _In_ void* callbackData,
        _In_ PWslProcessExitedHandler pfnExitHandler,
        _In_opt_ PWslStdOutHandler pfnStdOutHandler,
        _In_opt_ PWslStdErrHandler pfnStdErrHandler
    );
    void Close();
    HRESULT SendToStdIn(_In_bytecount_(size) const void* data, _In_ DWORD size);

    HRESULT ReadLineFromStdOut(_When_(SUCCEEDED(return), _Outptr_result_z_) PWSTR* outLine, _In_ DWORD dwTimeoutMillisec)
    {
        return _ReadLine(outLine, m_hStdOutRead, &m_olStdOut, dwTimeoutMillisec);
    }
    HRESULT ReadLineFromStdErr(_When_(SUCCEEDED(return), _Outptr_result_z_) PWSTR* outLine, _In_ DWORD dwTimeoutMillisec)
    {
        return _ReadLine(outLine, m_hStdErrRead, &m_olStdErr, dwTimeoutMillisec);
    }

private:
    void _OnExitProcess(_In_ DWORD dwExitCode);
    void _TerminateProcess(_In_ DWORD dwExitCode, _In_ bool noCallHandler);
    void _StartReceiveStdOut();
    void _StartReceiveStdErr();
    static HRESULT _ReadLine(_When_(SUCCEEDED(return), _Outptr_result_z_) PWSTR* outLine, _In_ HANDLE h, _Inout_ OVERLAPPED* pol, _In_ DWORD dwTimeoutMillisec);

    inline void _OnFail(_In_ HRESULT hr)
    {
        _TerminateProcess(static_cast<DWORD>(hr), false);
    }

    static void CALLBACK _ProcessEventHandler(void* data);
    static void CALLBACK _StdOutEventHandler(void* data);
    static void CALLBACK _StdErrEventHandler(void* data);

private:
    HANDLE m_hProcess;
    HANDLE m_hStdInWrite;
    HANDLE m_hStdInRead;
    HANDLE m_hStdOutWrite;
    HANDLE m_hStdOutRead;
    HANDLE m_hStdErrWrite;
    HANDLE m_hStdErrRead;
    OVERLAPPED m_olStdIn;
    OVERLAPPED m_olStdOut;
    OVERLAPPED m_olStdErr;
    void* m_bufferStdOut;
    void* m_bufferStdErr;
    DWORD m_receivedStdOut;
    DWORD m_receivedStdErr;
    bool m_isReceivedStdOut;
    bool m_isReceivedStdErr;
    void* m_callbackData;
    PWslProcessExitedHandler m_pfnExitHandler;
    PWslStdOutHandler m_pfnStdOutHandler;
    PWslStdErrHandler m_pfnStdErrHandler;
};

#endif
