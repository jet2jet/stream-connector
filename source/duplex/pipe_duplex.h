#pragma once

#include "file_duplex.h"

class PipeDuplex : public FileDuplex
{
public:
    PipeDuplex(_In_ HANDLE hPipeIn, _In_ HANDLE hPipeOut);
};
