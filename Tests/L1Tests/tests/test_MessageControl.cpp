#include "gtest/gtest.h"
#include "MessageControl.h"

// Dummy IShell implementation
class DummyShell : public WPEFramework::PluginHost::IShell {
public:
    DummyShell() = default;
    ~DummyShell() override = default;

    // Implement only methods required by IShell interface
    string ConfigLine() const override { return ""; }
    bool Background() const override { return false; }
    string VolatilePath() const override { return ""; }
    void Register(WPEFramework::PluginHost::IShell::INotification* /*notification*/) override {}
    void Unregister(WPEFramework::PluginHost::IShell::INotification* /*notification*/) override {}
    // ...add other required stubs if IShell has more pure virtuals...
};

// Dummy Channel implementation
class DummyChannel : public WPEFramework::PluginHost::Channel {
public:
    // Use a valid SOCKET and NodeId for base constructor
    DummyChannel()
        : WPEFramework::PluginHost::Channel(0, WPEFramework::Core::NodeId("127.0.0.1", 12345)) {}
    ~DummyChannel() override = default;

    uint32_t Id() const override { return 1; }
    // ...add other required stubs if Channel has more pure virtuals...
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
    DummyChannel channel;
    EXPECT_TRUE(control->Attach(channel));
    control->Detach(channel);
}
    control->Deinitialize(&shell);
}

TEST_F(MessageControlL1Test, AttachDetachChannel) {
    DummyChannel channel;
    EXPECT_TRUE(control->Attach(channel));
    control->Detach(channel);
}
