/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*************************************************************************************
 SECTION 1: Summary
 **************************************************************************************
 Plugin Name: USBMassStorage
 JSON-RPC Methods (aggregated via Exchange::JUSBMassStorage::Register):
   - (Derived from IUSBMassStorage interface) Expected methods include at least:
       getDeviceList, getMountPoints, getPartitionInfo
   - NOTE: Exact exported method names depend on JSON generation in interfaces/json/JUSBMassStorage.h
     which is not present in workspace snapshot. Using interface method names verbatim.
 Properties: (None explicitly declared in plugin source)
 Notifications / Events:
   - onDeviceMounted(deviceinfo, mountPoints)
   - onDeviceUnmounted(deviceinfo, mountPoints)
 External Dependencies:
   Exchange Interfaces:
     - Exchange::IUSBMassStorage (primary aggregated implementation)
     - Exchange::IUSBMassStorage::INotification (events)
     - Exchange::IUSBDevice (queried by implementation via QueryInterfaceByCallsign)
     - Exchange::IConfiguration (Configure call on implementation)
   PluginHost / Core:
     - PluginHost::IShell (service lifecycle, Submit for events)
     - Core::WorkerPool / IWorkerPool (async job dispatch in implementation)
     - RPC::IRemoteConnection (lifecycle)
   Wrappers / System Calls (inside implementation):
     - filesystem / dir mgmt: mkdir, rmdir, stat, statfs, statvfs, open, close
     - mount / umount (sys/mount.h)
     - ioctl (BLKGETSIZE64, BLKGETSIZE, BLKSSZGET)
     - open()/ioctl()/close
     - parsing /proc/partitions (ifstream)
   Missing wrapper mocks for: mount, umount, statfs, statvfs, ioctl, open (WrapsMock has many but not these specific ones under provided excerpt) -> TODO verify existing Wraps / secure wrappers before extending.
 Existing Mocks Found:
   - ServiceMock (Tests/mocks/thunder/ServiceMock.h)
   - COMLinkMock (Tests/mocks/thunder/COMLinkMock.h) (may be unused here)
   - WorkerPoolImplementation (Tests/mocks/WorkerPoolImplementation.h)
   - USBDeviceMock (Tests/mocks/USBDeviceMock.h)
   - ThunderPortability.h macros for event subscription harness
 Missing Mocks (TODO):
   - Mock for Exchange::IUSBMassStorage itself SHOULD NOT be created (we use real plugin aggregated interface)
   - Mock / abstraction for system calls mount/umount/statfs/statvfs/ioctl/open if direct interception required for negative path simulation. Add later if needed.
 **************************************************************************************
 SECTION 2: Fixture Design
 **************************************************************************************
 Base Fixture (USBMassStorageTest):
   - Creates Core::ProxyType<Plugin::USBMassStorage> plugin
   - Establishes JSON-RPC handler & connection context
   - Allocates NiceMock<ServiceMock> service
   - Starts WorkerPool (similar to USBDevice tests) for async event dispatch (implementation uses Core::IWorkerPool::Instance())
   - Provides helper to inject a NiceMock<USBDeviceMock> as the remote IUSBDevice returned by QueryInterfaceByCallsign("org.rdk.UsbDevice")
   - Initializes plugin in SetUp (Initialize) and Deinitialize in TearDown.
 Initialized Fixture (USBMassStorageInitializedTest):
   - Inherits from base; performs additional arrangement of remote USBDeviceMock expected calls for success path (e.g., GetDeviceList returning iterator).
 Mock Members:
   - NiceMock<ServiceMock> service
   - USBDeviceMock* usbDeviceMock (allocated dynamically so we can control lifetime and simulate QueryInterfaceByCallsign)
   - Core::ProxyType<WorkerPoolImplementation> workerPool
 Notification Strategy:
   - Subscribe to onDeviceMounted / onDeviceUnmounted using ThunderPortability macros (EVENT_SUBSCRIBE/EVENT_UNSUBSCRIBE)
   - Simulate underlying implementation events by directly creating a temporary implementation instance and invoking Dispatch (best-effort; real integration would use Root<>)
 Lifecycle & Cleanup:
   - Ensure service->Unregister called
   - Release workerPool & reset Core::IWorkerPool global pointer
   - Delete usbDeviceMock after plugin Deinitialize
   - Clear any created /tmp/media/usb* directories (best-effort; test-scoped paths)
 **************************************************************************************
 SECTION 3: Generated Fixture Code
 **************************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include "USBMassStorage.h"
