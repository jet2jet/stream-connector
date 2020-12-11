#pragma once

#include "connector.h"

class UnixSocketConnector : public Connector
{
public:
    UnixSocketConnector();
    virtual ~UnixSocketConnector();

    _Check_return_
    HRESULT Initialize(_In_z_ PCWSTR pszFileName, _In_ bool isAbstract);
    _Check_return_
    virtual HRESULT MakeConnection(_When_(return == S_OK, _Outptr_) Duplex** outDuplex) const;

private:
    PWSTR m_pszFileName;
    bool m_isAbstract;
};
