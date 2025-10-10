#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <interfaces/IMessageControl.h>
#include "MessageControl.h"
#include <core/core.h>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

namespace {
    class TestCallback : public Core::IDispatch {
    public:
        TestCallback() : callCount(0) {}

        void Dispatch() override {
            // Required by IDispatch
        }

        void Message(const Core::Messaging::MessageInfo& metadata, const string& text) {
            callCount++;
            lastMetadata = metadata;
            lastText = text;
        }

        uint32_t callCount;
        Core::Messaging::MessageInfo lastMetadata;
        string lastText;
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

TEST_F(MessageControlL1Test, EnableTracing) {
    // Test enabling tracing messages
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::MessageType::TRACING,
        "category1", 
        "testmodule",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, EnableLogging) {
    // Test enabling logging messages
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::MessageType::LOGGING,
        "category1",
        "testmodule", 
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, EnableDisableWarning) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::MessageType::TRACE,  // Fixed enum
        "category1",
        "testmodule",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    hr = plugin->Enable(
        Exchange::IMessageControl::MessageType::TRACE,
        "category1", 
        "testmodule",
        false);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, ControlsIterator) {
    plugin->Enable(Exchange::IMessageControl::MessageType::TRACE, "cat1", "mod1", true);
    plugin->Enable(Exchange::IMessageControl::MessageType::LOGGING, "cat2", "mod2", true);

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
        MockChannel() : PluginHost::Channel("MockChannel") {}

        uint32_t Identifier() const override { return 1; }
        void Trigger() override {}
        string WebSocketProtocol() const override { return ""; }
    };

    MockChannel channel;
    
    // Test attach/detach
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
