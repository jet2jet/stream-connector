#pragma once

#include "wsl_socat_listener_base.h"

#ifdef _WIN64

class WslUnixSocketListener : public WslSocatListenerBase
{
public:
    WslUnixSocketListener();
    virtual ~WslUnixSocketListener() { Close(); }

    _Check_return_
    HRESULT Initialize(
        _In_opt_z_ LPCWSTR pszDistributionName,
        _In_z_ LPCWSTR pszSocketWslFilePath,
        _In_ bool isAbstract,
        _In_ PAcceptHandler pfnOnAccept,
        _In_opt_ void* callbackData
    );

protected:
    virtual void OnCleanup(_In_opt_z_ LPCWSTR pszDistributionName);

private:
    WCHAR* m_pszSocketWslFile;
};

#endif
