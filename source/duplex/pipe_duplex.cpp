#include "../framework.h"

#include "pipe_duplex.h"

_Use_decl_annotations_
PipeDuplex::PipeDuplex(HANDLE hPipeIn, HANDLE hPipeOut)
    : FileDuplex(hPipeIn, hPipeOut, true)
{
}
