/*
 * L2 Test for SceneSet Plugin
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <interfaces/IAppManager.h>

using namespace WPEFramework;
using namespace WPEFramework::Exchange;

#define SCENESET_CALLSIGN _T("org.rdk.SceneSet")
#define JSON_TIMEOUT (1000)

class SceneSetL2Test : public L2TestMocks {
protected:
    ~SceneSetL2Test() override = default;
    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(SceneSetL2Test, PluginAvailable) {
    // Verify plugin can be located via COM-RPC
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> engine(Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create());
    PluginHost::IShell* shell = Service<PluginHost::IShell>(SCENESET_CALLSIGN, engine, JSON_TIMEOUT);
    ASSERT_NE(shell, nullptr) << "SceneSet plugin shell not available";
    if (shell) shell->Release();
}

/*TEST_F(SceneSetL2Test, AcquireAppManagerInterfaceIfConfigured) {
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> engine(Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create());
    PluginHost::IShell* shell = Service<PluginHost::IShell>(SCENESET_CALLSIGN, engine, JSON_TIMEOUT);
    ASSERT_NE(shell, nullptr);

    IAppManager* appManager = nullptr;
    if (shell) {
        void* iface = shell->QueryInterfaceByCallsign(InterfaceType<IAppManager>::ID, _T("org.rdk.AppManager"));
        if (iface) {
            appManager = reinterpret_cast<IAppManager*>(iface);
        }
    }
    // appManager may be null if AppManager not present; just log.
    if (appManager) {
        appManager->Release();
    }
    if (shell) shell->Release();
}*/

TEST_F(SceneSetL2Test, Initialize_ReturnsSuccess) {
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> engine(Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create());
    PluginHost::IShell* shell = Service<PluginHost::IShell>(SCENESET_CALLSIGN, engine, JSON_TIMEOUT);
    ASSERT_NE(shell, nullptr);
    if (shell) {
        // Try to get the IPlugin interface and call Initialize
        Exchange::IPlugin* plugin = dynamic_cast<Exchange::IPlugin*>(shell);
        if (plugin) {
            std::string result = plugin->Initialize(shell);
            EXPECT_EQ(result, "");
        } else {
            FAIL() << "SceneSet plugin does not implement IPlugin interface";
        }
        shell->Release();
    }
}

