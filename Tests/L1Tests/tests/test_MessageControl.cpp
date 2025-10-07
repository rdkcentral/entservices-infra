#include "gtest/gtest.h"
#include "MessageControl.h"
#include "interfaces/IShell.h"
#include "interfaces/Channel.h"

// Dummy IShell implementation
class DummyShell : public WPEFramework::PluginHost::IShell {
public:
    DummyShell() = default;
    ~DummyShell() override = default;

    // IUnknown
    void AddRef() override {}
    uint32_t Release() override { return 0; }

    // IShell
    string ConfigLine() const override { return ""; }
    bool Background() const override { return false; }
    string VolatilePath() const override { return ""; }
    void Register(INotification* /*notification*/) override {}
    void Unregister(INotification* /*notification*/) override {}
    // Implement all pure virtual methods from IShell
    void* QueryInterface(const uint32_t) override { return nullptr; }
    void* Aquire(const uint32_t) override { return nullptr; }
    void Release(const uint32_t) override {}
    string CallSign() const override { return ""; }
    string Locator() const override { return ""; }
    string PersistentPath() const override { return ""; }
    string DataPath() const override { return ""; }
    string CachePath() const override { return ""; }
    string SystemPath() const override { return ""; }
    string ProxyStubPath() const override { return ""; }
    string VolatilePath(const string&) const override { return ""; }
    void* Root() override { return nullptr; }
    void* SubSystem() override { return nullptr; }
    void* Environment() override { return nullptr; }
    void* User() override { return nullptr; }
    void* Security() override { return nullptr; }
    void* Configuration() override { return nullptr; }
    void* Metadata() override { return nullptr; }
    void* Notification() override { return nullptr; }
    void* Dispatcher() override { return nullptr; }
    void* Controller() override { return nullptr; }
    void* RemoteConnection() override { return nullptr; }
    void* RemoteConnection(const uint32_t) override { return nullptr; }
    void* RemoteConnection(const string&) override { return nullptr; }
    void* RemoteConnection(const string&, const uint32_t) override { return nullptr; }
    void* RemoteConnection(const string&, const string&) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const uint32_t) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const uint32_t) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const uint32_t) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const string&) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const string&, const uint32_t) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const string&, const string&) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const string&, const string&, const uint32_t) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const string&, const string&, const string&) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const string&, const string&, const string&, const uint32_t) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const uint32_t) override { return nullptr; }
    void* RemoteConnection(const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&) override { return nullptr; }
    // ...add more stubs if IShell has more pure virtuals...
};

// Dummy Channel implementation
class DummyChannel : public WPEFramework::PluginHost::Channel {
public:
    DummyChannel()
        : WPEFramework::PluginHost::Channel(0, WPEFramework::Core::NodeId("127.0.0.1", 12345)) {}
    ~DummyChannel() override = default;

    uint32_t Id() const override { return 1; }
    // Implement all pure virtual methods from Channel if any
};

using namespace WPEFramework::Plugin;

class MessageControlL1Test : public ::testing::Test {
protected:
    std::unique_ptr<MessageControl> control;

    void SetUp() override {
        control = std::make_unique<MessageControl>();
    }

    void TearDown() override {
        control.reset();
    }
};

TEST_F(MessageControlL1Test, Initialization) {
    DummyShell shell;
    std::string result = control->Initialize(&shell);
    EXPECT_TRUE(result.empty() || result.find("could not be") == std::string::npos);
}

TEST_F(MessageControlL1Test, InformationReturnsString) {
    EXPECT_TRUE(control->Information().empty());
}

TEST_F(MessageControlL1Test, DeinitializeDoesNotCrash) {
    DummyShell shell;
    control->Deinitialize(&shell);
}

TEST_F(MessageControlL1Test, AttachDetachChannel) {
    DummyChannel channel;
    EXPECT_TRUE(control->Attach(channel));
    control->Detach(channel);
}
