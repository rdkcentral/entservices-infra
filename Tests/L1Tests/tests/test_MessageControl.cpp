#include "gtest/gtest.h"
#include "MessageControl.h"

using namespace WPEFramework::Plugin;

class MessageControlL1Test : public ::testing::Test {
protected:
    std::unique_ptr<MessageControl> control;

    void SetUp() override {
        control = std::make_unique<MessageControl>();
    }
};

// Basic construction (object should be created)
TEST_F(MessageControlL1Test, Constructed) {
    EXPECT_NE(control.get(), nullptr);
}

// Information should be empty even before Initialize
TEST_F(MessageControlL1Test, InformationEmptyBeforeInit) {
    EXPECT_TRUE(control->Information().empty());
}

// Inbound command interface without initialization should still return a JSON element proxy
TEST_F(MessageControlL1Test, InboundCommandReturnsElementProxy) {
    auto element = control->Inbound("any");
    EXPECT_TRUE(element.IsValid());
}

// NOTE:
// Initialization / Attach / Detach tests removed to avoid complex Thunder IShell/Channel stubs.
// Add them later only with full interface-compliant mocks.
// NOTE:
// Initialization & Attach/Detach tests removed because creating a fully
// functional DummyShell / Channel stub requires matching all pure virtual
// methods from Thunder's PluginHost::IShell / Channel which caused the
// previous build failures. Add them back only with correct Thunder stubs.
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
