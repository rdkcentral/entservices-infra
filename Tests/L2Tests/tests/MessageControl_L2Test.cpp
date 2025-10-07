#include "gtest/gtest.h"
#include "MessageControl.h"

// Dummy Channel implementation
class DummyChannel : public WPEFramework::PluginHost::Channel {
public:
    uint32_t Id() const override { return 42; }
    // ...other required dummy methods...
};

using namespace WPEFramework::Plugin;

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
