#include "../framework.h"
#include "../util/functions.h"

#include "wsl_tcp_socket_connector.h"

#ifdef _WIN64

WslTcpSocketConnector::WslTcpSocketConnector()
{
}

_Use_decl_annotations_
HRESULT WslTcpSocketConnector::Initialize(PCWSTR pszDistributionName, PCWSTR pszAddress, USHORT port)
{
    PWSTR pszConnect;
    auto hr = MakeFormattedString(&pszConnect, L"tcp-connect:'%s':%hu", pszAddress, port);
    if (FAILED(hr))
        return hr;
    hr = InitializeImpl(pszDistributionName, pszConnect);
    free(pszConnect);
    return hr;
}

#endif
