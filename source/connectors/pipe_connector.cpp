#include "../framework.h"
#include "pipe_connector.h"

#include "../duplex/pipe_duplex.h"

PipeConnector::PipeConnector()
    : m_pszPipeName(nullptr)
{
}

PipeConnector::~PipeConnector()
{
    if (m_pszPipeName)
        free(m_pszPipeName);
}

_Use_decl_annotations_
HRESULT PipeConnector::Initialize(PCWSTR pszPipeName)
{
    if (m_pszPipeName)
        return E_UNEXPECTED;
    auto psz = _wcsdup(pszPipeName);
    if (!psz)
        return E_OUTOFMEMORY;
    m_pszPipeName = psz;
    return S_OK;
}

_Use_decl_annotations_
HRESULT PipeConnector::MakeConnection(Duplex** outDuplex) const
{
    *outDuplex = nullptr;
    if (!m_pszPipeName)
        return E_UNEXPECTED;
    HANDLE hPipe = ::CreateFileW(
        m_pszPipeName,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr
    );
    if (hPipe == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32(::GetLastError());
    auto duplex = new PipeDuplex(hPipe, hPipe);
    if (!duplex)
    {
        ::CloseHandle(hPipe);
        return E_OUTOFMEMORY;
    }
    *outDuplex = duplex;
    return S_OK;
}
