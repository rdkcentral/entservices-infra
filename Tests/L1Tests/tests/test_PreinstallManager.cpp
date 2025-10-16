/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
**/
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include "PreinstallManager.h"
#include "PreinstallManagerImplementation.h"
#include "ServiceMock.h"
#include "PackageManagerMock.h"
#include "WorkerPoolImplementation.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"
#include "Module.h"
#include "WorkerPoolImplementation.h"
#include "WrapsMock.h"
#include "FactoriesImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TIMEOUT   (1000)

#define PREINSTALL_PACKAGE_ID           "com.test.preinstall.app"
#define PREINSTALL_PACKAGE_VERSION      "1.0.0"
#define PREINSTALL_PACKAGE_FILELOCATION "/opt/preinstall/testapp/package.wgt"
#define PREINSTALL_NEWER_VERSION        "2.0.0"
#define PREINSTALL_OLDER_VERSION        "0.9.0"
#define TIMEOUT   (2000)
#define PREINSTALL_MANAGER_PACKAGE_ID        "com.test.preinstall"
#define PREINSTALL_MANAGER_VERSION           "1.0.0"
#define PREINSTALL_MANAGER_FILE_LOCATOR      "/opt/preinstall/com.test.preinstall.tar.gz"
#define PREINSTALL_MANAGER_INSTALL_STATUS    "INSTALLED"
#define PREINSTALL_MANAGER_UNPACK_PATH       "/opt/apps/com.test.preinstall"
#define TEST_JSON_INSTALLATION_STATUS        R"({"packageId":"com.test.preinstall","version":"1.0.0","status":"INSTALLED"})"

typedef enum : uint32_t {
    PreinstallManager_invalidEvent = 0,
    PreinstallManager_onAppInstallationStatusEvent
} PreinstallManagerTest_events_t;
    PreinstallManager_StateInvalid = 0x00000000,
    PreinstallManager_onAppInstallationStatus = 0x00000001
} PreinstallManagerL1test_async_events_t;

using ::testing::NiceMock;
using namespace WPEFramework;
using namespace std;

class NotificationTest : public Exchange::IPreinstallManager::INotification 
{
    private:
        BEGIN_INTERFACE_MAP(NotificationTest)
        INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
        END_INTERFACE_MAP
    
    public:
        mutable mutex m_mutex;
        mutable condition_variable m_condition_variable;
        mutable uint32_t m_event_signal = PreinstallManager_invalidEvent;
        mutable string receivedJsonResponse;

        void OnAppInstallationStatus(const std::string& jsonresponse) override 
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            receivedJsonResponse = jsonresponse;
            m_event_signal = PreinstallManager_onAppInstallationStatusEvent;
            m_condition_variable.notify_one();
        }

        uint32_t WaitForEventStatus(uint32_t timeout_ms, PreinstallManagerTest_events_t status) const
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            auto timeout = std::chrono::milliseconds(timeout_ms);
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
            {
                 TEST_LOG("Timeout waiting for request status event");
                 return PreinstallManager_invalidEvent;
            }
            return m_event_signal;
        }
};

// Mock Package Iterator
class PackageIteratorMock : public Exchange::IPackageInstaller::IPackageIterator {
public:
    virtual ~PackageIteratorMock() = default;
    
    MOCK_METHOD(bool, Next, (Exchange::IPackageInstaller::Package&), (override));
    MOCK_METHOD(void, Reset, (), (override));
    MOCK_METHOD(bool, IsValid, (), (const, override));

    BEGIN_INTERFACE_MAP(PackageIteratorMock)
    INTERFACE_ENTRY(Exchange::IPackageInstaller::IPackageIterator)
    END_INTERFACE_MAP
};
namespace {
const string callSign = _T("PreinstallManager");
}

class PreinstallManagerTest : public ::testing::Test {
protected:
    string packageId;
    string version;
    string fileLocator;
    bool forceInstall;
    string jsonResponse;
    Exchange::IPackageInstaller::FailReason failReason;
    Exchange::RuntimeConfig configMetadata;

