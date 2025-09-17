
#pragma once

#include "Module.h"

#include <interfaces/ILifecycleManager.h>
#include <core/JSON.h>
#include <string>
#include <interfaces/json/JsonData_LifecycleManager.h>
#include <interfaces/json/JLifecycleManager.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"


namespace WPEFramework {
namespace Plugin {


class AppcLifecycleManager : public PluginHost::IPlugin, public PluginHost::JSONRPC {
public:
    AppcLifecycleManager();
    ~AppcLifecycleManager() override;

    // IPlugin methods
    const string Initialize(PluginHost::IShell* service) override;
    void Deinitialize(PluginHost::IShell* service) override;
    string Information() const override;
    void Deactivated(RPC::IRemoteConnection* connection);

    static AppcLifecycleManager* Instance() { return _instance; }

    BEGIN_INTERFACE_MAP(AppcLifecycleManager)
    INTERFACE_ENTRY(PluginHost::IPlugin)
    INTERFACE_ENTRY(PluginHost::IDispatcher)
    END_INTERFACE_MAP


private:
    // JSON-RPC handler
    void get_settargetappstate(const JsonObject& params, JsonObject& response);

    PluginHost::IShell* _currentService;
    Exchange::ILifecycleManager* _lifecycleManager;
    uint32_t _connectionId;
    static AppcLifecycleManager* _instance;
};

} // namespace Plugin
} // namespace WPEFramework
