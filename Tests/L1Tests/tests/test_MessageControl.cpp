#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MessageControlImplementation.cpp" // Direct include for unit test

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

// ===== Testable Wrapper Class =====
class TestableMessageControl : public MessageControlImplementation {
public:
    // Expose protected methods for testing if needed
    using MessageControlImplementation::Attach;
    using MessageControlImplementation::Detach;
    using MessageControlImplementation::Callback;
    using MessageControlImplementation::Enable;
    using MessageControlImplementation::Controls;
};

// ===== Test Fixture Base =====
class MessageControlTest : public ::testing::Test {
protected:
    TestableMessageControl* control;

    void SetUp() override {
        control = new TestableMessageControl();
    }
    void TearDown() override {
        delete control;
    }
};

// ===== Dummy Callback =====
class DummyCallback : public Exchange::IMessageControl::ICollect::ICallback {
public:
    DummyCallback() : called(false) {}
    void Message(Exchange::IMessageControl::MessageType, const string&, const string&, const string&, const uint32_t, const string&, const uint64_t, const string&) override {
        called = true;
    }
    bool called;
};

// =========================
// ===== TEST CASES ========
// =========================

TEST_F(MessageControlTest, ConstructDestruct) {
    EXPECT_NE(control, nullptr);
}

TEST_F(MessageControlTest, CallbackSetUnset) {
    DummyCallback cb;
    EXPECT_EQ(control->Callback(&cb), Core::ERROR_NONE);
    EXPECT_EQ(control->Callback(nullptr), Core::ERROR_NONE);
}

TEST_F(MessageControlTest, AttachDetachInstance) {
    uint32_t id = 42;
    EXPECT_EQ(control->Attach(id), Core::ERROR_NONE);
    EXPECT_EQ(control->Detach(id), Core::ERROR_NONE);
}

TEST_F(MessageControlTest, EnableControls) {
    EXPECT_EQ(control->Enable(Exchange::IMessageControl::MessageType::TRACING, "testcat", "testmod", true), Core::ERROR_NONE);

    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    EXPECT_EQ(control->Controls(controls), Core::ERROR_NONE);
    ASSERT_NE(controls, nullptr);

    bool found = false;
    while (controls->Next()) {
        if (controls->Type() == Exchange::IMessageControl::MessageType::TRACING &&
            controls->Category() == "testcat" &&
            controls->Module() == "testmod" &&
            controls->Enabled()) {
            found = true;
            break;
        }
    }
    controls->Release();
    EXPECT_TRUE(found);
}
