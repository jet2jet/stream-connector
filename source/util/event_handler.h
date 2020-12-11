#pragma once

void RegisterEventHandler(_In_ HANDLE hEvent, _In_ void (CALLBACK* pfnCallback)(void* data), _In_opt_ void* data);
void UnregisterEventHandler(_In_ HANDLE hEvent);

_Check_return_ bool InitEventHandler();
void CleanupEventHandler();
void ProcessEventHandlers();
