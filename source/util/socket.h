#pragma once

_Post_satisfies_(FAILED(return))
HRESULT GetWSAErrorAsHResult(_In_ int result);
_Post_satisfies_(FAILED(return))
HRESULT GetLastWSAErrorAsHResult();
_Check_return_
int recvTimeout(
    _In_ SOCKET s,
    _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) void* buf,
    _In_ int len,
    _In_ int flags,
    _In_ DWORD dwTimeoutMillisec
);