    Core::ProxyType<Plugin::PreinstallManagerImplementation> mPreinstallManagerImpl;
    Exchange::IPreinstallManager* interface = nullptr;
    Exchange::IConfiguration* mPreinstallManagerConfigure = nullptr;
    PackageInstallerMock* mPackageInstallerMock = nullptr;
    PackageIteratorMock* mPackageIteratorMock = nullptr;
    ServiceMock* mServiceMock = nullptr;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    uint32_t event_signal;
    PackageInstallerMock* mPackageInstallerMock = nullptr;
    WrapsImplMock *p_wrapsImplMock = nullptr;
    Core::JSONRPC::Message message;
    FactoriesImplementation factoriesImplementation;
    PLUGINHOST_DISPATCHER *dispatcher;

    PreinstallManagerTest()
	: workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        mPreinstallManagerImpl = Core::ProxyType<Plugin::PreinstallManagerImplementation>::Create();
        
        interface = static_cast<Exchange::IPreinstallManager*>(mPreinstallManagerImpl->QueryInterface(Exchange::IPreinstallManager::ID));
    Core::ProxyType<Plugin::PreinstallManager> plugin;
    Plugin::PreinstallManagerImplementation *mPreinstallManagerImpl;
    Exchange::IPackageInstaller::INotification* mPackageInstallerNotification_cb = nullptr;
    Exchange::IPreinstallManager::INotification* mPreinstallManagerNotification = nullptr;

		Core::IWorkerPool::Assign(&(*workerPool));
		workerPool->Run();
    }
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;

    virtual ~PreinstallManagerTest() override
    Core::hresult createResources()
    {
		interface->Release();

        Core::IWorkerPool::Assign(nullptr);
		workerPool.Release();
    }
	
	void createResources() 
	{
	    // Initialize the parameters with default values
        packageId = PREINSTALL_PACKAGE_ID;
        version = PREINSTALL_PACKAGE_VERSION;
        fileLocator = PREINSTALL_PACKAGE_FILELOCATION;
        forceInstall = true;
        jsonResponse = R"({"packageId":"com.test.preinstall.app","version":"1.0.0","status":"INSTALLED"})";
        failReason = Exchange::IPackageInstaller::FailReason::NONE;
        
        event_signal = PreinstallManager_invalidEvent;
		
		// Set up mocks and expect calls
        Core::hresult status = Core::ERROR_GENERAL;
        mServiceMock = new NiceMock<ServiceMock>;
        mPackageInstallerMock = new NiceMock<PackageInstallerMock>;
        mPackageIteratorMock = new NiceMock<PackageIteratorMock>;
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);

        mPreinstallManagerConfigure = static_cast<Exchange::IConfiguration*>(mPreinstallManagerImpl->QueryInterface(Exchange::IConfiguration::ID));
		
        PluginHost::IFactories::Assign(&factoriesImplementation);
        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
            plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(mServiceMock);
        
        TEST_LOG("In createResources!");
        
        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.PackageManagerRDKEMS") {
                    return reinterpret_cast<void*>(mPackageInstallerMock);
                } 
            return nullptr;
        }));

		EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());
	
		EXPECT_CALL(*mPackageInstallerMock, Register(::testing::_))
          .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
        // Configure the PreinstallManager
        mPreinstallManagerConfigure->Configure(mServiceMock);
		
		ASSERT_TRUE(interface != nullptr);  
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Invoke(
                [&](const uint32_t id, const std::string& name) -> void* {
                    if (name == "org.rdk.PackageManagerRDKEMS") {
                        if (id == Exchange::IPackageInstaller::ID) {
                            return reinterpret_cast<void*>(mPackageInstallerMock);
                        }
                    }
                    return nullptr;
                }));

        EXPECT_CALL(*mPackageInstallerMock, Register(::testing::_))
            .WillOnce(::testing::Invoke(
                [&](Exchange::IPackageInstaller::INotification* notification) {
                    mPackageInstallerNotification_cb = notification;
                    return Core::ERROR_NONE;
                }));

        ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(-1));
        
        EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
        mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();
        TEST_LOG("createResources - All done!");
        status = Core::ERROR_NONE;

        return status;
    }

    void releaseResources()
    {
	    // Clean up mocks
		if (mServiceMock != nullptr)
        TEST_LOG("In releaseResources!");

        if (mPackageInstallerMock != nullptr && mPackageInstallerNotification_cb != nullptr)
        {
			EXPECT_CALL(*mServiceMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mServiceMock;
						mServiceMock = nullptr;
						return 0;
					}));    
            ON_CALL(*mPackageInstallerMock, Unregister(::testing::_))
                .WillByDefault(::testing::Invoke([&]() {
                    return 0;
                }));
            mPackageInstallerNotification_cb = nullptr;
        }

        if (mPackageInstallerMock != nullptr)
        {
			EXPECT_CALL(*mPackageInstallerMock, Unregister(::testing::_))
              .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

			EXPECT_CALL(*mPackageInstallerMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mPackageInstallerMock;
						mPackageInstallerMock = nullptr;
						return 0;
					}));
            EXPECT_CALL(*mPackageInstallerMock, Release())
                .WillOnce(::testing::Invoke(
                    [&]() {
                        delete mPackageInstallerMock;
                        return 0;
                    }));
        }

        if (mPackageIteratorMock != nullptr)
        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
			EXPECT_CALL(*mPackageIteratorMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mPackageIteratorMock;
						mPackageIteratorMock = nullptr;
						return 0;
					}));
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }

		// Clean up the PreinstallManager
        mPreinstallManagerConfigure->Release();
		
		ASSERT_TRUE(interface != nullptr); 
    }
        dispatcher->Deactivate();
        dispatcher->Release();

    void fillPackageIterator(Exchange::IPackageInstaller::Package& package, bool hasNext = true) {
        package.packageId = packageId;
        package.version = version;
        package.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
        
        EXPECT_CALL(*mPackageIteratorMock, Next(::testing::_))
            .Times(::testing::AtLeast(1))
            .WillOnce(::testing::Invoke([&](Exchange::IPackageInstaller::Package& pkg) {
                pkg = package;
                return hasNext;
            }))
            .WillRepeatedly(::testing::Return(false));
        plugin->Deinitialize(mServiceMock);
        delete mServiceMock;
        mPreinstallManagerImpl = nullptr;
    }
};

