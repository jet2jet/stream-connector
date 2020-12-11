#pragma once

#include "duplex.h"

class FileDuplex : public Duplex
{
public:
    // hFileIn can be equal to hFileOut
    FileDuplex(_In_ HANDLE hFileIn, _In_ HANDLE hFileOut, _In_ bool closeOnDispose);
    virtual ~FileDuplex();

    _Check_return_
    virtual HRESULT StartRead(_When_(SUCCEEDED(return), _Out_) HANDLE* outEvent);
    _Check_return_
    virtual HRESULT FinishRead(
        _When_(return == S_OK, _Outptr_result_bytebuffer_(*outSize))
        _When_(return != S_OK, _Outptr_result_maybenull_)
        void** outBuffer,
        _When_(SUCCEEDED(return), _Out_) DWORD* outSize
    );

    virtual HRESULT Write(
        _In_reads_bytes_(size) const void* buffer,
        _In_ DWORD size,
        _When_(SUCCEEDED(return), _Out_opt_) DWORD* outWrittenSize
    );

private:
    _Check_return_
    HRESULT InitEvent();

    HANDLE m_hFileIn;
    HANDLE m_hFileOut;
    OVERLAPPED m_ol;
    OVERLAPPED m_olWrite;
    void* m_workBuffer;
    DWORD m_dwBufferSize;
    DWORD m_dwReceived;
    bool m_closeOnDispose;
    bool m_isPipeInput;
    bool m_isReceived;
};
