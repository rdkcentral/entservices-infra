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
/*
TEST_F(MessageControlL1Test, Initialize) {
    class MinimalShell : public PluginHost::IShell {
    public:
        string ConfigLine() const override { return "{}"; }
        string VolatilePath() const override { return "/tmp/"; }
        bool Background() const override { return false; }
        uint32_t Release() const override { return 0; }
        uint32_t AddRef() const override { return 1; }
    };

    MinimalShell* shell = new MinimalShell();
    string result = plugin->Initialize(shell);
    EXPECT_TRUE(result.empty());
}

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
/*
TEST_F(MessageControlL1Test, EnableAndVerifyControls) {
    plugin->Enable(Exchange::IMessageControl::STANDARD_OUT, "cat1", "mod1", true);
    
    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    Core::hresult hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    ASSERT_NE(nullptr, controls);

    bool found = false;
    Exchange::IMessageControl::Control current;
    while (controls->Next(current)) {
        found = true;
    }
    EXPECT_TRUE(found);
    controls->Release();
}
*/
TEST_F(MessageControlL1Test, EnableAndDisableMultiple) {
    plugin->Enable(Exchange::IMessageControl::STANDARD_OUT, "cat1", "mod1", true);
    plugin->Enable(Exchange::IMessageControl::STANDARD_ERROR, "cat2", "mod2", true);
    plugin->Enable(Exchange::IMessageControl::STANDARD_OUT, "cat1", "mod1", false);
    plugin->Enable(Exchange::IMessageControl::STANDARD_ERROR, "cat2", "mod2", false);
    
    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    Core::hresult hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    controls->Release();
}

TEST_F(MessageControlL1Test, InboundCommunication) {
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("command");
    EXPECT_TRUE(element.IsValid());

    Core::ProxyType<Core::JSON::IElement> response = 
        plugin->Inbound(1234, element);
    EXPECT_TRUE(response.IsValid());
}

TEST_F(MessageControlL1Test, WebSocketInboundFlow) {
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("test");
    EXPECT_TRUE(element.IsValid());
    
    Core::ProxyType<Core::JSON::IElement> response = plugin->Inbound(1234, element);
    EXPECT_TRUE(response.IsValid());
}
/*
TEST_F(MessageControlL1Test, MessageOutputControl) {
    plugin->Enable(Exchange::IMessageControl::STANDARD_OUT, "test", "module1", true);
    plugin->Enable(Exchange::IMessageControl::STANDARD_ERROR, "test", "module2", true);
    
    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    Core::hresult hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    ASSERT_NE(nullptr, controls);

    int count = 0;
    Exchange::IMessageControl::Control current;
    while (controls->Next(current)) {
        count++;
    }
    EXPECT_GT(count, 0);
    controls->Release();
}
*/
TEST_F(MessageControlL1Test, VerifyMultipleEnableDisable) {
    for(auto type : {
        Exchange::IMessageControl::TRACING,
        Exchange::IMessageControl::LOGGING,
        Exchange::IMessageControl::REPORTING}) {

        Core::hresult hr = plugin->Enable(type, "category1", "testmodule", true);
        EXPECT_EQ(Core::ERROR_NONE, hr);
        
        Exchange::IMessageControl::IControlIterator* controls = nullptr;
        hr = plugin->Controls(controls);
        EXPECT_EQ(Core::ERROR_NONE, hr);
        ASSERT_NE(nullptr, controls);
        controls->Release();

        hr = plugin->Enable(type, "category1", "testmodule", false);
        EXPECT_EQ(Core::ERROR_NONE, hr);
    }
}

TEST_F(MessageControlL1Test, InboundMessageFlow) {
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("command");
    EXPECT_TRUE(element.IsValid());

    for(uint32_t id = 1; id < 4; id++) {
        Core::ProxyType<Core::JSON::IElement> response = plugin->Inbound(id, element);
        EXPECT_TRUE(response.IsValid());
    }
}

TEST_F(MessageControlL1Test, InitializeDeinitialize) {
    class MockShell : public PluginHost::IShell {
    public:
        MOCK_CONST_METHOD0(ConfigLine, string());
        MOCK_CONST_METHOD0(VolatilePath, string());
        MOCK_CONST_METHOD0(Background, bool());
        MOCK_CONST_METHOD0(Accessor, string());
        MOCK_CONST_METHOD0(WebPrefix, string());
        MOCK_CONST_METHOD0(Locator, string());
        MOCK_CONST_METHOD0(ClassName, string());
        MOCK_CONST_METHOD0(Versions, string());
        MOCK_CONST_METHOD0(Callsign, string());
        MOCK_CONST_METHOD0(PersistentPath, string());
        MOCK_CONST_METHOD0(DataPath, string());
        MOCK_CONST_METHOD0(ProxyStubPath, string());
        MOCK_CONST_METHOD0(SystemPath, string());
        MOCK_CONST_METHOD0(PluginPath, string());
        MOCK_CONST_METHOD0(SystemRootPath, string());
        MOCK_METHOD1(SystemRootPath, Core::hresult(const string&));
        MOCK_CONST_METHOD0(Startup, startup());
        MOCK_METHOD1(Startup, Core::hresult(startup));
        MOCK_CONST_METHOD1(Substitute, string(const string&));
        MOCK_CONST_METHOD0(Resumed, bool());
        MOCK_METHOD1(Resumed, Core::hresult(bool));
        MOCK_CONST_METHOD0(HashKey, string());
        MOCK_METHOD1(ConfigLine, Core::hresult(const string&));
        MOCK_CONST_METHOD1(Metadata, Core::hresult(string&));
        MOCK_METHOD0(SubSystems, ISubSystem*());
        MOCK_METHOD1(Notify, void(const string&));
        MOCK_CONST_METHOD0(State, state());
        MOCK_CONST_METHOD0(AddRef, void());
        MOCK_METHOD0(Release, uint32_t());
        MOCK_METHOD1(QueryInterface, void*(uint32_t));
        MOCK_METHOD1(EnableWebServer, void(const string&, const string&));
        MOCK_METHOD0(DisableWebServer, void());
        MOCK_METHOD1(Register, void(IPlugin::INotification*));
        MOCK_METHOD1(Unregister, void(IPlugin::INotification*));
    };

    MockShell shell;

    EXPECT_CALL(shell, ConfigLine())
        .WillOnce(::testing::Return(R"({"console":true,"syslog":false,"filename":""})"));
    EXPECT_CALL(shell, VolatilePath())
        .WillOnce(::testing::Return("/tmp/"));
    EXPECT_CALL(shell, Background())
        .WillOnce(::testing::Return(false));
    
    string result = plugin->Initialize(&shell);
    EXPECT_TRUE(result.empty());
    
    plugin->Deinitialize(&shell);
}

