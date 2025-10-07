#include "gtest/gtest.h"
#include "MessageControl.h"
#include <interfaces/IPlugin.h>
#include <interfaces/Channel.h>

using namespace WPEFramework::Plugin;

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
    // Add stubs for any other pure virtual methods if required
};

// Dummy Channel implementation
class DummyChannel : public WPEFramework::PluginHost::Channel {
public:
    DummyChannel() : WPEFramework::PluginHost::Channel(nullptr, 1) {}
    ~DummyChannel() override = default;

    uint32_t Id() const override { return 1; }
    // Add stubs for any other pure virtual methods if required
};

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
    control->Deinitialize(&shell);
}

TEST_F(MessageControlL1Test, AttachDetachChannel) {
    DummyChannel channel;
    EXPECT_TRUE(control->Attach(channel));
    control->Detach(channel);
}
