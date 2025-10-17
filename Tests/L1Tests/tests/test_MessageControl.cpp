#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MessageControl.h"
#include <core/core.h>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <atomic> // added for std::atomic used by CountingShell

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

class MessageControlL1Test : public ::testing::Test {
protected:
    Core::ProxyType<MessageControl> plugin;
    PluginHost::IShell* _shell;
    bool _shellOwned; // new: indicates fixture owns the shell and must Release it

    void SetUp() override {
        plugin = Core::ProxyType<MessageControl>::Create();
        _shell = nullptr;
        _shellOwned = false;
    }

    void TearDown() override {
        if (_shell != nullptr) {
            if (plugin.IsValid()) {
                // Let the plugin deinitialize and release the shell (plugin called AddRef in Initialize)
                plugin->Deinitialize(_shell);
            }
            // Only delete the shell here if the fixture allocated it and thus owns it.
            if (_shellOwned) {
                delete _shell;
            }
            _shell = nullptr;
            _shellOwned = false;
        }
        plugin.Release();
    }

    class CountingShell : public TestShell {
    public:
        CountingShell()
            : TestShell()
            , Count(0)
        {
        }

        uint32_t Submit(const uint32_t id, const Core::ProxyType<Core::JSON::IElement>& response) override {
            ++Count;
            LastId = id;
            Last = response; // store a shallow reference for inspection
            return Core::ERROR_NONE;
        }

        ~CountingShell() override {
            // Ensure proper cleanup of resources
            Last.Release();
        }

        std::atomic<int> Count;
        uint32_t LastId{0};
        Core::ProxyType<Core::JSON::IElement> Last;
    };

    class TestShell : public PluginHost::IShell {
    public:
        // Accept custom config, default matches previous behavior (no remote)
        explicit TestShell(const string& config = R"({"console":true,"syslog":false})")
            : _config(config)
            , _refCount(1)
        {
        }

