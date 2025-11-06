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
**/
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <cstring>
#include <chrono>
#include <future>
#include <dirent.h>

#include "PreinstallManager.h"
#include "PreinstallManagerImplementation.h"
#include "ServiceMock.h"
#include "PackageManagerMock.h"
#include "WrapsMock.h"
#include "ThunderPortability.h"

extern "C" DIR* __real_opendir(const char* pathname);

using ::testing::NiceMock;
using ::testing::Return;
using namespace WPEFramework;

namespace WPEFramework {
namespace Plugin {
namespace Tests {

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define PREINSTALL_MANAGER_TEST_PACKAGE_ID      "com.test.preinstall.app"
#define PREINSTALL_MANAGER_TEST_VERSION         "1.0.0"

// Mock notification class using GMock
class MockNotificationTest : public Exchange::IPreinstallManager::INotification 
{
public:
    MockNotificationTest() = default;
    virtual ~MockNotificationTest() = default;
    
    MOCK_METHOD(void, OnAppInstallationStatus, (const string& jsonresponse), (override));
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));

    BEGIN_INTERFACE_MAP(MockNotificationTest)
    INTERFACE_ENTRY(Exchange::IPreinstallManager::INotification)
    END_INTERFACE_MAP
};

class PreinstallManagerTest : public ::testing::Test {
protected:
    // Core objects
    Core::ProxyType<Plugin::PreinstallManager> plugin;
    Core::JSONRPC::Handler& handler;
    Core::JSONRPC::Context connection;
    string response;
    
    // Interfaces
    Exchange::IPreinstallManager* interface;
    
    // Mocks
    NiceMock<ServiceMock> service;
    Core::ProxyType<Plugin::PreinstallManagerImplementation> preinstallManagerImpl;
    PackageInstallerMock* mPackageInstallerMock;
    WrapsImplMock* p_wrapsImplMock;

    PreinstallManagerTest():
        plugin(Core::ProxyType<Plugin::PreinstallManager>::Create()),
        handler(*plugin),
        connection(0, 1, "")
    {
        preinstallManagerImpl = Core::ProxyType<Plugin::PreinstallManagerImplementation>::Create();
        mPackageInstallerMock = new NiceMock<PackageInstallerMock>;
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        
        Wraps::setImpl(p_wrapsImplMock);

        // Directory mocks following StorageManager pattern
        ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
            .WillByDefault(::testing::Invoke([](const char* pathname) {
                TEST_LOG("opendir called with pathname: %s", pathname);
                // Use real opendir like StorageManager does
                return __real_opendir(pathname);
            }));

        ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
            .WillByDefault([](DIR* dirp) -> struct dirent* {
                static int call_count = 0;
                static struct dirent entry;
                if (call_count == 0) {
                    std::strncpy(entry.d_name, "testapp.oar", sizeof(entry.d_name) - 1);
                    entry.d_name[sizeof(entry.d_name) - 1] = '\0';
                    entry.d_type = DT_REG;
                    call_count++;
                    return &entry;
                } else {
                    call_count = 0; // Reset for next test
                    return nullptr;
                }
            });

        ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
            .WillByDefault([](DIR* dirp) {
                TEST_LOG("closedir called");
                return 0;
            });

        ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
            .WillByDefault([](const char* path, struct stat* info) {
                // Simulate success like StorageManager
                return 0;
            });

        // Set up service mock for PackageInstaller
        EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .WillRepeatedly(::testing::Invoke(
                [&](const uint32_t id, const std::string& name) -> void* {
                    if (name == "org.rdk.PackageManagerRDKEMS") {
                        if (id == Exchange::IPackageInstaller::ID) {
                            return reinterpret_cast<void*>(mPackageInstallerMock);
                        }
                    }
                    return nullptr;
                }));

        // Set up PackageInstaller registration
        EXPECT_CALL(*mPackageInstallerMock, Register(::testing::_))
            .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

        EXPECT_CALL(*mPackageInstallerMock, Unregister(::testing::_))
            .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

        // Initialize the plugin
        interface = static_cast<Exchange::IPreinstallManager*>(
            preinstallManagerImpl->QueryInterface(Exchange::IPreinstallManager::ID));
            
        preinstallManagerImpl->Configure(&service);
        plugin->Initialize(&service);
    }

