#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MessageControl.h"
#include <core/core.h>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

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
/*
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
*/
TEST_F(MessageControlL1Test, EnableTracing) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category1", 
        "testmodule",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, EnableLogging) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::LOGGING,
        "category1",
        "testmodule", 
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, EnableDisableWarning) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category1",
        "testmodule",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category1", 
        "testmodule",
        false);
    EXPECT_EQ(Core::ERROR_NONE, hr);
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

TEST_F(MessageControlL1Test, WebSocketSupport) {
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("test");
    EXPECT_TRUE(element.IsValid());
}

TEST_F(MessageControlL1Test, Initialize) {
    string basicConfig = R"({"console":false,"syslog":false,"filename":""})";
    
    MockShell mockService;
    EXPECT_CALL(mockService, ConfigLine())
        .WillRepeatedly(::testing::Return(basicConfig));
    EXPECT_CALL(mockService, Background())
        .WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(mockService, VolatilePath())
        .WillRepeatedly(::testing::Return("/tmp/"));

    string result = plugin->Initialize(&mockService);
    EXPECT_TRUE(result.empty());
}

/*
TEST_F(MessageControlL1Test, NetworkConfig) {
    string jsonConfig = R"({"port":2200, "binding":"127.0.0.1"})";
    Core::JSON::String config;
    config.FromString(jsonConfig);
    
    EXPECT_EQ(2200, plugin->_config.Remote.Port.Value());
    EXPECT_STREQ("127.0.0.1", plugin->_config.Remote.Binding.Value().c_str());
}

TEST_F(MessageControlL1Test, OutputDirector) {
    string testFileName = "/tmp/test.log";
    
    auto* fileOutput = new Publishers::FileOutput(
        Core::Messaging::MessageInfo::abbreviate::ABBREVIATED, 
        testFileName);
    plugin->Announce(fileOutput);

    std::ifstream testFile(testFileName);
    EXPECT_TRUE(testFile.good()) << "File " << testFileName << " was not created";
    testFile.close();
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category1",
        "testmodule",
        true); 
    EXPECT_EQ(Core::ERROR_NONE, hr);

    plugin->Deinitialize(nullptr);
    std::ifstream checkFile(testFileName);
    EXPECT_FALSE(checkFile.good()) << "File " << testFileName << " was not cleaned up";
}
*/
TEST_F(MessageControlL1Test, EnableMultipleCategories) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category1", 
        "module1",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category2",
        "module1", 
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, EnableMultipleModules) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::LOGGING,
        "category1",
        "module1",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    hr = plugin->Enable(
        Exchange::IMessageControl::LOGGING,
        "category1",
        "module2",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

TEST_F(MessageControlL1Test, EnableAndVerifyControls) {
    plugin->Enable(Exchange::IMessageControl::STANDARD_OUT, "cat1", "mod1", true);
    
    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    Core::hresult hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    ASSERT_NE(nullptr, controls);

    Exchange::IMessageControl::Control current;
    bool found = false;
    while (controls->Next(current)) {
        if (current.type == Exchange::IMessageControl::STANDARD_OUT &&
            current.category == "cat1" &&
            current.module == "mod1") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    controls->Release();
}
