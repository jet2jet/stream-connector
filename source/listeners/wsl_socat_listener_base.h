#pragma once
#pragma once

#include "namedpipe_listener.h"

#include "../util/wsl_process.h"

#ifdef _WIN64

class WslSocatListenerBase : public NamedPipeListener
{
public:
    WslSocatListenerBase();
    virtual ~WslSocatListenerBase() { Close(); }

    virtual void Close();

protected:
    _Check_return_
    HRESULT InitializeBase(
        _In_opt_z_ LPCWSTR pszDistributionName,
        _In_z_ LPCWSTR pszListen,
        _In_ PAcceptHandler pfnOnAccept,
        _In_opt_ void* callbackData
    );
    _Check_return_
    virtual HRESULT CheckConnectedPipe(_In_ HANDLE hPipe);
    virtual void OnCleanup(_In_opt_z_ LPCWSTR pszDistributionName) {}

private:
    static void CALLBACK _ExitHandler(_In_ void* data, _In_ DWORD dwExitCode);
    static HRESULT CALLBACK _StdErrHandler(_In_ void* data, _In_bytecount_(size) const void* receivedData, _In_ DWORD size);

private:
    WCHAR* m_pszWslDistribution;
    HANDLE m_hEventTemp;
    WslProcess m_process;
    DWORD m_dwWslPid; // not Windows PID
    std::wstring m_strStdErrChunk;
};

#endif