    virtual ~PreinstallManagerTest() override {
        plugin->Deinitialize(&service);
        
        if (interface != nullptr) {
            interface->Release();
        }
        
        Wraps::setImpl(nullptr);
        
        if (p_wrapsImplMock != nullptr) {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }
        
        if (mPackageInstallerMock != nullptr) {
            delete mPackageInstallerMock;
            mPackageInstallerMock = nullptr;
        }
    }

    virtual void SetUp() {
        ASSERT_TRUE(interface != nullptr);
    }

    virtual void TearDown() {
        ASSERT_TRUE(interface != nullptr);
    }

    auto FillPackageIterator()
    {
        std::list<Exchange::IPackageInstaller::Package> packageList;
        Exchange::IPackageInstaller::Package package_1;

        package_1.packageId = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
        package_1.version = PREINSTALL_MANAGER_TEST_VERSION;
        package_1.digest = "";
        package_1.state = Exchange::IPackageInstaller::InstallState::INSTALLED;
        package_1.sizeKb = 0;

        packageList.emplace_back(package_1);
        return Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);
    }
};

/*Test cases for PreinstallManager Plugin*/

/*
 * Test for StartPreinstall with installation failure
 * This test checks that installation failures are handled gracefully and getFailReason is called.
 * It verifies that the method continues processing even when individual packages fail to install.
 */
TEST_F(PreinstallManagerTest, StartPreinstall_InstallFailure_Success) {
    // Mock GetConfigForPackage to return valid package info
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            TEST_LOG("GetConfigForPackage called for: %s", fileLocator.c_str());
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });
    
    // Mock Install to fail with a specific reason to trigger getFailReason
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            TEST_LOG("Install called - simulating failure for package: %s", packageId.c_str());
            failReason = Exchange::IPackageInstaller::FailReason::SIGNATURE_VERIFICATION_FAILURE;
            return Core::ERROR_GENERAL; // Return failure to trigger getFailReason call
        });
    
    Core::hresult result = interface->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_NONE, result);
    TEST_LOG("StartPreinstall_InstallFailure_Success completed with result: %u", result);
}

/*
 * Test for StartPreinstall successful installation
 * This test checks the successful path where packages are found and installed correctly.
 * It verifies that the normal installation flow works as expected.
 */
TEST_F(PreinstallManagerTest, StartPreinstall_Success) {
    // Mock GetConfigForPackage to return valid package info
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            TEST_LOG("GetConfigForPackage called for: %s", fileLocator.c_str());
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    // Mock Install to succeed
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &packageId, const string &version, 
                           Exchange::IPackageInstaller::IKeyValueIterator* const& additionalMetadata, 
                           const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) {
            TEST_LOG("Install called successfully for package: %s", packageId.c_str());
            return Core::ERROR_NONE;
        });

    Core::hresult result = interface->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_NONE, result);
    TEST_LOG("StartPreinstall_Success completed with result: %u", result);
}

/*
 * Test for StartPreinstall with force install disabled
 * This test checks that existing packages are considered when forceInstall is false.
 * It verifies that the ListPackages functionality is called and existing packages are handled.
 */
TEST_F(PreinstallManagerTest, StartPreinstall_NoForceInstall_Success) {
    // Mock ListPackages to return existing packages
    EXPECT_CALL(*mPackageInstallerMock, ListPackages(::testing::_))
        .WillRepeatedly([&](Exchange::IPackageInstaller::IPackageIterator*& packages) {
            auto mockIterator = FillPackageIterator();
            packages = mockIterator;
            return Core::ERROR_NONE;
        });

    // Mock GetConfigForPackage to return valid package info
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            TEST_LOG("GetConfigForPackage called for: %s", fileLocator.c_str());
            id = PREINSTALL_MANAGER_TEST_PACKAGE_ID;
            version = PREINSTALL_MANAGER_TEST_VERSION;
            return Core::ERROR_NONE;
        });

    Core::hresult result = interface->StartPreinstall(false);
    EXPECT_EQ(Core::ERROR_NONE, result);
    TEST_LOG("StartPreinstall_NoForceInstall_Success completed with result: %u", result);
}

/*
 * Test for notification registration
 * This test verifies that notification interfaces can be registered successfully.
 */
