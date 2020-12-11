#pragma once

#include "wsl_socat_connector_base.h"

#ifdef _WIN64

class WslTcpSocketConnector : public WslSocatConnectorBase
{
public:
    WslTcpSocketConnector();

    _Check_return_
    HRESULT Initialize(_In_opt_z_ PCWSTR pszDistributionName, _In_z_ PCWSTR pszAddress, _Pre_satisfies_(port > 0) USHORT port);
};

#endif
