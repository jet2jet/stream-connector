#pragma once

#include "socket_listener_base.h"

class CygwinSockFileListener : public SocketListener
{
public:
    CygwinSockFileListener();
    virtual ~CygwinSockFileListener() { Close(); }

    _Check_return_
    HRESULT InitializeSocket(
        _In_ LPCWSTR pszSocketFilePath,
        _In_ PAcceptHandler pfnOnAccept,
        _In_opt_ void* callbackData
    );
    virtual void Close();

protected:
    _Check_return_
    virtual HRESULT CheckAcceptedSocket(_In_ SOCKET socket);

private:
    DWORD m_idSocket[4];
    WCHAR* m_pszSocketFile;
};
