#define MODULE_NAME MessageControlL1Tests

#include <gtest/gtest.h>
#include "MessageControl.h"

using namespace WPEFramework::Plugin;

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

// NOTE: The following tests require valid IShell and Channel objects.
// If you want to cover these, you must provide complete mocks for IShell and Channel.
// Otherwise, keep only the above tests for error-free compilation.
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

// ...end of file...
