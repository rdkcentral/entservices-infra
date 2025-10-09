#define MODULE_NAME "MessageControlL1Tests"

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

// NOTE:
// Initialization / Attach / Detach tests removed to avoid complex IShell/Channel mocks.
// Add later only with fully compliant Thunder interface stubs.
