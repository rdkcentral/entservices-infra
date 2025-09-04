
#pragma once

#include <interfaces/IPlugin.h>
#include <interfaces/ILifecycleManager.h>
#include <core/JSON.h>

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
    void Deactivated(RPC::IRemoteConnection* connection) override;

    static AppcLifecycleManager* Instance() { return _instance; }

private:
    // JSON-RPC handler
    void get_settargetappstate(const JsonObject& params, JsonObject& response);

    PluginHost::IShell* _service;
    Exchange::ILifecycleManager* _lifecycleManasger;
    uint32_t _connectionId;
    static AppcLifecycleManager* _instance;
};

} // namespace Plugin
} // namespace WPEFramework
