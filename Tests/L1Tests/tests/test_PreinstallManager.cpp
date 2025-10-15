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
#include "Module.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TIMEOUT   (1000)

#define PREINSTALL_PACKAGE_ID           "com.test.preinstall.app"
#define PREINSTALL_PACKAGE_VERSION      "1.0.0"
#define PREINSTALL_PACKAGE_FILELOCATION "/opt/preinstall/testapp/package.wgt"
#define PREINSTALL_NEWER_VERSION        "2.0.0"
#define PREINSTALL_OLDER_VERSION        "0.9.0"

typedef enum : uint32_t {
    PreinstallManager_invalidEvent = 0,
    PreinstallManager_onAppInstallationStatusEvent
} PreinstallManagerTest_events_t;

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

    PreinstallManagerTest()
	: workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        mPreinstallManagerImpl = Core::ProxyType<Plugin::PreinstallManagerImplementation>::Create();
        
        interface = static_cast<Exchange::IPreinstallManager*>(mPreinstallManagerImpl->QueryInterface(Exchange::IPreinstallManager::ID));

		Core::IWorkerPool::Assign(&(*workerPool));
		workerPool->Run();
    }

    virtual ~PreinstallManagerTest() override
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
        mServiceMock = new NiceMock<ServiceMock>;
        mPackageInstallerMock = new NiceMock<PackageInstallerMock>;
        mPackageIteratorMock = new NiceMock<PackageIteratorMock>;

        mPreinstallManagerConfigure = static_cast<Exchange::IConfiguration*>(mPreinstallManagerImpl->QueryInterface(Exchange::IConfiguration::ID));
		
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
    }

    void releaseResources()
    {
	    // Clean up mocks
		if (mServiceMock != nullptr)
        {
			EXPECT_CALL(*mServiceMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mServiceMock;
						mServiceMock = nullptr;
						return 0;
					}));    
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
        }

        if (mPackageIteratorMock != nullptr)
        {
			EXPECT_CALL(*mPackageIteratorMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mPackageIteratorMock;
						mPackageIteratorMock = nullptr;
						return 0;
					}));
        }

		// Clean up the PreinstallManager
        mPreinstallManagerConfigure->Release();
		
		ASSERT_TRUE(interface != nullptr); 
    }

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

    Core::Sink<NotificationTest> notification;

    // TC-1: Check if the notification is registered successfully
    EXPECT_EQ(Core::ERROR_NONE, interface->Register(&notification));

    EXPECT_EQ(Core::ERROR_NONE, interface->Unregister(&notification));

    releaseResources();
}

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

    Core::Sink<NotificationTest> notification;
	
	// TC-2: Check if the notification unregistration fails without registering
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Unregister(&notification));

    releaseResources();
}

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
 */
TEST_F(PreinstallManagerTest, StartPreinstall_NewerVersion_Success)
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

    forceInstall = false;
    
    EXPECT_EQ(Core::ERROR_NONE, interface->StartPreinstall(forceInstall));

    releaseResources();
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
 */
TEST_F(PreinstallManagerTest, StartPreinstall_OlderVersion_Skip)
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

    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const string& packageId, const string& version, Exchange::IPackageInstaller::IKeyValueIterator* const& metadata, const string& fileLocator, Exchange::IPackageInstaller::FailReason& reason) {
            reason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL;
        }));

    forceInstall = true;
    
    EXPECT_EQ(Core::ERROR_GENERAL, interface->StartPreinstall(forceInstall));

    releaseResources();
}

/* Test Case for StartPreinstall with invalid package configuration
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Mock GetConfigForPackage to return error (invalid package)
 * Call StartPreinstall with forceInstall = true
 * Verify preinstall failure by asserting that StartPreinstall() returns Core::ERROR_GENERAL
 * Release the PreinstallManager interface object and clean-up related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstall_InvalidPackageConfig)
{
    createResources();

    // TC-7: Test StartPreinstall with invalid package configuration
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_GENERAL));

    // Install should not be called for invalid packages
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    forceInstall = true;
    
    EXPECT_EQ(Core::ERROR_GENERAL, interface->StartPreinstall(forceInstall));

    releaseResources();
}

/* Test Case for handleOnAppInstallationStatus event handling
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Call handleOnAppInstallationStatus with valid JSON response
 * Verify that the event is properly handled and dispatched to registered notifications
 * Release the PreinstallManager interface object and clean-up related test resources
 */
TEST_F(PreinstallManagerTest, HandleOnAppInstallationStatus_Success)
{
    createResources();

    Core::Sink<NotificationTest> notification;
    interface->Register(&notification);

    // TC-8: Test handleOnAppInstallationStatus event handling
    string testJsonResponse = R"({"packageId":"com.test.app","version":"1.0.0","status":"INSTALLED"})";
    
    mPreinstallManagerImpl->handleOnAppInstallationStatus(testJsonResponse);

    // Wait for event to be processed with timeout
    uint32_t result = notification.WaitForEventStatus(TIMEOUT, PreinstallManager_onAppInstallationStatusEvent);
    EXPECT_EQ(PreinstallManager_onAppInstallationStatusEvent, result);
    EXPECT_EQ(testJsonResponse, notification.receivedJsonResponse);

    interface->Unregister(&notification);

    releaseResources();
}

/* Test Case for handleOnAppInstallationStatus with empty JSON response
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Call handleOnAppInstallationStatus with empty JSON response
 * Verify that the event handling gracefully handles empty response
 * Release the PreinstallManager interface object and clean-up related test resources
 */
TEST_F(PreinstallManagerTest, HandleOnAppInstallationStatus_EmptyResponse)
{
    createResources();

    Core::Sink<NotificationTest> notification;
    interface->Register(&notification);

    // TC-9: Test handleOnAppInstallationStatus with empty response
    string emptyJsonResponse = "";
    
    mPreinstallManagerImpl->handleOnAppInstallationStatus(emptyJsonResponse);

    // Wait for event - should timeout since empty response shouldn't trigger event
    uint32_t result = notification.WaitForEventStatus(200, PreinstallManager_onAppInstallationStatusEvent);
    EXPECT_EQ(PreinstallManager_invalidEvent, result); // Should timeout

    interface->Unregister(&notification);

    releaseResources();
}

/* Test Case for StartPreinstall when ListPackages fails
 * 
 * Set up PreinstallManager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set forceInstall to false to trigger ListPackages call
 * Mock ListPackages to return error
 * Call StartPreinstall with forceInstall = false
 * Verify preinstall failure by asserting that StartPreinstall() returns Core::ERROR_GENERAL
 * Release the PreinstallManager interface object and clean-up related test resources
 */
TEST_F(PreinstallManagerTest, StartPreinstall_ListPackagesFailure)
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

    forceInstall = false;
    
    EXPECT_EQ(Core::ERROR_GENERAL, interface->StartPreinstall(forceInstall));

    releaseResources();
}

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

    forceInstall = false;
    
    EXPECT_EQ(Core::ERROR_NONE, interface->StartPreinstall(forceInstall));

    releaseResources();
}
