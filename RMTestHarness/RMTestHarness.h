#pragma once

#include "Module.h"
#include <essos-resmgr.h>
#include "UtilsLogging.h"
#include "UtilsJsonRpc.h"
#include <interfaces/json/JsonData_RMTestHarness.h>
#include <interfaces/json/JRMTestHarness.h>
#include <interfaces/IRMTestHarness.h>
#include <map>
#include <mutex>
#include <vector>
#include <string>

namespace WPEFramework {
namespace Plugin {

class RMTestHarness : public PluginHost::IPlugin, public PluginHost::JSONRPC {
public:
    RMTestHarness();
    virtual ~RMTestHarness();

    BEGIN_INTERFACE_MAP(RMTestHarness)
        INTERFACE_AGGREGATE(Exchange::IRMTestHarness, mRMTestHarness)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_ENTRY(PluginHost::IDispatcher)
    END_INTERFACE_MAP

    // IPlugin
    const string Initialize(PluginHost::IShell* service) override;
    void Deinitialize(PluginHost::IShell* service) override;
    string Information() const override;

private:
    RMTestHarness(const RMTestHarness&) = delete;
    RMTestHarness& operator=(const RMTestHarness&) = delete;

    PluginHost::IShell* mService;
    uint32_t mConnectionId;
    Exchange::IRMTestHarness* mRMTestHarness;
};

} // namespace Plugin
} // namespace WPEFramework