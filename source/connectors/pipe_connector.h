#pragma once

#include "connector.h"

class PipeConnector : public Connector
{
public:
    PipeConnector();
    virtual ~PipeConnector();

    _Check_return_
    HRESULT Initialize(_In_z_ PCWSTR pszPipeName);
    _Check_return_
    virtual HRESULT MakeConnection(_When_(return == S_OK, _Outptr_) Duplex** outDuplex) const;

private:
    PWSTR m_pszPipeName;
};