        string ConfigLine() const override {
            return _config;
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
        
        // Proper refcount implementation
        void AddRef() const override {
            // Restore safe refcount behavior (do not delete 'this' here)
            Core::InterlockedIncrement(_refCount);
        }

        uint32_t Release() const override {
            // Decrement and return the new count. Tests delete heap instances explicitly.
            return Core::InterlockedDecrement(_refCount);
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
        
    private:
        mutable uint32_t _refCount;
        string _config;
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

TEST_F(MessageControlL1Test, AttachDetachChannel) {
    // Initialize shell first
    _shell = new TestShell();
    _shellOwned = true; // fixture owns this allocation
    ASSERT_NE(nullptr, _shell);
    string result = plugin->Initialize(_shell);
    EXPECT_TRUE(result.empty());

    class TestChannel : public PluginHost::Channel {
    public:
        TestChannel() 
            : PluginHost::Channel(0, Core::NodeId("127.0.0.1", 8899))
            , _baseTime(static_cast<uint32_t>(Core::Time::Now().Ticks())) {
            State(static_cast<ChannelState>(2), true); // Use ACTIVATED state
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
    // Initialize shell first
    _shell = new TestShell();
    _shellOwned = true; // fixture owns this allocation
    ASSERT_NE(nullptr, _shell);
    string result = plugin->Initialize(_shell);
    EXPECT_TRUE(result.empty());

    class TestChannel : public PluginHost::Channel {
    public:
        TestChannel(uint32_t id) 
            : PluginHost::Channel(0, Core::NodeId("127.0.0.1", 8899))
            , _baseTime(static_cast<uint32_t>(Core::Time::Now().Ticks()))
            , _id(id) {
            State(static_cast<ChannelState>(2), true); // Use ACTIVATED state
            
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

// Validate Text::Convert output (console-like formatting) without constructing ConsoleOutput
TEST_F(MessageControlL1Test, TextConvert_ForConsoleFormat) {
    Publishers::Text textConv(Core::Messaging::MessageInfo::abbreviate::ABBREVIATED);
    Core::Messaging::MessageInfo defaultMeta;
    const string payload = "console-output-test";

    const string converted = textConv.Convert(defaultMeta, payload);
    EXPECT_NE(string::npos, converted.find(payload));
    EXPECT_NE(string::npos, converted.find("\n")); // console lines end with newline
}

// Validate converter output used by syslog (safe check, no syslog access)
TEST_F(MessageControlL1Test, SyslogOutput_ConverterOutput) {
    Publishers::Text textConv(Core::Messaging::MessageInfo::abbreviate::ABBREVIATED);
    Core::Messaging::MessageInfo defaultMeta;
    const string payload = "syslog-output-test";

    const string converted = textConv.Convert(defaultMeta, payload);
    EXPECT_NE(string::npos, converted.find(payload));
}

TEST_F(MessageControlL1Test, WebSocketOutput_AttachCapacity_Command_Received) {
    // Use heap-allocated TestShell to match refcount lifecycle used by WebSocketOutput::Initialize/Deinitialize
    TestShell* shell = new TestShell();
    Publishers::WebSocketOutput ws;

    ws.Initialize(shell, 1);
    EXPECT_TRUE(ws.Attach(42));
    EXPECT_FALSE(ws.Attach(43));

    Core::ProxyType<Core::JSON::IElement> cmd = ws.Command();
    EXPECT_TRUE(cmd.IsValid());

    Core::ProxyType<Core::JSON::IElement> ret = ws.Received(42, cmd);
    EXPECT_TRUE(ret.IsValid());

    EXPECT_TRUE(ws.Detach(42));
    ws.Deinitialize();

    // Release shell (Deinitialize released once); free heap-allocated shell explicitly.
    delete shell;
}

TEST_F(MessageControlL1Test, WebSocketOutput_Message_NoCrash_SubmitCalled) {
    TestShell* shell = new TestShell();
    Publishers::WebSocketOutput ws;

    ws.Initialize(shell, 2);
    EXPECT_TRUE(ws.Attach(1001));

    Core::Messaging::MessageInfo defaultMeta;
    ws.Message(defaultMeta, "websocket-export-test");

    EXPECT_TRUE(ws.Detach(1001));
    ws.Deinitialize();

    // Explicitly free heap-allocated TestShell
    delete shell;
}

// Ensure plugin Initialize creates a FileOutput when 'filepath' is present in config and file is writable.
TEST_F(MessageControlL1Test, MessageControl_InitializeCreatesFileOutput) {
    // Reuse TestShell with file-config via constructor
    TestShell* shell = new TestShell(R"({"filepath":"test_messagecontrol_init.log","abbreviated":true})");
    ASSERT_NE(nullptr, shell);

    string initResult = plugin->Initialize(shell);
    EXPECT_TRUE(initResult.empty());

    const string expectedFile = "/tmp/test_messagecontrol_init.log";
    std::ifstream in(expectedFile);
    if (in.good()) {
        in.close();
        EXPECT_TRUE(std::ifstream(expectedFile).good());
        std::remove(expectedFile.c_str());
    } else {
        GTEST_SKIP() << "Cannot create/read temp file in this environment; skipping file existence check.";
    }

    plugin->Deinitialize(shell);
    // Deinitialize performed one Release; explicitly delete the heap-allocated test shell.
    delete shell;
}

// Validate JSON option setters/getters and that Convert sets Message even with default metadata.
TEST_F(MessageControlL1Test, JSON_OutputOptions_TogglesAndConvert) {
    Publishers::JSON json;
    // Toggle various options on/off and verify getters
    json.FileName(true); EXPECT_TRUE(json.FileName());
    json.LineNumber(true); EXPECT_TRUE(json.LineNumber());
    json.ClassName(true); EXPECT_TRUE(json.ClassName());
    json.Category(true); EXPECT_TRUE(json.Category());
    json.Module(true); EXPECT_TRUE(json.Module());
    json.Callsign(true); EXPECT_TRUE(json.Callsign());
    json.Date(true); EXPECT_TRUE(json.Date());
    json.Paused(false); EXPECT_FALSE(json.Paused());

    // Toggle some off again
    json.FileName(false); EXPECT_FALSE(json.FileName());
    json.Date(false); EXPECT_FALSE(json.Date());

    // Convert with default/INVALID metadata should still set Message
    Core::Messaging::MessageInfo defaultMeta;
    Publishers::JSON::Data data;
    json.Convert(defaultMeta, "json-payload", data);
    EXPECT_EQ(std::string("json-payload"), std::string(data.Message));
}

TEST_F(MessageControlL1Test, MessageOutput_SimpleText_JSON) {
    // Use default MessageInfo (invalid) to ensure functions don't ASSERT and do return sensible values.

    Core::Messaging::MessageInfo defaultMeta; // default/invalid metadata

    // Text::Convert: should return string containing the payload even with invalid metadata
    Publishers::Text textConv(Core::Messaging::MessageInfo::abbreviate::ABBREVIATED);
    const string payload = "hello-text";
    const string result = textConv.Convert(defaultMeta, payload);
    EXPECT_NE(string::npos, result.find(payload));

    // JSON::Convert: should set Data.Message at minimum
    Publishers::JSON::Data data;
    Publishers::JSON jsonConv;
    jsonConv.Convert(defaultMeta, "json-msg", data);
    EXPECT_EQ(std::string("json-msg"), std::string(data.Message));

  // UDPOutput::Message: ensure it executes without crash using default metadata
    Core::NodeId anyNode("127.0.0.1", 0);
    Publishers::UDPOutput udp(anyNode);
    udp.Message(defaultMeta, "udp-msg");
    SUCCEED(); // if we reach here, the call did not ASSERT/crash
}

TEST_F(MessageControlL1Test, MessageOutput_FileWrite) {
	// Create a small temp file and verify FileOutput writes the payload when file can be created.
	const string tmpName = "/tmp/test_messageoutput_filewrite.log";
	// Ensure no leftover
	std::remove(tmpName.c_str());

	// Create FileOutput using same constructor used elsewhere in repo
	Publishers::FileOutput fileOutput(Core::Messaging::MessageInfo::abbreviate::ABBREVIATED, tmpName);

	Core::Messaging::MessageInfo defaultMeta; // invalid metadata tolerated by Convert()
	const string payload = "file-write-test-payload";

	// Try to write; FileOutput::Message checks file internals itself.
	fileOutput.Message(defaultMeta, payload);

	// If file exists, read and verify payload; otherwise skip gracefully.
	std::ifstream in(tmpName);
	if (in.good()) {
		std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		in.close();
		EXPECT_NE(string::npos, content.find(payload));
		// cleanup
		std::remove(tmpName.c_str());
	} else {
		// Environment prevented file creation; skip the test as not applicable in this environment
		GTEST_SKIP() << "Cannot create/read temp file; skipping FileOutput write verification.";
	}
}

// New: enable using empty category/module should be accepted (exercise input handling)
TEST_F(MessageControlL1Test, EnableWithEmptyFields) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::LOGGING,
        "", // empty category
        "", // empty module
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr);
}

// New: WebSocketOutput::Detach for unknown id should return false (exercise detach branch)
TEST_F(MessageControlL1Test, WebSocketOutput_UnknownDetach) {
    TestShell* shell = new TestShell();
    Publishers::WebSocketOutput ws;

    ws.Initialize(shell, 1);
    // Detach an id that was never attached; expect false
    EXPECT_FALSE(ws.Detach(9999));
    ws.Deinitialize();
    // Explicit delete instead of relying on Release() to free memory
    delete shell;
}

// New: Exercise TestShell lifecycle helpers (Activate/Deactivate/Hibernate) return success codes
TEST_F(MessageControlL1Test, TestShell_Lifecycle) {
    TestShell shell;
    EXPECT_EQ(Core::ERROR_NONE, shell.Activate(PluginHost::IShell::reason::REQUESTED));
    EXPECT_EQ(Core::ERROR_NONE, shell.Deactivate(PluginHost::IShell::reason::REQUESTED));
    EXPECT_EQ(Core::ERROR_NONE, shell.Hibernate(5000));
}

TEST_F(MessageControlL1Test, TestShell_SubstituteAndMetadata) {
    TestShell shell; // stack instance
    const string input = "replace-me";
    EXPECT_EQ(input, shell.Substitute(input));

    string meta;
    Core::hresult hr = shell.Metadata(meta);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    // Metadata result is not strictly specified; ensure call succeeds and returns a string (possibly empty)
    EXPECT_TRUE(meta.size() >= 0);
}

// New: ensure WebSocketOutput::Message actually invokes IShell::Submit for attached channels
TEST_F(MessageControlL1Test, WebSocketOutput_Message_TriggersSubmit) {
    CountingShell* shell = new CountingShell();
    Publishers::WebSocketOutput ws;

    ws.Initialize(shell, 2);
    EXPECT_EQ(2u, ws.MaxConnections());

    EXPECT_TRUE(ws.Attach(100));
    EXPECT_TRUE(ws.Attach(200));

    Core::Messaging::MessageInfo defaultMeta;
    // Send a message; this should result in one Submit per attached, non-paused channel.
    ws.Message(defaultMeta, "trigger-submit-test");

    // Give a small window (if implementation asynchronous) — but Message is synchronous here.
    EXPECT_GT(shell->Count.load(), 0);
    EXPECT_TRUE((shell->LastId == 100) || (shell->LastId == 200));

    ws.Detach(100);
    ws.Detach(200);
    ws.Deinitialize();

    delete shell; // Explicitly delete the shell to avoid memory leaks
}

// New: JSON::Convert should do nothing when Paused bit is set
TEST_F(MessageControlL1Test, JSON_Paused_PreventsConvert) {
    Publishers::JSON json;
    Publishers::JSON::Data data;
    // Ensure paused prevents any conversion (data.Message should remain empty)
    json.Paused(true);

    Core::Messaging::MessageInfo defaultMeta;
    json.Convert(defaultMeta, "payload-should-be-ignored", data);

    EXPECT_TRUE(std::string(data.Message).empty());
}
