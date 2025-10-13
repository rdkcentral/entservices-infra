#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <core/core.h>
#include <plugins/plugins.h>
#include <interfaces/IMessageControl.h>
#include "MessageControl.h"
#include "Module.h"

namespace WPEFramework {

namespace TestMessageControl {

class MessageControlL1Test : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::MessageControl> plugin;

    void SetUp() override {
        plugin = Core::ProxyType<Plugin::MessageControl>::Create();
    }

    void TearDown() override {
        plugin.Release();
    }
};

TEST_F(MessageControlL1Test, Construction) {
    EXPECT_NE(nullptr, plugin.operator->());
}

TEST_F(MessageControlL1Test, InitialState) {
    EXPECT_TRUE(plugin->Information().empty());
}

TEST_F(MessageControlL1Test, EnableAllMessageTypes) {
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
    Exchange::IMessageControl::Control testControl;
    testControl.type = Exchange::IMessageControl::TRACING;
    testControl.category = "TestCategory";
    testControl.module = "TestModule"; 
    testControl.enabled = true;

    Core::hresult hr = plugin->Enable(
        testControl.type,
        testControl.category,
        testControl.module,
        testControl.enabled);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    ASSERT_NE(nullptr, controls);

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

TEST_F(MessageControlL1Test, ControlsIterator) {
    plugin->Enable(Exchange::IMessageControl::TRACING, "cat1", "mod1", true);
    plugin->Enable(Exchange::IMessageControl::LOGGING, "cat2", "mod2", true);

    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    Core::hresult hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    ASSERT_NE(nullptr, controls);

    controls->Release();
}

TEST_F(MessageControlL1Test, EnableAndDisableControl) {
    const string category = "category1";
    const string module = "testmodule";

    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        category,
        module,
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        category, 
        module,
        false);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, WebSocketSupport) {
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("test");
    EXPECT_TRUE(element.IsValid());
}

TEST_F(MessageControlL1Test, Initialize) {
    string jsonConfig = R"(
    {
        "console": true,
        "syslog": false,
        "abbreviated": true,
        "maxexportconnections": 5
    })";

    Core::JSONRPC::Message message;
    message.FromString(jsonConfig);
    
    string result = plugin->Initialize(nullptr);
    EXPECT_TRUE(result.empty());
}

TEST_F(MessageControlL1Test, AttachDetach) {
    class TestChannel : public PluginHost::Channel {
    public:
        uint32_t Id() const override { return 1; }
    };

    TestChannel channel;
    
    bool attached = plugin->Attach(channel);
    EXPECT_TRUE(attached);

    plugin->Detach(channel);
}

} // namespace TestMessageControl
} // namespace WPEFramework
