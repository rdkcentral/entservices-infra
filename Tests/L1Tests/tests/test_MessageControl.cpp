#define MODULE_NAME MessageControlL1Tests

#include <gtest/gtest.h>
#include "MessageControl.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

// Minimal stub for Channel (only for Attach/Detach test, if needed)
class DummyChannel : public PluginHost::Channel {
public:
    DummyChannel() : PluginHost::Channel(0, Core::NodeId("127.0.0.1", 12345)) {}
    ~DummyChannel() override = default;
    uint32_t Id() const override { return 42; }
    // ...implement other pure virtuals if needed...
};

class MessageControlTest : public ::testing::Test {
protected:
    std::unique_ptr<MessageControl> control;
    void SetUp() override {
        control = std::make_unique<MessageControl>();
    }
    void TearDown() override {
        control.reset();
    }
};

TEST_F(MessageControlTest, Constructed) {
    EXPECT_NE(control.get(), nullptr);
}

TEST_F(MessageControlTest, InformationEmptyBeforeInit) {
    EXPECT_TRUE(control->Information().empty());
}

TEST_F(MessageControlTest, InboundCommandReturnsElementProxy) {
    auto element = control->Inbound("any");
    EXPECT_TRUE(element.IsValid());
}

// If you want to test Attach/Detach, you must provide a fully implemented DummyChannel
// Uncomment below if your DummyChannel is complete
/*
TEST_F(MessageControlTest, AttachDetachChannel) {
    DummyChannel channel;
    EXPECT_TRUE(control->Attach(channel));
    control->Detach(channel);
}
*/

// ...add more tests for other public MessageControl methods as needed...

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
