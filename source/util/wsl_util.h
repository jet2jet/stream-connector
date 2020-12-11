#pragma once

struct PipeData
{
    HANDLE hRead;
    HANDLE hWrite;
};

#ifdef _WIN64

_Check_return_
HRESULT WslGetDefaultDistribution(_When_(SUCCEEDED(return), _Outptr_opt_result_z_) PWSTR* outDistribution);

_Check_return_
HRESULT WslIsValidDistribution(_In_opt_z_ PCWSTR pszDistribution);

_Check_return_
HRESULT WslExecute(
    _In_opt_z_ PCWSTR pszDistribution,
    _In_z_ PCWSTR pszCommandLine,
    _In_ bool isPipeOverlapped,
    _When_(SUCCEEDED(return), _Out_) HANDLE* outProcess,
    _When_(SUCCEEDED(return), _Out_opt_) PipeData* outStdIn,
    _When_(SUCCEEDED(return), _Out_opt_) PipeData* outStdOut,
    _When_(SUCCEEDED(return), _Out_opt_) PipeData* outStdErr
);

_Check_return_
HRESULT WslWhich(
    _In_opt_z_ PCWSTR pszDistribution,
    _In_z_ PCWSTR pszExecutable,
    _In_ DWORD dwTimeoutMillisec,
    _When_(return == S_OK, _Outptr_) _When_(return == S_FALSE, _Post_satisfies_(*pszResult == nullptr)) PWSTR* pszResult
);

_Check_return_
HRESULT WslPath(
    _In_opt_z_ PCWSTR pszDistribution,
    _In_z_ PCWSTR pszFile,
    _In_ DWORD dwTimeoutMillisec,
    _When_(SUCCEEDED(return), _Outptr_) PWSTR* pszResult
);

// return S_OK if pszWslFile is writable file name (i.e. not dir) or S_FALSE otherwise
_Check_return_
HRESULT WslTestIfWritable(
    _In_opt_z_ PCWSTR pszDistribution,
    _In_z_ PCWSTR pszWslFile,
    _In_ DWORD dwTimeoutMillisec
);

#endif
