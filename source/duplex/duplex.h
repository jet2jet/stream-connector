#pragma once

class Duplex
{
public:
    virtual ~Duplex() {}

    _Check_return_
    virtual HRESULT StartRead(_When_(SUCCEEDED(return), _Out_) HANDLE* outEvent) = 0;
    _Check_return_
    virtual HRESULT FinishRead(
        _When_(return == S_OK, _Outptr_result_bytebuffer_(*outSize))
        _When_(return != S_OK, _Outptr_result_maybenull_)
        void** outBuffer,
        _When_(SUCCEEDED(return), _Out_) DWORD* outSize
    ) = 0;

    virtual HRESULT Write(
        _In_reads_bytes_(size) const void* buffer,
        _In_ DWORD size,
        _When_(SUCCEEDED(return), _Out_opt_) DWORD* outWrittenSize
    ) = 0;
};
