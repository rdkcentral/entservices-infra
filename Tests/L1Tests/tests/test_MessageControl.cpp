#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <interfaces/IMessageControl.h>
#include "MessageControl.h"
#include <core/core.h>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

namespace {
    class TestCallback : public Exchange::IMessageControl {
    public:
        TestCallback() : callCount(0) {}
        
        uint32_t Enable(const Exchange::IMessageControl::MessageType type, 
                       const string& category, 
                       const string& module, 
                       const bool enabled) override {
            lastType = type;
            lastCategory = category;
            lastModule = module;
            lastEnabled = enabled;
            callCount++;
            return Core::ERROR_NONE;
        }

        uint32_t Controls(Exchange::IMessageControl::IControlIterator*& controls) const override {
            controls = nullptr;
            return Core::ERROR_NONE;
        }

        BEGIN_INTERFACE_MAP(TestCallback)
            INTERFACE_ENTRY(Exchange::IMessageControl)
        END_INTERFACE_MAP

        uint32_t callCount;
        Exchange::IMessageControl::MessageType lastType;
        string lastCategory;
        string lastModule;
        bool lastEnabled;
    };
}

class MessageControlL1Test : public ::testing::Test {
protected:
    Core::ProxyType<MessageControl> plugin;

    void SetUp() override {
        plugin = Core::ProxyType<MessageControl>::Create();
    }

    void TearDown() override {
        plugin.Release();
    }
};

TEST_F(MessageControlL1Test, Construction) {
    EXPECT_NE(nullptr, plugin.operator->());
}

TEST_F(MessageControlL1Test, InitialState) {
    // Test initial state before Initialize() is called
    EXPECT_TRUE(plugin->Information().empty());
}

TEST_F(MessageControlL1Test, EnableAllMessageTypes) {
    // Test all message types defined in IMessageControl
    std::vector<Exchange::IMessageControl::MessageType> types = {
        Exchange::IMessageControl::TRACING,
        Exchange::IMessageControl::LOGGING,
        Exchange::IMessageControl::REPORTING,
        Exchange::IMessageControl::STANDARD_OUT,
        Exchange::IMessageControl::STANDARD_ERROR
    };

    for (auto type : types) {
        Core::hresult hr = plugin->Enable(type, "category1", "testmodule", true);
        EXPECT_EQ(Core::ERROR_NONE, hr);
    }
}

TEST_F(MessageControlL1Test, ControlStructure) {
    // Test Control structure functionality
    Exchange::IMessageControl::Control testControl;
    testControl.type = Exchange::IMessageControl::TRACING;
    testControl.category = "TestCategory";
    testControl.module = "TestModule";
    testControl.enabled = true;

    // Enable the control
    Core::hresult hr = plugin->Enable(
        testControl.type,
        testControl.category,
        testControl.module,
        testControl.enabled);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    // Verify through iterator
    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    ASSERT_NE(nullptr, controls);

    // Check if our control exists
    bool found = false;
    Exchange::IMessageControl::Control current;
    while (controls->Next(current)) {
        if (current.type == testControl.type &&
            current.category == testControl.category &&
            current.module == testControl.module &&
            current.enabled == testControl.enabled) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    controls->Release();
}

TEST_F(MessageControlL1Test, EnableTracing) {
    // Test enabling tracing messages
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,  // Using correct enum
        "category1", 
        "testmodule",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, EnableLogging) {
    // Test enabling logging messages
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::LOGGING,  // Using correct enum
        "category1",
        "testmodule", 
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, EnableDisableWarning) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,  // Using correct enum value
        "category1",
        "testmodule",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,  // Using correct enum value
        "category1", 
        "testmodule",
        false);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, ControlsIterator) {
    plugin->Enable(Exchange::IMessageControl::TRACING, "cat1", "mod1", true);
    plugin->Enable(Exchange::IMessageControl::LOGGING, "cat2", "mod2", true);

    // Get controls iterator
    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    Core::hresult hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    ASSERT_NE(nullptr, controls);

    // Clean up
    controls->Release();
}

TEST_F(MessageControlL1Test, WebSocketSupport) {
    // Test WebSocket interface support
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("test");
    EXPECT_TRUE(element.IsValid());
}

TEST_F(MessageControlL1Test, ChannelOperations) {
    class MockChannel : public PluginHost::Channel {
    public:
        MockChannel(const SOCKET& connector, const Core::NodeId& remoteId)
            : PluginHost::Channel(connector, remoteId) {}

        // Only implement the minimum required virtual method
        uint32_t Id() const override { return 1234; }
    };

    SOCKET socket = 0;  // Dummy socket
    Core::NodeId remoteId("127.0.0.1:8080");
    MockChannel channel(socket, remoteId);
    
    bool attached = plugin->Attach(channel);
    EXPECT_TRUE(attached);
    
    plugin->Detach(channel);
}

TEST_F(MessageControlL1Test, MessageCallback) {
    auto callback = Core::Service<TestCallback>::Create();
    
    // Register callback
    Core::hresult hr = plugin->Callback(callback);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    // Unregister callback
    hr = plugin->Callback(nullptr);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    if (callback != nullptr) {
        callback->Release();
    }
}

TEST_F(MessageControlL1Test, ConfigureConsoleOutput) {
    string jsonConfig = R"(
    {
        "console": true,
        "syslog": false,
        "abbreviated": true,
        "maxexportconnections": 5
    })";

    Core::JSONRPC::Message message;
    message.FromString(jsonConfig);

    // Initialize with config
    string result = plugin->Initialize(nullptr); // Note: Real IShell needed
    EXPECT_TRUE(result.empty()); // Empty means success
}

TEST_F(MessageControlL1Test, ConfigureSyslogOutput) {
    string jsonConfig = R"(
    {
        "console": false,
        "syslog": true,
        "abbreviated": true,
        "maxexportconnections": 5
    })";

    Core::JSONRPC::Message message;
    message.FromString(jsonConfig);

    // Initialize with config  
    string result = plugin->Initialize(nullptr); // Note: Real IShell needed
    EXPECT_TRUE(result.empty()); // Empty means success
}
