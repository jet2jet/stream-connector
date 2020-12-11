#include "../framework.h"
#include "event_handler.h"

struct _EventHandlerData
{
    HANDLE hEvent;
    void (CALLBACK* pfnCallback)(void* data);
    void* data;
};

std::vector<HANDLE>* g_pHandles = nullptr;
std::vector<_EventHandlerData>* g_pHandlers = nullptr;

_Check_return_ bool InitEventHandler()
{
    g_pHandles = new std::vector<HANDLE>();
    if (!g_pHandles)
        return false;
    g_pHandlers = new std::vector<_EventHandlerData>();
    if (!g_pHandlers)
        return false;
    return true;
}

void CleanupEventHandler()
{
    if (g_pHandles)
    {
        delete g_pHandles;
        g_pHandles = nullptr;
    }
    if (g_pHandlers)
    {
        delete g_pHandlers;
        g_pHandlers = nullptr;
    }
}

_Use_decl_annotations_
void RegisterEventHandler(HANDLE hEvent, void (CALLBACK* pfnCallback)(void* data), void* data)
{
    _Analysis_assume_(g_pHandles != nullptr);
    _Analysis_assume_(g_pHandlers != nullptr);
    g_pHandles->push_back(hEvent);
    g_pHandlers->push_back({ hEvent, pfnCallback, data });
}

_Use_decl_annotations_
void UnregisterEventHandler(HANDLE hEvent)
{
    if (hEvent == INVALID_HANDLE_VALUE)
        return;
    _Analysis_assume_(g_pHandles != nullptr);
    _Analysis_assume_(g_pHandlers != nullptr);

    auto it1 = g_pHandles->begin();
    auto it2 = g_pHandlers->begin();
    while (true)
    {
        if (it1 == g_pHandles->end())
            break;
        if (*it1 == hEvent)
        {
            g_pHandles->erase(it1, it1 + 1);
            g_pHandlers->erase(it2, it2 + 1);
            break;
        }
        ++it1;
    }
}

//_Use_decl_annotations_
void ProcessEventHandlers()
{
    while (true)
    {
        auto size = static_cast<DWORD>(g_pHandles ? g_pHandles->size() : 0);
        auto arr = size > 0 ? &g_pHandles->at(0) : nullptr;
        auto r = ::MsgWaitForMultipleObjectsEx(size, arr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (r >= WAIT_OBJECT_0 && r < WAIT_OBJECT_0 + size)
        {
            auto index = r - WAIT_OBJECT_0;
            auto listener = g_pHandlers->cbegin() + index;
            listener->pfnCallback(listener->data);
        }
        else if (r == WAIT_OBJECT_0 + size)
        {
            return;
        }
    }
}
