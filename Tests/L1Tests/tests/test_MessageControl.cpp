/*
 * Copyright 2024 RDK Management
 * Licensed under the Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <gtest/gtest.h>
#include "MessageControlImplementation.cpp" // Direct include for unit test

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

class DummyCallback : public Exchange::IMessageControl::ICollect::ICallback {
public:
    DummyCallback() : called(false) {}
    void Message(Exchange::IMessageControl::MessageType, const string&, const string&, const string&, const uint32_t, const string&, const uint64_t, const string&) override {
        called = true;
    }
    bool called;
};

class MessageControlImplTest : public ::testing::Test {
protected:
    MessageControlImplementation* control;

    void SetUp() override {
        control = new MessageControlImplementation();
    }
    void TearDown() override {
        delete control;
    }
};

TEST_F(MessageControlImplTest, ConstructDestruct) {
    EXPECT_NE(control, nullptr);
}

TEST_F(MessageControlImplTest, CallbackSetUnset) {
    DummyCallback* cb = new DummyCallback();
    EXPECT_EQ(control->Callback(cb), Core::ERROR_NONE);
    EXPECT_EQ(control->Callback(nullptr), Core::ERROR_NONE);
    delete cb;
}

TEST_F(MessageControlImplTest, AttachDetachInstance) {
    uint32_t id = 42;
    EXPECT_EQ(control->Attach(id), Core::ERROR_NONE);
    EXPECT_EQ(control->Detach(id), Core::ERROR_NONE);
}

TEST_F(MessageControlImplTest, EnableControls) {
    // Enable a control and check Controls iterator
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
