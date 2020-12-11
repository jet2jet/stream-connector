#pragma once

class Duplex;

typedef void (CALLBACK* PAcceptHandler)(_In_ Duplex* duplex, _In_ void* data);

class Listener
{
public:
    virtual ~Listener() {}
    virtual void Close() {}
};
