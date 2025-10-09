/*
 * Copyright 2024 RDK Management
 * Licensed under the Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#define MODULE_NAME MessageControlL1Tests

#include <gtest/gtest.h>
#include "MessageControl.h"
#include "MessageControlImplementation.cpp" // Include concrete implementation

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

// Minimal stub for Channel (only for Attach/Detach test)
class DummyChannel : public PluginHost::Channel {
public:
    DummyChannel() : PluginHost::Channel(0, Core::NodeId("127.0.0.1", 12345)) {}
    ~DummyChannel() override = default;
    uint32_t Id() const override { return 42; }
    // Implement other pure virtuals if needed
};

class MessageControlTest : public ::testing::Test {
protected:
    MessageControlImplementation* control = nullptr;
    void SetUp() override {
        control = new MessageControlImplementation();
    }
    void TearDown() override {
        if (control) delete control;
    }
};

TEST_F(MessageControlTest, InformationEmptyBeforeInit) {
    EXPECT_TRUE(control->Controls(nullptr) == Core::ERROR_NONE);
}

TEST_F(MessageControlTest, EnableControlsCleanup) {
    EXPECT_EQ(Core::ERROR_NONE, control->Enable(Exchange::IMessageControl::MessageType::TRACING, "TestCategory", "TestModule", true));
    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    EXPECT_EQ(Core::ERROR_NONE, control->Controls(controls));
    if (controls) controls->Release();
}

TEST_F(MessageControlTest, AttachDetachInstance) {
    EXPECT_EQ(Core::ERROR_NONE, control->Attach(123));
    EXPECT_EQ(Core::ERROR_NONE, control->Detach(123));
}

TEST_F(MessageControlTest, CallbackSetUnset) {
    struct DummyCallback : public Exchange::IMessageControl::ICollect::ICallback {
        void Message(Exchange::IMessageControl::MessageType, const string&, const string&, const string&, const uint32_t, const string&, const uint64_t, const string&) override {}
    };
    DummyCallback cb;
    EXPECT_EQ(Core::ERROR_NONE, control->Callback(&cb));
    EXPECT_EQ(Core::ERROR_NONE, control->Callback(nullptr));
}

// ...add more tests for other public MessageControlImplementation methods as needed...