/* Test Case for Registering and Unregistering Notification
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a notification instance using the NotificationTest class
 * Register the notification with the PreinstallManager interface
 * Verify successful registration of notification by asserting that Register() returns Core::ERROR_NONE
 * Unregister the notification from the PreinstallManager interface
 * Verify successful unregistration of notification by asserting that Unregister() returns Core::ERROR_NONE
 * Release the PreinstallManager interface object and clean-up related test resources
 */
TEST_F(PreinstallManagerTest, RegisterNotification_Success)
{
    createResources();
    PreinstallManagerTest()
        : plugin(Core::ProxyType<Plugin::PreinstallManager>::Create()),
        workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(2, Core::Thread::DefaultStackSize(), 16)),
        mJsonRpcHandler(*plugin),
        INIT_CONX(1, 0)
    {
        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();
    }

    Core::Sink<NotificationTest> notification;
    virtual ~PreinstallManagerTest() override
    {
        TEST_LOG("Delete ~PreinstallManagerTest Instance!");
        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();
    }
};

    // TC-1: Check if the notification is registered successfully
    EXPECT_EQ(Core::ERROR_NONE, interface->Register(&notification));
class NotificationHandler : public Exchange::IPreinstallManager::INotification {
    private:
        BEGIN_INTERFACE_MAP(Notification)
        INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
        END_INTERFACE_MAP

    EXPECT_EQ(Core::ERROR_NONE, interface->Unregister(&notification));
    public:
        NotificationHandler() = default;
        virtual ~NotificationHandler() = default;

    releaseResources();
}
        std::mutex m_mutex;
        std::condition_variable m_condition_variable;
        uint32_t m_event_signalled = PreinstallManager_StateInvalid;
        string m_lastInstallationStatus;

/* Test Case for Unregistering Notification without registering
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a notification instance using the NotificationTest class
 * Unregister the notification from the PreinstallManager interface
 * Verify unregistration of notification fails by asserting that Unregister() returns Core::ERROR_GENERAL
 * Release the PreinstallManager interface object and clean-up related test resources
 */
TEST_F(PreinstallManagerTest, UnregisterNotification_WithoutRegister)
{
    createResources();
        void OnAppInstallationStatus(const string& jsonresponse) override
        {
            TEST_LOG("OnAppInstallationStatus called with: %s", jsonresponse.c_str());
            
            std::unique_lock<std::mutex> lock(m_mutex);
            m_lastInstallationStatus = jsonresponse;
            m_event_signalled = PreinstallManager_onAppInstallationStatus;
            m_condition_variable.notify_one();
        }

    Core::Sink<NotificationTest> notification;
	
	// TC-2: Check if the notification unregistration fails without registering
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Unregister(&notification));
        uint32_t WaitForEvent(uint32_t timeout_ms)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto timeout = std::chrono::milliseconds(timeout_ms);
            
            m_condition_variable.wait_for(lock, timeout, [this]() {
                return m_event_signalled != PreinstallManager_StateInvalid;
            });
            
            return m_event_signalled;
        }

    releaseResources();
}
        void ResetEvent()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_event_signalled = PreinstallManager_StateInvalid;
            m_lastInstallationStatus.clear();
        }
};

