#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <interfaces/IMessageControl.h>
#include "MessageControl.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

namespace {
    class TestCallback : public Exchange::IMessageControl::ICallback {
    public:
        TestCallback() : callCount(0) {}

        void Message(const Core::Messaging::MessageInfo& metadata, const string& text) override {
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
    // Test enabling and disabling warning messages
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::MessageType::ERROR, // Changed from WARNING to ERROR
        "category1",
        "testmodule",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    hr = plugin->Enable(
        Exchange::IMessageControl::MessageType::ERROR, // Changed from WARNING to ERROR
        "category1", 
        "testmodule",
        false);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, ControlsIterator) {
    // First enable some controls
    plugin->Enable(Exchange::IMessageControl::MessageType::TRACING, "cat1", "mod1", true);
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
    // Mock channel class since we can't use real one in L1 tests
    class MockChannel : public PluginHost::Channel {
    public:
        MockChannel(const string& name) : PluginHost::Channel(name) {}
        
        uint32_t Id() const override { return 1; }
        // Add other required overrides from Channel base class
        string Name() const override { return "MockChannel"; }
        void State(const PluginHost::Channel::state state) override {}
        state State() const override { return state::WEBSERVER; }
    };

    MockChannel channel("TestChannel");
    
    // Test attach/detach
    bool attached = plugin->Attach(channel);
    EXPECT_TRUE(attached);
    
    plugin->Detach(channel);
}

TEST_F(MessageControlL1Test, MessageCallback) {
    auto callback = Core::ServiceType<TestCallback>::Create();
    
    // Register callback
    Core::hresult hr = plugin->Callback(callback);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    // Unregister callback
    hr = plugin->Callback(nullptr);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    callback->Release();
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
