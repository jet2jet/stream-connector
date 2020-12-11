#include "../framework.h"
#include "../util/functions.h"

#include "../duplex/duplex.h"

#include "../logger/logger.h"

#include "worker.h"

struct WorkerData
{
    HANDLE hEventQuit;
    Duplex* duplexIn;
    Duplex* duplexOut;
    PFinishHandler pfnFinishHandler;
    void* dataHandler;
};

struct BufferData
{
    void* buffer;
    DWORD size;
};

static PWSTR MakeBufferString(const void* buffer, DWORD size)
{
    std::wstring str;
    WCHAR buf[4];
    bool isMany = false;
    if (size > 16)
    {
        size = 16;
        isMany = true;
    }
    auto p = static_cast<const BYTE*>(buffer);
    while (size--)
    {
        if (p != buffer)
            swprintf_s(buf, L" %02X", static_cast<UINT>(*p));
        else
            swprintf_s(buf, L"%02X", static_cast<UINT>(*p));
        str += buf;
        ++p;
    }
    if (isMany)
        str += L"...";
    return _wcsdup(str.c_str());
}

static HRESULT ConcatBufferAndRelease(_Inout_ std::vector<BufferData>& allReceived, _Out_ void** outBuffer, _Out_ DWORD* outSize)
{
    *outBuffer = nullptr;
    *outSize = 0;
    DWORD totalSize = 0;
    for (auto& it : allReceived)
        totalSize += it.size;
    auto ptr = static_cast<BYTE*>(malloc(totalSize));
    if (!ptr)
        return E_OUTOFMEMORY;
    *outBuffer = ptr;
    *outSize = totalSize;
    for (auto& it : allReceived)
    {
        memcpy(ptr, it.buffer, it.size);
        free(it.buffer);
        ptr += it.size;
    }
    allReceived.clear();
    return S_OK;
}