/* Test Case for StartPreinstall with forceInstall enabled and no existing packages
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set forceInstall to true
 * Mock GetConfigForPackage to return valid package configuration
 * Mock Install method to return success
 * Call StartPreinstall with forceInstall = true
 * Verify successful preinstall by asserting that StartPreinstall() returns Core::ERROR_NONE
 * Release the PreinstallManager interface object and clean-up related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstall_ForceInstall_Success)
/*******************************************************************************************************************
 * Test Case for RegisteredMethodsUsingJsonRpcSuccess
 * Setting up PreinstallManager Plugin and creating required JSON-RPC resources
 * Verifying whether all methods exist or not
 * Releasing the PreinstallManager interface and all related test resources
 ********************************************************************************************************************/
TEST_F(PreinstallManagerTest, RegisteredMethodsUsingJsonRpcSuccess)
{
    createResources();

    // TC-3: Test StartPreinstall with force install enabled
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const string& fileLocator, string& packageId, string& version, Exchange::RuntimeConfig& config) {
            packageId = this->packageId;
            version = this->version;
            return Core::ERROR_NONE;
        }));

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const string& packageId, const string& version, Exchange::IPackageInstaller::IKeyValueIterator* const& metadata, const string& fileLocator, Exchange::IPackageInstaller::FailReason& reason) {
            reason = Exchange::IPackageInstaller::FailReason::NONE;
            return Core::ERROR_NONE;
        }));

    forceInstall = true;
    
    EXPECT_EQ(Core::ERROR_NONE, interface->StartPreinstall(forceInstall));
    EXPECT_TRUE(mJsonRpcHandler.Exists(_T("startPreinstall")));

    releaseResources();
}

