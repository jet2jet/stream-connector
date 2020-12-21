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
    if (isIPv6)
        hr = MakeFormattedString(&pszListen, L"%s:%hu,bind='%s',fork,reuseaddr",
            L"tcp6-listen", port, pszBindAddress ? pszBindAddress : L"[::1]");
    else
        hr = MakeFormattedString(&pszListen, L"%s:%hu,bind='%s',fork,reuseaddr",
            L"tcp4-listen", port, pszBindAddress ? pszBindAddress : L"127.0.0.1");
    if (FAILED(hr))
        return hr;

    hr = WslSocatListenerBase::InitializeBase(pszDistributionName, pszListen, pfnOnAccept, callbackData);
    free(pszListen);

    if (FAILED(hr))
        return hr;

    return S_OK;
}

#endif
