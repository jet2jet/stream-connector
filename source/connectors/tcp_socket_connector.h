#pragma once

#include "connector.h"

class TcpSocketConnector : public Connector
{
public:
    TcpSocketConnector();
    virtual ~TcpSocketConnector();

    _Check_return_
    HRESULT Initialize(_In_z_ PCWSTR pszAddress, _Pre_satisfies_(port > 0) USHORT port);
    _Check_return_
    virtual HRESULT MakeConnection(_When_(return == S_OK, _Outptr_) Duplex** outDuplex) const;

private:
    PWSTR m_pszAddress;
    WCHAR m_szPortString[6];
};