/* Test Case for StartPreinstall with forceInstall disabled and newer version available
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set forceInstall to false
 * Mock ListPackages to return existing packages with older versions
 * Mock GetConfigForPackage to return newer version package configuration
 * Mock Install method to return success
 * Call StartPreinstall with forceInstall = false
 * Verify successful preinstall by asserting that StartPreinstall() returns Core::ERROR_NONE
 * Release the PreinstallManager interface object and clean-up related test resources
/*
 * Test Case for StartPreinstallUsingComRpcSuccess
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources
 * Setting Mock for PackageInstaller Install() to simulate successful installation
 * Calling StartPreinstall() with forceInstall=false using QueryInterface
 * Verifying the return of the API
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstall_NewerVersion_Success)
TEST_F(PreinstallManagerTest, StartPreinstallUsingComRpcSuccess)
{
    createResources();

    // TC-4: Test StartPreinstall with newer version available
    Exchange::IPackageInstaller::Package existingPackage;
    existingPackage.packageId = packageId;
    existingPackage.version = PREINSTALL_OLDER_VERSION;
    existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce(::testing::Invoke([&](Exchange::IPackageInstaller::IPackageIterator*& iterator) {
            iterator = mPackageIteratorMock;
            fillPackageIterator(existingPackage);
            return Core::ERROR_NONE;
        }));

    version = PREINSTALL_NEWER_VERSION; // Set newer version for preinstall package
    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const string& fileLocator, string& packageId, string& version, Exchange::RuntimeConfig& config) {
            packageId = this->packageId;
            version = this->version;
            return Core::ERROR_NONE;
        }));
    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const string& packageId, const string& version, Exchange::IPackageInstaller::IKeyValueIterator* const& metadata, const string& fileLocator, Exchange::IPackageInstaller::FailReason& reason) {
            reason = Exchange::IPackageInstaller::FailReason::NONE;
            return Core::ERROR_NONE;
        }));
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    forceInstall = false;
    
    EXPECT_EQ(Core::ERROR_NONE, interface->StartPreinstall(forceInstall));
    Core::hresult result = preinstallInterface->StartPreinstall(false);
    EXPECT_EQ(result, Core::ERROR_NONE);

    releaseResources();
    cleanup();
    preinstallInterface = nullptr;
}

/* Test Case for StartPreinstall with forceInstall disabled and older version - should skip
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set forceInstall to false
 * Mock ListPackages to return existing packages with newer versions
 * Mock GetConfigForPackage to return older version package configuration
 * Call StartPreinstall with forceInstall = false
 * Verify successful preinstall (skipping installation) by asserting that StartPreinstall() returns Core::ERROR_NONE
 * Release the PreinstallManager interface object and clean-up related test resources
/*
 * Test Case for StartPreinstallWithForceInstallUsingComRpcSuccess
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources  
 * Setting Mock for PackageInstaller Install() to simulate successful installation
 * Calling StartPreinstall() with forceInstall=true using QueryInterface
 * Verifying the return of the API
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstall_OlderVersion_Skip)
TEST_F(PreinstallManagerTest, StartPreinstallWithForceInstallUsingComRpcSuccess)
{
    createResources();

    // TC-5: Test StartPreinstall with older version available - should skip installation
    Exchange::IPackageInstaller::Package existingPackage;
    existingPackage.packageId = packageId;
    existingPackage.version = PREINSTALL_NEWER_VERSION;
    existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce(::testing::Invoke([&](Exchange::IPackageInstaller::IPackageIterator*& iterator) {
            iterator = mPackageIteratorMock;
            fillPackageIterator(existingPackage);
            return Core::ERROR_NONE;
        }));

    version = PREINSTALL_OLDER_VERSION; // Set older version for preinstall package

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const string& fileLocator, string& packageId, string& version, Exchange::RuntimeConfig& config) {
            packageId = this->packageId;
            version = this->version;
            return Core::ERROR_NONE;
        }));

    // Install should not be called for older version
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    forceInstall = false;
    
    EXPECT_EQ(Core::ERROR_NONE, interface->StartPreinstall(forceInstall));
    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    releaseResources();
}

/* Test Case for StartPreinstall with installation failure
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set forceInstall to true
 * Mock GetConfigForPackage to return valid package configuration
 * Mock Install method to return failure
 * Call StartPreinstall with forceInstall = true
 * Verify preinstall failure by asserting that StartPreinstall() returns Core::ERROR_GENERAL
 * Release the PreinstallManager interface object and clean-up related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstall_InstallationFailure)
{
    createResources();

    // TC-6: Test StartPreinstall with installation failure
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const string& fileLocator, string& packageId, string& version, Exchange::RuntimeConfig& config) {
            packageId = this->packageId;
            version = this->version;
            return Core::ERROR_NONE;
        }));
    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const string& packageId, const string& version, Exchange::IPackageInstaller::IKeyValueIterator* const& metadata, const string& fileLocator, Exchange::IPackageInstaller::FailReason& reason) {
            reason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL;
        }));
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    forceInstall = true;
    
    EXPECT_EQ(Core::ERROR_GENERAL, interface->StartPreinstall(forceInstall));
    Core::hresult result = preinstallInterface->StartPreinstall(true);
    EXPECT_EQ(result, Core::ERROR_NONE);

    releaseResources();
    cleanup();
    preinstallInterface = nullptr;
}

/* Test Case for StartPreinstall with invalid package configuration
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Mock GetConfigForPackage to return error (invalid package)
 * Call StartPreinstall with forceInstall = true
 * Verify preinstall failure by asserting that StartPreinstall() returns Core::ERROR_GENERAL
 * Release the PreinstallManager interface object and clean-up related test resources
/*
 * Test Case for StartPreinstallUsingComRpcFailurePackageInstallerObjectIsNull
 * Setting up only PreinstallManager Plugin and creating required COM-RPC resources
 * PackageInstaller Interface object is not created and hence the API should return error
 * Releasing the PreinstallManager Interface object only
 */
