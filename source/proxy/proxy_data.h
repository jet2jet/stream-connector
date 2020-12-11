#pragma once

#include <pshpack1.h>

#define PROXY_VERSION  1

struct ProxyData
{
    BYTE proxyVersion;
    BYTE logLevel;
    BYTE reserved[6];
};

#include <poppack.h>
