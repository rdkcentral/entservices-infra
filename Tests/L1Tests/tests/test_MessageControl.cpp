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
