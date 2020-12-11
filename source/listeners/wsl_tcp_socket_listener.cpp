#include "../framework.h"
#include "../util/functions.h"
#include "../util/wsl_util.h"

#include "wsl_tcp_socket_listener.h"

#ifdef _WIN64

//_Use_decl_annotations_
WslTcpSocketListener::WslTcpSocketListener()
{
}

_Use_decl_annotations_
HRESULT WslTcpSocketListener::Initialize(PCWSTR pszDistributionName, PCWSTR pszBindAddress, bool isIPv6, WORD port, PAcceptHandler pfnOnAccept, void* callbackData)
{
    if (m_hPipeCurrent != INVALID_HANDLE_VALUE)
        return S_OK;

    PWSTR pszListen;
    HRESULT hr;
    if (pszBindAddress)
        hr = MakeFormattedString(&pszListen, L"%s:%hu,bind='%s',fork,reuseaddr",
            isIPv6 ? L"tcp6-listen" : L"tcp4-listen", port, pszBindAddress);
    else
        hr = MakeFormattedString(&pszListen, L"%s:%hu,fork,reuseaddr",
            isIPv6 ? L"tcp6-listen" : L"tcp4-listen", port);
    if (FAILED(hr))
        return hr;

    hr = WslSocatListenerBase::InitializeBase(pszDistributionName, pszListen, pfnOnAccept, callbackData);
    free(pszListen);

    if (FAILED(hr))
        return hr;

    return S_OK;
}

#endif
