#include "inexor/ui/cefframe.h"

void InexorCefFrame::InitializeContext()
{
    CreateFunction("log", this);
}

bool InexorCefFrame::Execute(const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception)
{
    CEF_REQUIRE_RENDERER_THREAD();
    if (name == "log") {
        if (arguments.size() == 1 && arguments[0]->IsString()) {
            logoutf("[%d] %s", lastmillis, arguments[0]->GetStringValue().ToString().c_str());
            return true;
        }
    }
    return false;
}

bool InexorCefFrame::Get(const CefString& name, const CefRefPtr<CefV8Value> object, CefRefPtr<CefV8Value>& return_value, CefString& exception)
{
    CEF_REQUIRE_RENDERER_THREAD();
    return false;
}

bool InexorCefFrame::Set(const CefString& name, const CefRefPtr<CefV8Value> object, const CefRefPtr<CefV8Value> value, CefString& exception)
{
    CEF_REQUIRE_RENDERER_THREAD();
    return false;
}

