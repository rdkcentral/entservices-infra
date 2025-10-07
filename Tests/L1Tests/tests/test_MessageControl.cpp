#include "gtest/gtest.h"
#include "MessageControl.h"

// Forward declare INotification if not available
namespace WPEFramework {
namespace PluginHost {
    struct IShell {
        virtual ~IShell() = default;
        virtual void AddRef() = 0;
        virtual uint32_t Release() = 0;
        virtual string ConfigLine() const = 0;
        virtual bool Background() const = 0;
        virtual string VolatilePath() const = 0;
        // Use void* for notification to avoid type errors
        virtual void Register(void* /*INotification*/) {}
        virtual void Unregister(void* /*INotification*/) {}
    };
    struct Channel {
        virtual ~Channel() = default;
        virtual uint32_t Id() const { return 1; }
    };
}
}

// Dummy IShell implementation
class DummyShell : public WPEFramework::PluginHost::IShell {
public:
    void AddRef() override {}
    uint32_t Release() override { return 0; }
    string ConfigLine() const override { return ""; }
    bool Background() const override { return false; }
    string VolatilePath() const override { return ""; }
    void Register(void* /*INotification*/) override {}
    void Unregister(void* /*INotification*/) override {}
};

// Dummy Channel implementation
class DummyChannel : public WPEFramework::PluginHost::Channel {
public:
    uint32_t Id() const override { return 1; }
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