TEST_F(PreinstallManagerTest, StartPreinstall_InvalidPackageConfig)
TEST_F(PreinstallManagerTest, StartPreinstallUsingComRpcFailurePackageInstallerObjectIsNull)
{
    createResources();
    // Setup with null PackageInstaller to simulate failure
    mServiceMock = new NiceMock<ServiceMock>;
    p_wrapsImplMock = new NiceMock<WrapsImplMock>;
    Wraps::setImpl(p_wrapsImplMock);

    PluginHost::IFactories::Assign(&factoriesImplementation);
    dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
    dispatcher->Activate(mServiceMock);
    
    // Mock to return nullptr for PackageInstaller
    EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    // TC-7: Test StartPreinstall with invalid package configuration
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_GENERAL));
    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(-1));

    // Install should not be called for invalid packages
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
    mPreinstallManagerImpl = Plugin::PreinstallManagerImplementation::getInstance();

    forceInstall = true;
    
    EXPECT_EQ(Core::ERROR_GENERAL, interface->StartPreinstall(forceInstall));
    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    releaseResources();
    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr) {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }
        dispatcher->Deactivate();
        dispatcher->Release();
        plugin->Deinitialize(mServiceMock);
        delete mServiceMock;
        mPreinstallManagerImpl = nullptr;
    };

    Core::hresult result = preinstallInterface->StartPreinstall(false);
    EXPECT_EQ(result, Core::ERROR_GENERAL);

    cleanup();
    preinstallInterface = nullptr;
}

