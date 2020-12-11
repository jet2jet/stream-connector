#include "../framework.h"
#include "wsl_socat_connector_base.h"
#include "../util/functions.h"
#include "../util/wsl_util.h"

#include "../duplex/file_duplex.h"

#ifdef _WIN64

WslSocatConnectorBase::WslSocatConnectorBase()
    : m_pszDistributionName(nullptr)
    , m_pszConnect(nullptr)
{
}

WslSocatConnectorBase::~WslSocatConnectorBase()
{
    if (m_pszDistributionName)
        free(m_pszDistributionName);
    if (m_pszConnect)
        free(m_pszConnect);
}

_Use_decl_annotations_
HRESULT WslSocatConnectorBase::InitializeImpl(PCWSTR pszDistributionName, PCWSTR pszConnect)
{
    if (m_pszConnect)
        return E_UNEXPECTED;
    PWSTR pszD;
    if (!pszDistributionName)
        pszD = nullptr;
    else
    {
        pszD = _wcsdup(pszDistributionName);
        if (!pszD)
            return E_OUTOFMEMORY;
    }
    auto pszC = _wcsdup(pszConnect);
    if (!pszC)
    {
        if (pszD)
            free(pszD);
        return E_OUTOFMEMORY;
    }
    m_pszDistributionName = pszD;
    m_pszConnect = pszC;
    return S_OK;
}

_Use_decl_annotations_
HRESULT WslSocatConnectorBase::MakeConnection(Duplex** outDuplex) const
{
    *outDuplex = nullptr;
    if (!m_pszConnect)
        return E_UNEXPECTED;

    HRESULT hr;
    PWSTR pszSocatFileName;
    hr = WslWhich(m_pszDistributionName, L"socat", 10000, &pszSocatFileName);
    if (FAILED(hr))
        return hr;

    PWSTR pszCommandLine;
    hr = MakeFormattedString(&pszCommandLine, L"%s stdio %s", pszSocatFileName, m_pszConnect);
    free(pszSocatFileName);
    if (FAILED(hr))
        return hr;

    HANDLE hProcess;
    PipeData pipeStdIn, pipeStdOut;
    hr = WslExecute(
        m_pszDistributionName,
        pszCommandLine,
        true,
        &hProcess,
        &pipeStdIn,
        &pipeStdOut,
        nullptr
    );
    free(pszCommandLine);
    if (FAILED(hr))
        return hr;
    ::CloseHandle(pipeStdIn.hRead);
    ::CloseHandle(pipeStdOut.hWrite);
    
    auto duplex = new FileDuplex(pipeStdOut.hRead, pipeStdIn.hWrite, true);
    if (!duplex)
    {
        ::CloseHandle(pipeStdIn.hWrite);
        ::CloseHandle(pipeStdOut.hRead);
        ::TerminateProcess(hProcess, static_cast<UINT>(-1));
        ::CloseHandle(hProcess);
        return E_OUTOFMEMORY;
    }
    ::CloseHandle(hProcess);
    *outDuplex = duplex;
    return S_OK;
}

#endif
