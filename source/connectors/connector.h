#pragma once

class Duplex;

class Connector
{
public:
	virtual ~Connector() {}

	_Check_return_
	virtual HRESULT MakeConnection(_When_(return == S_OK, _Outptr_) Duplex** outDuplex) const = 0;
};
