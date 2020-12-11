#pragma once

#include "wsl_socat_connector_base.h"

#ifdef _WIN64

class WslUnixSocketConnector : public WslSocatConnectorBase
{
public:
    WslUnixSocketConnector();

    _Check_return_
    HRESULT Initialize(_In_opt_z_ PCWSTR pszDistributionName, _In_z_ PCWSTR pszWslPathName, _In_ bool isAbstract);
};

#endif