/* Test Case for handleOnAppInstallationStatus event handling
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Call handleOnAppInstallationStatus with valid JSON response
 * Verify that the event is properly handled and dispatched to registered notifications
 * Release the PreinstallManager interface object and clean-up related test resources
/*
 * Test Case for RegisterNotificationUsingComRpcSuccess
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources
 * Creating a notification handler and registering it using QueryInterface
 * Verifying successful registration
 * Unregistering the notification and verifying successful unregistration
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, HandleOnAppInstallationStatus_Success)
TEST_F(PreinstallManagerTest, RegisterNotificationUsingComRpcSuccess)
{
    createResources();

    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    {
        // Test event handling with NotificationTest
        Core::Sink<NotificationTest> eventNotification;
        interface->Register(&eventNotification);
        
        // TC-8: Test handleOnAppInstallationStatus event handling
        string testJsonResponse = R"({"packageId":"com.test.app","version":"1.0.0","status":"INSTALLED"})";
        
        mPreinstallManagerImpl->handleOnAppInstallationStatus(testJsonResponse);

        // Wait for event to be processed with timeout
        uint32_t eventResult = eventNotification.WaitForEventStatus(TIMEOUT, PreinstallManager_onAppInstallationStatusEvent);
        EXPECT_EQ(PreinstallManager_onAppInstallationStatusEvent, eventResult);
        EXPECT_EQ(testJsonResponse, eventNotification.receivedJsonResponse);
        
        interface->Unregister(&eventNotification);
    }
    
    {
        // Test Register/Unregister with NotificationHandler
        Core::Sink<NotificationHandler> notificationHandler;
        
        Core::hresult regResult = preinstallInterface->Register(&notificationHandler);
        EXPECT_EQ(regResult, Core::ERROR_NONE);

        Core::hresult unregResult = preinstallInterface->Unregister(&notificationHandler);
        EXPECT_EQ(unregResult, Core::ERROR_NONE);
    }

    cleanup();
    preinstallInterface = nullptr;
}

/* Test Case for handleOnAppInstallationStatus with empty JSON response
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Call handleOnAppInstallationStatus with empty JSON response
 * Verify that the event handling gracefully handles empty response
 * Release the PreinstallManager interface object and clean-up related test resources
/*
 * Test Case for UnregisterNotificationWithoutRegisterUsingComRpcFailure
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources
 * Creating a notification handler but not registering it
 * Attempting to unregister the notification should fail using QueryInterface
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, HandleOnAppInstallationStatus_EmptyResponse)
TEST_F(PreinstallManagerTest, UnregisterNotificationWithoutRegisterUsingComRpcFailure)
{
    createResources();

    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    {
        // Test event handling with NotificationTest
        Core::Sink<NotificationTest> eventNotification;
        interface->Register(&eventNotification);
        
        // TC-9: Test handleOnAppInstallationStatus with empty response
        string emptyJsonResponse = "";
        
        mPreinstallManagerImpl->handleOnAppInstallationStatus(emptyJsonResponse);

        // Wait for event - should timeout since empty response shouldn't trigger event
        uint32_t eventResult = eventNotification.WaitForEventStatus(200, PreinstallManager_onAppInstallationStatusEvent);
        EXPECT_EQ(PreinstallManager_invalidEvent, eventResult); // Should timeout
        
        interface->Unregister(&eventNotification);
    }
    
    {
        // Test Unregister without prior Register - should fail
        Core::Sink<NotificationHandler> notificationHandler;
        
        Core::hresult unregResult = preinstallInterface->Unregister(&notificationHandler);
        EXPECT_EQ(unregResult, Core::ERROR_GENERAL);
    }

    cleanup();
    preinstallInterface = nullptr;
}

/* Test Case for StartPreinstall when ListPackages fails
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set forceInstall to false to trigger ListPackages call
 * Mock ListPackages to return error
 * Call StartPreinstall with forceInstall = false
 * Verify preinstall failure by asserting that StartPreinstall() returns Core::ERROR_GENERAL
 * Release the PreinstallManager interface object and clean-up related test resources
/*
 * Test Case for OnAppInstallationStatusNotificationSuccess
 * Setting up PreinstallManager Plugin and creating required COM-RPC resources
 * Registering a notification handler using Sink<NotificationHandler>
 * Simulating an installation status callback from PackageInstaller
 * Verifying that the notification is received
 * Releasing the PreinstallManager Interface object and all related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstall_ListPackagesFailure)
TEST_F(PreinstallManagerTest, OnAppInstallationStatusNotificationSuccess)
{
    createResources();

    // TC-10: Test StartPreinstall when ListPackages fails
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));

    // GetConfigForPackage and Install should not be called when ListPackages fails
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    // Get the IPreinstallManager interface using QueryInterface
    Exchange::IPreinstallManager* preinstallInterface = static_cast<Exchange::IPreinstallManager*>(
        plugin->QueryInterface(Exchange::IPreinstallManager::ID));
    ASSERT_NE(preinstallInterface, nullptr);

    forceInstall = false;
    
    EXPECT_EQ(Core::ERROR_GENERAL, interface->StartPreinstall(forceInstall));
    // Use RAII-style cleanup
    auto cleanup = [&]() {
        if (preinstallInterface) {
            preinstallInterface->Release();
        }
        releaseResources();
    };

    releaseResources();
}
    Core::Sink<NotificationHandler> notification;
    Core::hresult result = preinstallInterface->Register(&notification);
    ASSERT_EQ(result, Core::ERROR_NONE);

/* Test Case for StartPreinstall with same version already installed - should skip
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set forceInstall to false
 * Mock ListPackages to return existing package with same version
 * Mock GetConfigForPackage to return same version package configuration
 * Call StartPreinstall with forceInstall = false
 * Verify successful preinstall (skipping installation) by asserting that StartPreinstall() returns Core::ERROR_NONE
 * Release the PreinstallManager interface object and clean-up related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstall_SameVersion_Skip)
{
    createResources();
    // Simulate installation status callback
    const string testInstallationStatus = TEST_JSON_INSTALLATION_STATUS;
    
    ASSERT_NE(mPackageInstallerNotification_cb, nullptr) << "PackageInstaller notification callback is null";
    
    // Reset event before triggering
    notification.ResetEvent();
    
    mPackageInstallerNotification_cb->OnAppInstallationStatus(testInstallationStatus);

    // TC-11: Test StartPreinstall with same version available - should skip installation
    Exchange::IPackageInstaller::Package existingPackage;
    existingPackage.packageId = packageId;
    existingPackage.version = PREINSTALL_PACKAGE_VERSION; // Same version
    existingPackage.state = Exchange::IPackageInstaller::InstallState::INSTALLED;

    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillOnce(::testing::Invoke([&](Exchange::IPackageInstaller::IPackageIterator*& iterator) {
            iterator = mPackageIteratorMock;
            fillPackageIterator(existingPackage);
            return Core::ERROR_NONE;
        }));

    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const string& fileLocator, string& packageId, string& version, Exchange::RuntimeConfig& config) {
            packageId = this->packageId;
            version = this->version;
            return Core::ERROR_NONE;
        }));

    // Install should not be called for same version
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    // Wait for notification with shorter timeout
    uint32_t eventReceived = notification.WaitForEvent(TIMEOUT);
    EXPECT_EQ(eventReceived, PreinstallManager_onAppInstallationStatus);
    EXPECT_EQ(notification.m_lastInstallationStatus, testInstallationStatus);

    forceInstall = false;
    
    EXPECT_EQ(Core::ERROR_NONE, interface->StartPreinstall(forceInstall));
    result = preinstallInterface->Unregister(&notification);
    EXPECT_EQ(result, Core::ERROR_NONE);

    releaseResources();
    cleanup();
    preinstallInterface = nullptr;
}
