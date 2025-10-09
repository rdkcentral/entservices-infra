/*
* Copyright 2024 RDK Management
* Licensed under the Apache License, Version 2.0
* http://www.apache.org/licenses/LICENSE-2.0
*/

#include <gtest/gtest.h>
#include <core/core.h>
#include <plugins/IPlugin.h>
#include <plugins/Channel.h>
#include <string>
#include "MessageControl.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

// Stub IShell for plugin initialization
class ShellStub : public PluginHost::IShell {
public:
    ShellStub() : _config("{\"console\":true}") {}
    void AddRef() override {}
    uint32_t Release() override { return 0; }
    string ConfigLine() const override { return _config; }
    bool Background() const override { return false; }
    string VolatilePath() const override { return "/tmp/"; }
    void Register(RPC::IRemoteConnection::INotification*) override {}
    void Unregister(RPC::IRemoteConnection::INotification*) override {}
private:
    string _config;
};

// Dummy Channel for Attach/Detach
class DummyChannel : public PluginHost::Channel {
public:
    uint32_t Id() const override { return 123; }
};

class MessageControlL1Test : public ::testing::Test {
protected:
    std::unique_ptr<MessageControl> control;
    void SetUp() override {
        control = std::make_unique<MessageControl>();
    }
};

TEST_F(MessageControlL1Test, InitializationAndDeinitialization) {
    ShellStub shell;
    EXPECT_TRUE(control->Initialize(&shell).empty());
    control->Deinitialize(&shell);
}

TEST_F(MessageControlL1Test, AttachDetachChannel) {
    DummyChannel channel;
    EXPECT_TRUE(control->Attach(channel));
    control->Detach(channel);
}

TEST_F(MessageControlL1Test, InformationIsEmpty) {
    EXPECT_TRUE(control->Information().empty());
}

TEST_F(MessageControlL1Test, InboundCommandReturnsElement) {
    auto element = control->Inbound("test");
    EXPECT_TRUE(element.IsValid());
}

TEST_F(MessageControlL1Test, InboundReceivedReturnsElement) {
    auto dummyElement = Core::ProxyType<Core::JSON::IElement>::Create();
    auto result = control->Inbound(1, dummyElement);
    EXPECT_TRUE(result.IsValid());
}
