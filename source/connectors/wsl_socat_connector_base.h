#pragma once

#include "connector.h"

#ifdef _WIN64

class WslSocatConnectorBase : public Connector
{
public:
    WslSocatConnectorBase();
    virtual ~WslSocatConnectorBase();

    _Check_return_
    virtual HRESULT MakeConnection(_When_(return == S_OK, _Outptr_) Duplex** outDuplex) const;

protected:
    _Check_return_
    HRESULT InitializeImpl(_In_opt_z_ PCWSTR pszDistributionName, _In_z_ PCWSTR pszConnect);

private:
    PWSTR m_pszDistributionName;
    PWSTR m_pszConnect;
};

#endif
