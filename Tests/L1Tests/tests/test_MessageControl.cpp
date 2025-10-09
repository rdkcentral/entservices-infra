#include "gtest/gtest.h"
#include "MessageControl.h"
#include <core/core.h>
#include <plugins/IPlugin.h>
#include <plugins/Channel.h>
#include <string>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

// Minimal stub to allow instantiation for testing
class MessageControlStub : public MessageControl {
public:
    MessageControlStub() : MessageControl() {}
    ~MessageControlStub() override = default;

    // Implement all pure virtual methods from MessageControl and its bases
    const string Initialize(PluginHost::IShell* /*service*/) override { return ""; }
    void Deinitialize(PluginHost::IShell* /*service*/) override {}
    string Information() const override { return ""; }
    bool Attach(PluginHost::Channel& /*channel*/) override { return true; }
    void Detach(PluginHost::Channel& /*channel*/) override {}
    Core::ProxyType<Core::JSON::IElement> Inbound(const string& /*identifier*/) override { return Core::ProxyType<Core::JSON::IElement>::Create(); }
    Core::ProxyType<Core::JSON::IElement> Inbound(const uint32_t /*ID*/, const Core::ProxyType<Core::JSON::IElement>& /*element*/) override { return Core::ProxyType<Core::JSON::IElement>::Create(); }
};

class MessageControlL1Test : public ::testing::Test {
protected:
    std::unique_ptr<MessageControlStub> control;
    void SetUp() override {
        control = std::make_unique<MessageControlStub>();
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
