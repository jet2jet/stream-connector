#pragma once

#include "socket_listener_base.h"

class UnixSocketListener : public SocketListener
{
public:
    UnixSocketListener();
    virtual ~UnixSocketListener() { Close(); }

    _Check_return_
    HRESULT InitializeSocket(
        _In_z_ PCWSTR pszPath,
        _In_ bool isAbstract,
        _In_ PAcceptHandler pfnOnAccept,
        _In_opt_ void* callbackData
    );
    virtual void Close();

private:
    WCHAR* m_pszSocketFile;
};
