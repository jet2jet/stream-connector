#pragma once

#include "duplex.h"

class SocketDuplex : public Duplex
{
public:
    SocketDuplex(_In_ SOCKET socket);
    virtual ~SocketDuplex();

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
    SOCKET m_socket;
    WSAOVERLAPPED m_ol;
    WSABUF m_buf;
    DWORD m_dwReceived;
    bool m_isReceived;
};
