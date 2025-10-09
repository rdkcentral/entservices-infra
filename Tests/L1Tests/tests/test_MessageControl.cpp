#define MODULE_NAME "MessageControlL1Tests"

#include <gtest/gtest.h>
#include "MessageControl.h"
#include "MessageOutput.h"
#include "MessageControlImplementation.cpp"
#include "MessageControl.h"
#include "MessageOutput.cpp"
#include "MessageControl.cpp"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

// Minimal IShell stub for initialization
class ShellStub : public PluginHost::IShell {
public:
    ShellStub() = default;
    ~ShellStub() override = default;

    // Implement all pure virtuals with dummy returns
    void AddRef() override {}
    uint32_t Release() override { return 0; }
    string ConfigLine() const override { return "{}"; }
    string VolatilePath() const override { return "/tmp/"; }
    bool Background() const override { return false; }
    void Register(RPC::IRemoteConnection::INotification*) override {}
    void Unregister(RPC::IRemoteConnection::INotification*) override {}
    // ...add more if your IShell has more pure virtuals...
};

// Minimal Channel stub for attach/detach
class DummyChannel : public PluginHost::Channel {
public:
    DummyChannel() : PluginHost::Channel(0, Core::NodeId("127.0.0.1", 12345)) {}
    ~DummyChannel() override = default;
    uint32_t Id() const override { return 123; }
    // ...add more if your Channel has more pure virtuals...
};

class MessageControlL1Test : public ::testing::Test {
protected:
    std::unique_ptr<MessageControl> control;
    void SetUp() override {
        control = std::make_unique<MessageControl>();
    }
};

TEST_F(MessageControlL1Test, Constructed) {
    EXPECT_NE(control.get(), nullptr);
}

TEST_F(MessageControlL1Test, InformationEmptyBeforeInit) {
    EXPECT_TRUE(control->Information().empty());
}

TEST_F(MessageControlL1Test, InboundCommandReturnsElementProxy) {
    auto element = control->Inbound("any");
    EXPECT_TRUE(element.IsValid());
}

TEST_F(MessageControlL1Test, InitializationAndDeinitialization) {
    ShellStub shell;
    std::string result = control->Initialize(&shell);
    EXPECT_TRUE(result.empty() || result.find("could not be") == std::string::npos);
    control->Deinitialize(&shell);
}

TEST_F(MessageControlL1Test, AttachDetachChannel) {
    DummyChannel channel;
    EXPECT_TRUE(control->Attach(channel));
    control->Detach(channel);
}

// L2 tests (more integration-like)
class MessageControlL2Test : public ::testing::Test {
protected:
    MessageControl* control;
    void SetUp() override {
        control = new MessageControl();
    }
    void TearDown() override {
        delete control;
    }
};

TEST_F(MessageControlL2Test, AttachDetachChannel) {
    DummyChannel channel;
    EXPECT_TRUE(control->Attach(channel));
    control->Detach(channel);
}

TEST_F(MessageControlL2Test, InboundCommandReturnsElement) {
    auto element = control->Inbound("test");
    EXPECT_TRUE(element.IsValid());
}

TEST_F(MessageControlL2Test, InboundReceivedReturnsElement) {
    auto dummyElement = Core::ProxyType<Core::JSON::IElement>::Create();
    auto result = control->Inbound(1, dummyElement);
    EXPECT_TRUE(result.IsValid());
}
