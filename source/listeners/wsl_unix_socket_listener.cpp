#include "../framework.h"
#include "../util/functions.h"
#include "../util/wsl_util.h"

#include "../app/app.h"

#include "wsl_unix_socket_listener.h"

#ifdef _WIN64

//_Use_decl_annotations_
WslUnixSocketListener::WslUnixSocketListener()
    : m_pszSocketWslFile(nullptr)
{
}

_Use_decl_annotations_
HRESULT WslUnixSocketListener::Initialize(LPCWSTR pszDistributionName, LPCWSTR pszSocketWslFilePath, bool isAbstract, PAcceptHandler pfnOnAccept, void* callbackData)
{
    if (m_hPipeCurrent != INVALID_HANDLE_VALUE)
        return S_OK;
    auto hr = WslTestIfWritable(pszDistributionName, pszSocketWslFilePath, GetWslDefaultTimeout());
    if (FAILED(hr))
        return hr;
    if (hr == S_FALSE)
        return E_ACCESSDENIED;
    auto pszSocketWslFilePathDup = _wcsdup(pszSocketWslFilePath);
    if (!pszSocketWslFilePathDup)
        return E_OUTOFMEMORY;

    PWSTR pszListen;
    hr = MakeFormattedString(&pszListen, L"%s:'%s',fork",
        isAbstract ? L"abstract-listen" : L"unix-listen", pszSocketWslFilePathDup);
    if (FAILED(hr))
    {
        free(pszSocketWslFilePathDup);
        return hr;
    }

    m_pszSocketWslFile = pszSocketWslFilePathDup;

    hr = WslSocatListenerBase::InitializeBase(pszDistributionName, pszListen, pfnOnAccept, callbackData);
    free(pszListen);

    if (FAILED(hr))
        return hr;

    return S_OK;
}

_Use_decl_annotations_
void WslUnixSocketListener::OnCleanup(LPCWSTR pszDistributionName)
{
    if (m_pszSocketWslFile)
    {
        PWSTR p;
        if (pszDistributionName && SUCCEEDED(::MakeFormattedString(&p, L"sh -c \"rm -f '%s'\"", m_pszSocketWslFile)))
        {
            HANDLE h;
            if (SUCCEEDED(::WslExecute(pszDistributionName, p, false, &h, nullptr, nullptr, nullptr)))
            {
                if (::WaitForSingleObject(h, 3000) != WAIT_OBJECT_0)
                {
                    ::TerminateProcess(h, static_cast<UINT>(-1));
                }
                ::CloseHandle(h);
            }
            free(p);
        }
        free(m_pszSocketWslFile);
        m_pszSocketWslFile = nullptr;
    }
}

#endif
