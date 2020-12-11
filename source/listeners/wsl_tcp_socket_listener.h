#pragma once

#include "wsl_socat_listener_base.h"

#ifdef _WIN64

class WslTcpSocketListener : public WslSocatListenerBase
{
public:
    WslTcpSocketListener();
    virtual ~WslTcpSocketListener() { Close(); }

    _Check_return_
    HRESULT Initialize(
        _In_opt_z_ PCWSTR pszDistributionName,
        _In_opt_z_ PCWSTR pszBindAddress,
        _In_ bool isIPv6,
        _In_ WORD port,
        _In_ PAcceptHandler pfnOnAccept,
        _In_opt_ void* callbackData
    );
};

#endif
