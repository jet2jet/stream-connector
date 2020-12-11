#include "framework.h"
#include "common.h"

#include "options.h"

#include "util/functions.h"

#include "app/app.h"
#include "proxy/proxy.h"

extern "C" int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    HRESULT hr;

    hr = InitCurrentProcessModuleName(hInstance);
    if (FAILED(hr))
        return static_cast<int>(hr);

    Option opt = { 0 };
    hr = ParseOptions(&opt);
    if (FAILED(hr))
    {
        if (hr == E_INVALIDARG)
            return 1;
        return static_cast<int>(hr);
    }
    if (hr == S_FALSE)
        return 0;
    int r;
    if (opt.proxyPipeId)
        r = ProxyMain(opt);
    else
        r = AppMain(hInstance, nCmdShow, opt);
    ClearOptions(&opt);
    return r;
}
