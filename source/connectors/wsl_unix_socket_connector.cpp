#include "../framework.h"
#include "../util/functions.h"

#include "wsl_unix_socket_connector.h"

#ifdef _WIN64

WslUnixSocketConnector::WslUnixSocketConnector()
{
}

_Use_decl_annotations_
HRESULT WslUnixSocketConnector::Initialize(PCWSTR pszDistributionName, PCWSTR pszWslPathName, bool isAbstract)
{
    PWSTR pszConnect;
    auto hr = MakeFormattedString(&pszConnect, L"%s:'%s'",
        isAbstract ? L"abstract-connect" : L"unix-connect", pszWslPathName);
    if (FAILED(hr))
        return hr;
    hr = InitializeImpl(pszDistributionName, pszConnect);
    free(pszConnect);
    return hr;
}

#endif