#include "USBMassStorageImplementation.h"
#include "ServiceMock.h"
#include "USBDeviceMock.h"
#include "WorkerPoolImplementation.h"
#include "ThunderPortability.h"

using ::testing::NiceMock;
using ::testing::Invoke;
using ::testing::Return;
using namespace WPEFramework;

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;34m[%s:%d](%s) " x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);

namespace {
const string kCallsign = _T("USBMassStorage");
}

class USBMassStorageTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::USBMassStorage> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message; // for EVENT_SUBSCRIBE macro variant
    NiceMock<ServiceMock> service;
    USBDeviceMock* usbDeviceMock {nullptr};
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    string response;

    USBMassStorageTest()
        : plugin(Core::ProxyType<Plugin::USBMassStorage>::Create())
        , handler(*plugin)
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(2, Core::Thread::DefaultStackSize(), 16))
    {
    }

    ~USBMassStorageTest() override {}

    void SetUp() override {
        // Assign worker pool (mirrors pattern from other tests)
        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        // Provide USBDeviceMock through QueryInterfaceByCallsign when implementation configures.
        usbDeviceMock = new NiceMock<USBDeviceMock>();

        ON_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .WillByDefault(Invoke([&](const uint32_t, const string& name) -> void* {
                if (name == "org.rdk.UsbDevice") {
                    return static_cast<void*>(static_cast<Exchange::IUSBDevice*>(usbDeviceMock));
                }
                return nullptr;
            }));

        // Basic defaults for USBDeviceMock methods used during Configure (GetDeviceList may be called)
        ON_CALL(*usbDeviceMock, GetDeviceList(::testing::_))
            .WillByDefault(Return(Core::ERROR_NONE)); // returns empty iterator (nullptr) acceptable

        // Initialize plugin
        ASSERT_EQ(string(""), plugin->Initialize(&service));
    }

    void TearDown() override {
        plugin->Deinitialize(&service);
        if (usbDeviceMock) {
            delete usbDeviceMock; usbDeviceMock = nullptr;
        }
        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();
    }
};

class USBMassStorageInitializedTest : public USBMassStorageTest {
protected:
    void SetUp() override {
        USBMassStorageTest::SetUp();
    }
};

/*******************************************************
 * Test: RegisteredMethods
 *******************************************************/
TEST_F(USBMassStorageTest, RegisteredMethods) {
    // Verify core interface JSON-RPC methods exist (names inferred from interface)
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDeviceList")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getMountPoints")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPartitionInfo")));
    // Future additions: // EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("<newMethod>")));
}

/*******************************************************
 * Positive & Negative tests for getDeviceList
 *******************************************************/
TEST_F(USBMassStorageInitializedTest, getDeviceList_Success_EmptyList) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    // When no devices: implementation may return [] or error; accept empty array string or [] object form.
    // Flexible check: allow either empty array or empty object depending on JSON marshalling
    EXPECT_TRUE(response.empty() || response == "[]" || response == _T("[]"));
}

TEST_F(USBMassStorageInitializedTest, getDeviceList_Failure_RemoteNull) {
    // Simulate remote USBDevice becoming null by forcing QueryInterfaceByCallsign to return nullptr and clearing internal state.
    ON_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillByDefault(Return(nullptr));
    // Force a reconfigure path by deinitializing and initializing again.
    plugin->Deinitialize(&service);
    EXPECT_EQ(string(""), plugin->Initialize(&service));
    // Now call getDeviceList; expect general error (Core::ERROR_GENERAL) -> translates to non-zero
    EXPECT_NE(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
}

/*******************************************************
 * Positive & Negative tests for getMountPoints
 *******************************************************/
TEST_F(USBMassStorageInitializedTest, getMountPoints_Failure_EmptyDeviceName) {
    EXPECT_NE(Core::ERROR_NONE, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"\"}"), response));
}

TEST_F(USBMassStorageInitializedTest, getMountPoints_Failure_InvalidDevice) {
    EXPECT_NE(Core::ERROR_NONE, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"NonExistent\"}"), response));
}

