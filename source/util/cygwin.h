#pragma once

_Check_return_
HRESULT CreateCygwinSockFile(_In_ LPCWSTR lpFileName, _In_ SOCKET socket, _In_ const DWORD id[4], _Out_opt_ bool* pbIsOverwritten);
_Check_return_
HRESULT IsWritableAsCygwinSockFile(_In_ LPCWSTR lpFileName);
