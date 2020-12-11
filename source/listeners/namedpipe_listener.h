#pragma once

#include "listener.h"

class NamedPipeListener : public Listener
{
public:
    NamedPipeListener();
    virtual ~NamedPipeListener() { Close(); }

    virtual void Close();

    _Check_return_
    HRESULT Initialize(
        _In_z_ PCWSTR pszPipeName,
        _In_ PAcceptHandler pfnOnAccept,
        _In_opt_ void* callbackData
    );

protected:
    _Check_return_
    virtual HRESULT CheckConnectedPipe(_In_ HANDLE hPipe)
    {
        return S_OK;
    }
    _Check_return_
    static HRESULT _CreatePipe(_In_z_ PCWSTR pszPipeName, _In_ HANDLE hEvent, _When_(SUCCEEDED(return), _Out_) HANDLE* outPipe);

private:
    static void CALLBACK EventListener(_In_ void* data);

protected:
    HANDLE m_hPipeCurrent;
private:
    HANDLE m_hEvent;
    PWSTR m_pszPipeName;
    PAcceptHandler m_pfnOnAccept;
    void* m_callbackData;
};
