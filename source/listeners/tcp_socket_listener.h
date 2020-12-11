#pragma once

#include "socket_listener_base.h"

class TcpSocketListener : public SocketListener
{
public:
    virtual ~TcpSocketListener() { Close(); }

    _Check_return_
    HRESULT InitializeSocket(
        _In_ USHORT port,
        _In_ bool isIPv6,
        _In_opt_z_ PCWSTR pszBindAddress,
        _In_ PAcceptHandler pfnOnAccept,
        _In_opt_ void* callbackData,
        _When_(return == S_OK, _Out_opt_) USHORT* outPort
    );
    _Check_return_
    inline HRESULT InitializeSocket(
        _In_opt_z_ PCWSTR pszBindAddress,
        _In_ bool isIPv6,
        _In_ PAcceptHandler pfnOnAccept,
        _In_opt_ void* callbackData,
        _When_(return == S_OK, _Out_opt_) USHORT* outPort
    )
    {
        return InitializeSocket(0, isIPv6, pszBindAddress, pfnOnAccept, callbackData, outPort);
    }
};
