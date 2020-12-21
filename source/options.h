#pragma once

#include <vector>

#include "logger/logger.h"

enum class ListenerType : WORD
{
    TcpSocket = 0,
    UnixSocket,
    CygwinSockFile,
    Pipe,
    WslTcpSocket,
    WslUnixSocket,
    _Count
};

#define WSL_DEFAULT_TIMEOUT  30000

struct ListenerData
{
    ListenerType type;
    WORD id;
};

struct TcpSocketListenerData : public ListenerData
{
    WORD port;
    bool isIPv6;
    _Field_z_ PWSTR pszAddress;
};

struct UnixSocketListenerData : public ListenerData
{
    _Field_z_ PWSTR pszPath;
    bool isAbstract;
};

struct CygwinSockFileListenerData : public ListenerData
{
    _Field_z_ PWSTR pszCygwinPath;
};

struct PipeListenerData : public ListenerData
{
    _Field_z_ PWSTR pszPipeName;
};

struct WslTcpSocketListenerData : public ListenerData
{
    _Field_z_ _Maybenull_ PWSTR pszDistribution;
    WORD port;
    bool isIPv6;
    _Field_z_ PWSTR pszAddress;
};

struct WslUnixSocketListenerData : public ListenerData
{
    _Field_z_ _Maybenull_ PWSTR pszDistribution;
    _Field_z_ PWSTR pszWslPath;
    bool isAbstract;
};

enum class ConnectorType : WORD
{
    TcpSocket = 0,
    UnixSocket,
    Pipe,
    WslTcpSocket,
    WslUnixSocket,
    _Count
};

struct ConnectorData
{
    ConnectorType type;
};

struct TcpSocketConnectorData : public ConnectorData
{
    WORD port;
    _Field_z_ PWSTR pszAddress;
};

struct UnixSocketConnectorData : public ConnectorData
{
    _Field_z_ PWSTR pszFileName;
    bool isAbstract;
};

struct PipeConnectorData : public ConnectorData
{
    _Field_z_ PWSTR pszPipeName;
};

struct WslTcpSocketConnectorData : public ConnectorData
{
    _Field_z_ _Maybenull_ PWSTR pszDistribution;
    WORD port;
    _Field_z_ PWSTR pszAddress;
};

struct WslUnixSocketConnectorData : public ConnectorData
{
    _Field_z_ _Maybenull_ PWSTR pszDistribution;
    _Field_z_ PWSTR pszFileName;
    bool isAbstract;
};

typedef ListenerData* PListenerData;

struct Option
{
    PWSTR proxyPipeId;
    PWSTR pszName;
    std::vector<ListenerData*>* listeners;
    ConnectorData* connector;
    DWORD wslDefaultTimeout;
    LogLevel logLevel;
    BYTE wslSocatLogLevel;
};

_Check_return_
HRESULT ParseOptions(_When_(return == S_OK, _Out_) Option* outOptions);
void ClearOptions(_Inout_ Option* options);
