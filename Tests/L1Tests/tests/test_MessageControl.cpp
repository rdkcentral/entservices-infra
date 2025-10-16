#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MessageControl.h"
#include <core/core.h>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

class MessageControlL1Test : public ::testing::Test {
protected:
    Core::ProxyType<MessageControl> plugin;
    PluginHost::IShell* _shell;

    void SetUp() override {
        plugin = Core::ProxyType<MessageControl>::Create();
        _shell = new TestShell();
        plugin->Initialize(_shell);
    }

    void TearDown() override {
        if (plugin.IsValid()) {
            plugin->Deinitialize(_shell);
        }
        if (_shell != nullptr) {
            delete _shell;
            _shell = nullptr;
        }
        plugin.Release();
    }

    class TestShell : public PluginHost::IShell {
    public:
        string ConfigLine() const override { 
            return R"({"console":true,"syslog":false,"remote":{"port":8899,"binding":"0.0.0.0"}})"; 
        }
        string VolatilePath() const override { return "/tmp/"; }
        bool Background() const override { return false; }
        string Accessor() const override { return ""; }
        string WebPrefix() const override { return ""; }
        string Callsign() const override { return "MessageControl"; }
        string HashKey() const override { return ""; }
        string PersistentPath() const override { return "/tmp/"; }
        string DataPath() const override { return "/tmp/"; }
        string ProxyStubPath() const override { return "/tmp/"; }
        string SystemPath() const override { return "/tmp/"; }
        string PluginPath() const override { return "/tmp/"; }
        string SystemRootPath() const override { return "/tmp/"; }
        string Locator() const override { return ""; }
        string ClassName() const override { return ""; }
        string Versions() const override { return ""; }
        string Model() const override { return ""; }
        
        state State() const override { return state::ACTIVATED; }
        bool Resumed() const override { return true; }
        Core::hresult Resumed(const bool) override { return Core::ERROR_NONE; }
        reason Reason() const override { return reason::REQUESTED; }
        
        PluginHost::ISubSystem* SubSystems() override { return nullptr; }
        startup Startup() const override { return startup::ACTIVATED; }
        Core::hresult Startup(const startup) override { return Core::ERROR_NONE; }
        ICOMLink* COMLink() override { return nullptr; }
        void* QueryInterface(const uint32_t) override { return nullptr; }
        
        void AddRef() const override {
            Core::InterlockedIncrement(_refCount);
        }

        uint32_t Release() const override {
            if (Core::InterlockedDecrement(_refCount) == 0) {
                delete this;
                return 0;
            }
            return _refCount;
        }
        
        void EnableWebServer(const string& URLPath, const string& fileSystemPath) override {}
        void DisableWebServer() override {}
        Core::hresult SystemRootPath(const string& systemRootPath) override { return Core::ERROR_NONE; }
        string Substitute(const string& input) const override { return input; }
        Core::hresult ConfigLine(const string& config) override { return Core::ERROR_NONE; }
        Core::hresult Metadata(string& info) const override { return Core::ERROR_NONE; }
        bool IsSupported(const uint8_t version) const override { return true; }
        void Notify(const string& message) override {}
        void Register(PluginHost::IPlugin::INotification* sink) override {}
        void Unregister(PluginHost::IPlugin::INotification* sink) override {}
        void* QueryInterfaceByCallsign(const uint32_t id, const string& name) override { return nullptr; }
        Core::hresult Activate(const reason why) override { return Core::ERROR_NONE; }
        Core::hresult Deactivate(const reason why) override { return Core::ERROR_NONE; }
        Core::hresult Unavailable(const reason why) override { return Core::ERROR_NONE; }
        Core::hresult Hibernate(const uint32_t timeout) override { return Core::ERROR_NONE; }
        uint32_t Submit(const uint32_t id, const Core::ProxyType<Core::JSON::IElement>& response) override { return Core::ERROR_NONE; }
        
        mutable uint32_t _refCount;
        
        TestShell() : _refCount(1) {}
    };
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
    TestShell* shell = new TestShell();
    ASSERT_NE(nullptr, shell);

    string result = plugin->Initialize(shell);
    EXPECT_TRUE(result.empty());
    
    plugin->Deinitialize(shell);
    shell->Release();  // Use Release() instead of delete
}

TEST_F(MessageControlL1Test, AttachDetachChannel) {
    class TestChannel : public PluginHost::Channel {
    public:
        TestChannel() 
            : PluginHost::Channel(0, Core::NodeId("127.0.0.1", 8899))
            , _baseTime(static_cast<uint32_t>(Core::Time::Now().Ticks())) {
            State(static_cast<ChannelState>(1), true);
        }
        
        void LinkBody(Core::ProxyType<PluginHost::Request>& request) override {}
        void Received(Core::ProxyType<PluginHost::Request>& request) override {}
        void Send(const Core::ProxyType<Web::Response>& response) override {}
        uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize) override { return maxSendSize; }
        uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize) override { return receivedSize; }
        void StateChange() override {}
        void Send(const Core::ProxyType<Core::JSON::IElement>& element) override {}
        Core::ProxyType<Core::JSON::IElement> Element(const string& identifier) override { 
            return Core::ProxyType<Core::JSON::IElement>(); 
        }
        void Received(Core::ProxyType<Core::JSON::IElement>& element) override {}
        void Received(const string& text) override {}

    private:
        uint32_t _baseTime;
    };

    TestChannel channel;
    EXPECT_TRUE(plugin->Attach(channel));
    plugin->Detach(channel);
}

TEST_F(MessageControlL1Test, MultipleAttachDetach) {
    class TestChannel : public PluginHost::Channel {
    public:
        TestChannel(uint32_t id) 
            : PluginHost::Channel(0, Core::NodeId("127.0.0.1", 8899))
            , _baseTime(static_cast<uint32_t>(Core::Time::Now().Ticks()))
            , _id(id) {
            State(static_cast<ChannelState>(1), true);
        }
        
        void LinkBody(Core::ProxyType<PluginHost::Request>& request) override {}
        void Received(Core::ProxyType<PluginHost::Request>& request) override {}
        void Send(const Core::ProxyType<Web::Response>& response) override {}
        uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize) override { return maxSendSize; }
        uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize) override { return receivedSize; }
        void StateChange() override {}
        void Send(const Core::ProxyType<Core::JSON::IElement>& element) override {}
        Core::ProxyType<Core::JSON::IElement> Element(const string& identifier) override { 
            return Core::ProxyType<Core::JSON::IElement>(); 
        }
        void Received(Core::ProxyType<Core::JSON::IElement>& element) override {}
        void Received(const string& text) override {}
        
    private:
        uint32_t _baseTime;
        uint32_t _id;
    };

    TestChannel channel1(1);
    TestChannel channel2(2);
    TestChannel channel3(3);

    EXPECT_TRUE(plugin->Attach(channel1));
    EXPECT_TRUE(plugin->Attach(channel2));
    EXPECT_TRUE(plugin->Attach(channel3));

    plugin->Detach(channel2);
    plugin->Detach(channel1);
    plugin->Detach(channel3);
}