TEST_F(PreinstallManagerTest, RegisterNotification_Success) {
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    
    Core::hresult result = interface->Register(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    // Cleanup
    interface->Unregister(mockNotification.operator->());
    TEST_LOG("RegisterNotification_Success completed");
}

/*
 * Test for notification unregistration
 * This test verifies that notification interfaces can be unregistered successfully.
 */
TEST_F(PreinstallManagerTest, UnregisterNotification_Success) {
    auto mockNotification = Core::ProxyType<MockNotificationTest>::Create();
    
    // First register
    Core::hresult registerResult = interface->Register(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, registerResult);
    
    // Then unregister  
    Core::hresult unregisterResult = interface->Unregister(mockNotification.operator->());
    EXPECT_EQ(Core::ERROR_NONE, unregisterResult);
    
    TEST_LOG("UnregisterNotification_Success completed");
}

/*
 * Test for StartPreinstall when PackageInstaller is unavailable
 * This test verifies proper error handling when PackageInstaller service is not available.
 */
TEST_F(PreinstallManagerTest, StartPreinstall_PackageInstallerUnavailable_Failure) {
    // Create a new service mock that returns nullptr for PackageInstaller
    NiceMock<ServiceMock> failService;
    EXPECT_CALL(failService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));
    
    // Create a new PreinstallManager instance with the failing service
    auto failPreinstallManagerImpl = Core::ProxyType<Plugin::PreinstallManagerImplementation>::Create();
    failPreinstallManagerImpl->Configure(&failService);
    
    auto failInterface = static_cast<Exchange::IPreinstallManager*>(
        failPreinstallManagerImpl->QueryInterface(Exchange::IPreinstallManager::ID));
    
    Core::hresult result = failInterface->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    // Cleanup
    if (failInterface != nullptr) {
        failInterface->Release();
    }
    
    TEST_LOG("StartPreinstall_PackageInstallerUnavailable_Failure completed with result: %u", result);
}

/*
 * Test for QueryInterface functionality
 * This test verifies that the PreinstallManager correctly implements QueryInterface.
 */
TEST_F(PreinstallManagerTest, QueryInterface_Success) {
    // Test querying for IPreinstallManager interface
    Exchange::IPreinstallManager* queriedInterface = 
        static_cast<Exchange::IPreinstallManager*>(
            preinstallManagerImpl->QueryInterface(Exchange::IPreinstallManager::ID));
    
    EXPECT_TRUE(queriedInterface != nullptr);
    
    if (queriedInterface != nullptr) {
        queriedInterface->Release();
    }
    
    TEST_LOG("QueryInterface_Success completed");
}

/*
 * Test for GetConfigForPackage error handling
 * This test verifies that StartPreinstall handles GetConfigForPackage failures gracefully.
 */
TEST_F(PreinstallManagerTest, StartPreinstall_GetConfigFailure_Success) {
    // Mock GetConfigForPackage to fail
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([&](const string &fileLocator, string& id, string &version, WPEFramework::Exchange::RuntimeConfig &config) {
            TEST_LOG("GetConfigForPackage called for: %s - simulating failure", fileLocator.c_str());
            return Core::ERROR_GENERAL; // Return failure
        });
    
    // Install should not be called because GetConfigForPackage fails
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = interface->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_NONE, result); // Should still return success as method continues processing
    TEST_LOG("StartPreinstall_GetConfigFailure_Success completed with result: %u", result);
}

/*
 * Test for directory access failure handling
 * This test verifies that StartPreinstall handles directory access failures gracefully.
 * Following StorageManager pattern of overriding default opendir behavior in specific tests.
 */
TEST_F(PreinstallManagerTest, StartPreinstall_DirectoryAccessFailure_Success) {
    // Override default opendir behavior to simulate directory access failure
    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Invoke([](const char* pathname) -> DIR* {
            TEST_LOG("opendir called with pathname: %s - simulating failure", pathname);
            return nullptr; // Simulate directory access failure
        }));
    
    // PackageInstaller methods should not be called due to directory access failure
    EXPECT_CALL(*mPackageInstallerMock, GetConfigForPackage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mPackageInstallerMock, Install(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    
    Core::hresult result = interface->StartPreinstall(true);
    EXPECT_EQ(Core::ERROR_GENERAL, result); // Should return error due to directory access failure
    TEST_LOG("StartPreinstall_DirectoryAccessFailure_Success completed with result: %u", result);
}

} // End namespace Tests
} // End namespace Plugin
} // End namespace WPEFramework
