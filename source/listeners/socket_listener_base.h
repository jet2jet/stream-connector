#pragma once

#include "listener.h"

class SocketListener : public Listener
{
public:
    SocketListener();
    virtual ~SocketListener() {}

    virtual void Close();

protected:
    _Check_return_
    HRESULT InitSocketImpl(
        _In_ SOCKET sock,
        _In_ PAcceptHandler pfnOnAccept,
        _In_opt_ void* callbackData
    );
    _Check_return_
    virtual HRESULT CheckAcceptedSocket(_In_ SOCKET socket) { return S_OK; }

private:
    static void CALLBACK EventHandler(_In_ void* data);

protected:
    SOCKET m_socket;
    HANDLE m_hEvent;
    PAcceptHandler m_pfnOnAccept;
    void* m_callbackData;
};
