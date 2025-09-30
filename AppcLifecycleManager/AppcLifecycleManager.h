
#pragma once

#include "Module.h"

#include <interfaces/ILifecycleManager.h>
#include <interfaces/IAppcLifecycleManager.h>
#include <interfaces/json/JsonData_AppcLifecycleManager.h>
#include <interfaces/json/JAppcLifecycleManager.h>
#include <core/JSON.h>
#include <string>
#include "UtilsLogging.h"
#include "tracing/Logging.h"


namespace WPEFramework {
namespace Plugin {

class AppcLifecycleManager : public PluginHost::IPlugin, public PluginHost::JSONRPC, public Exchange::IAppcLifecycleManager {
public:
    AppcLifecycleManager();
    ~AppcLifecycleManager() override;

    // IPlugin methods
    const string Initialize(PluginHost::IShell* service) override;
    void Deinitialize(PluginHost::IShell* service) override;
    string Information() const override;
    void Deactivated(RPC::IRemoteConnection* connection);
    Core::hresult SetTargetAppState(const string& appInstanceId, const Exchange::IAppcLifecycleManager::LifecycleState &targetState, const string& launchIntent) override;
    // Removed singleton accessor

    BEGIN_INTERFACE_MAP(AppcLifecycleManager)
    INTERFACE_ENTRY(PluginHost::IPlugin)
    INTERFACE_ENTRY(PluginHost::IDispatcher)
    INTERFACE_ENTRY(Exchange::IAppcLifecycleManager)
    END_INTERFACE_MAP


private:
    // JSON-RPC handler
    PluginHost::IShell* _currentService;
    Exchange::ILifecycleManager* _lifecycleManager;
    uint32_t _connectionId;
};

} // namespace Plugin
} // namespace WPEFramework
