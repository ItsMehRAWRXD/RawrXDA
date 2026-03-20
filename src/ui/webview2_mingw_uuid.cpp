#ifdef __MINGW32__

#include <guiddef.h>
#include <WebView2.h>

template <>
const GUID& __mingw_uuidof<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>() {
    static const GUID iid{
        0x6c4819f3, 0xc9b7, 0x4260, {0x81, 0x27, 0xc9, 0xf5, 0xe8, 0xbd, 0xe7, 0xf4}
    };
    return iid;
}

template <>
const GUID& __mingw_uuidof<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>() {
    static const GUID iid{
        0x4e8a3389, 0xc9d8, 0x4bd2, {0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d}
    };
    return iid;
}

#endif