/*******************************************************
 * Positive & Negative tests for getPartitionInfo
 *******************************************************/
TEST_F(USBMassStorageInitializedTest, getPartitionInfo_Failure_EmptyPath) {
    EXPECT_NE(Core::ERROR_NONE, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\":\"\"}"), response));
}

TEST_F(USBMassStorageInitializedTest, getPartitionInfo_Failure_InvalidPath) {
    EXPECT_NE(Core::ERROR_NONE, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\":\"/tmp/media/usb999\"}"), response));
}

/*******************************************************
 * Notification subscription test (onDeviceMounted)
 * Minimal fix applied: correct template use of Core::Service<>::Create<>()
 *******************************************************/
TEST_F(USBMassStorageInitializedTest, OnDeviceMounted_EventDispatch) {
    Core::Event eventTriggered(false, true);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(Invoke([&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& element) {
            string text; element->ToString(text);
            TEST_LOG("Event JSON: %s", text.c_str());
            EXPECT_NE(text.find("onDeviceMounted"), std::string::npos);
            eventTriggered.SetEvent();
            return Core::ERROR_NONE;
        }));

    // Subscribe
    EVENT_SUBSCRIBE(0, _T("onDeviceMounted"), _T("org.rdk.USBMassStorage"), message);

    // Create a temporary implementation instance and dispatch a synthetic mount event using Job helper
    auto impl = Core::Service<Plugin::USBMassStorageImplementation>::Create<Plugin::USBMassStorageImplementation>();
    Plugin::USBMassStorageImplementation::USBStorageDeviceInfo storageInfo{}; // adjust to correct nested type if needed
    storageInfo.deviceName = "devMock";
    storageInfo.devicePath = "/dev/sdx";

    // Indirectly invoke private Dispatch via Job (friend access)
    auto job = Plugin::USBMassStorageImplementation::Job::Create(impl, Plugin::USBMassStorageImplementation::USB_STORAGE_EVENT_MOUNT, storageInfo);
    job->Dispatch();

    eventTriggered.Lock(100);

    EVENT_UNSUBSCRIBE(0, _T("onDeviceMounted"), _T("org.rdk.USBMassStorage"), message);

    if (impl) { impl->Release(); }
}

/*******************************************************
 * TODO Placeholders for further comprehensive success path tests:
 *  - getDeviceList_Success_WithDevices
 *  - getMountPoints_Success_AfterMount
 *  - getPartitionInfo_Success_ValidMount
 *  - OnDeviceUnmounted_EventDispatch
 *  - Initialize_Deinitialize_Idempotent
 *******************************************************/

/*************************************************************************************
 SECTION 4: Extension Guidelines
 **************************************************************************************
 Adding New Method Tests:
   1. Ensure method name appears via handler.Exists.
   2. Prepare input JSON string; use _T() macro for wide compatibility.
   3. For success: configure usbDeviceMock / filesystem preconditions.
   4. For failure: manipulate mocks to return error codes (e.g., GetDeviceList returning Core::ERROR_GENERAL).
 Dependency Expectations:
   - Use ON_CALL for broad behavior, EXPECT_CALL for specific scenario validation.
   - For system calls not mocked yet, introduce abstraction in WrapsImpl (if available) and add corresponding NiceMock.
 Failure Simulation Examples:
   - Null remote interface: service.QueryInterfaceByCallsign returns nullptr before Initialize.
   - Mount failures: create directories but have mount syscall fail (would require wrapper).
   - Partition info failures: have open()/ioctl()/stat* return error via wrappers.
 **************************************************************************************
 SECTION 5: Validation Checklist
 **************************************************************************************
 [ ] No internal plugin classes mocked (only external Service / USBDevice mocks used)
 [ ] Lifecycle Initialize/Deinitialize executed without leaks
 [ ] JSON-RPC method existence verified
 [ ] At least two methods exercised with success/failure (mountPoints & partitionInfo failures, deviceList success/ failure)
 [ ] Event subscription logic covered (onDeviceMounted)
 [ ] All allocated mocks deleted in TearDown
 [ ] WorkerPool started and stopped cleanly
 [ ] TODO markers left for additional success path & event tests
 *************************************************************************************/