_Use_decl_annotations_
HRESULT Transfer(HANDLE hEventQuit, Duplex* from, Duplex* to, PAddLogFormatted logger)
{
    HRESULT hr;
    void* buffer;
    DWORD size;
    HANDLE handleArray[3];
    std::vector<BufferData> allReceived;
    handleArray[0] = hEventQuit;
    bool readFinishedFrom = true;
    bool readFinishedTo = true;

    HANDLE hFrom, hTo;
    hr = from->StartRead(&hFrom);
    if (FAILED(hr))
        return hr;
    hr = to->StartRead(&hTo);
    if (FAILED(hr))
        return hr;
    int fromIndex = 0;
    int toIndex = 0;
    while (true)
    {
        int handleCount = 1;
        if (hFrom != INVALID_HANDLE_VALUE)
        {
            fromIndex = handleCount++;
            handleArray[fromIndex] = hFrom;
        }
        else
            break;
        if (hTo != INVALID_HANDLE_VALUE)
        {
            toIndex = handleCount++;
            handleArray[toIndex] = hTo;
        }
        else
            break;
        if (handleCount == 1)
            break;
        auto r = ::WaitForMultipleObjects(handleCount, handleArray, FALSE, INFINITE);
        // hEventQuit
        if (r == WAIT_OBJECT_0)
            break;
        // 'from' duplex
        else if (hFrom != INVALID_HANDLE_VALUE && r == WAIT_OBJECT_0 + fromIndex)
        {
            while (true)
            {
                buffer = nullptr;
                size = 0;
                hr = from->FinishRead(&buffer, &size);
                if (FAILED(hr))
                    break;
                auto p = MakeBufferString(buffer, size);
                if (logger)
                    logger(LogLevel::Debug, L"  [from] received hr = 0x%08lX, size = %lu <%s>", hr, size, p);
                free(p);
                if (hr != S_OK)
                {
                    hFrom = INVALID_HANDLE_VALUE;
                    break;
                }
                allReceived.push_back({ buffer, size });
                hr = from->StartRead(&hFrom);
                if (FAILED(hr))
                    break;
                if (::WaitForSingleObject(hFrom, 0) != WAIT_OBJECT_0)
                    break;
            }
            if (hr != S_OK && (FAILED(hr) || allReceived.size() == 0))
                break;
            if (allReceived.size() > 0)
            {
                hr = ConcatBufferAndRelease(allReceived, &buffer, &size);
                if (FAILED(hr))
                    break;
                auto p = MakeBufferString(buffer, size);
                if (logger)
                    logger(LogLevel::Debug, L"  [from] sending to 'to' size = %lu <%s>", size, p);
                free(p);
                hr = to->Write(buffer, size, nullptr);
                free(buffer);
                if (FAILED(hr))
                    break;
            }
        }
        // 'to' duplex
        else if (hTo != INVALID_HANDLE_VALUE && r == WAIT_OBJECT_0 + toIndex)
        {
            while (true)
            {
                buffer = nullptr;
                size = 0;
                hr = to->FinishRead(&buffer, &size);
                if (FAILED(hr))
                    break;
                auto p = MakeBufferString(buffer, size);
                if (logger)
                    logger(LogLevel::Debug, L"  [to] received hr = 0x%08lX, size = %lu <%s>", hr, size, p);
                free(p);
                if (hr != S_OK)
                {
                    hTo = INVALID_HANDLE_VALUE;
                    break;
                }
                allReceived.push_back({ buffer, size });
                hr = to->StartRead(&hTo);
                if (FAILED(hr))
                    break;
                if (::WaitForSingleObject(hTo, 0) != WAIT_OBJECT_0)
                    break;
            }
            if (hr != S_OK && (FAILED(hr) || allReceived.size() == 0))
                break;
            if (allReceived.size() > 0)
            {
                hr = ConcatBufferAndRelease(allReceived, &buffer, &size);
                if (FAILED(hr))
                    break;
                auto p = MakeBufferString(buffer, size);
                if (logger)
                    logger(LogLevel::Debug, L"  [to] sending to 'from' size = %lu <%s>", size, p);
                free(p);
                hr = from->Write(buffer, size, nullptr);
                free(buffer);
                if (FAILED(hr))
                    break;
            }
        }
        else if (r == WAIT_FAILED)
            return HRESULT_FROM_WIN32(::GetLastError());
    }
    for (auto& it : allReceived)
    {
        if (it.buffer)
            free(it.buffer);
    }
    return hr;
}

static DWORD WINAPI WorkerThreadProc(WorkerData* data)
{
    auto hr = Transfer(data->hEventQuit, data->duplexIn, data->duplexOut, AddLogFormatted);
    auto pfn = data->pfnFinishHandler;
    auto d = data->dataHandler;
    delete data->duplexIn;
    delete data->duplexOut;
    free(data);
    pfn(d, hr);
    return static_cast<DWORD>(hr);
}

_Use_decl_annotations_
HRESULT StartWorker(HANDLE* outThread, HANDLE hEventQuit, Duplex* duplexIn, Duplex* duplexOut,
    PFinishHandler pfnFinishHandler, void* dataHandler)
{
    *outThread = INVALID_HANDLE_VALUE;
    auto data = static_cast<WorkerData*>(malloc(sizeof(WorkerData)));
    if (!data)
        return E_OUTOFMEMORY;
    data->hEventQuit = hEventQuit;
    data->duplexIn = duplexIn;
    data->duplexOut = duplexOut;
    data->pfnFinishHandler = pfnFinishHandler;
    data->dataHandler = dataHandler;

    HANDLE hThread = reinterpret_cast<HANDLE>(_beginthreadex(
        nullptr,
        0,
        reinterpret_cast<_beginthreadex_proc_type>(WorkerThreadProc),
        data,
        0,
        nullptr
    ));
    if (!hThread)
    {
        auto err = _doserrno;
        free(data);
        return HRESULT_FROM_WIN32(err);
    }
    *outThread = hThread;
    return S_OK;
}
