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
#include <mntent.h>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include "DownloadManager.h"
#include "DownloadManagerImplementation.h"
#include "ISubSystemMock.h"
#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"
#include "FactoriesImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TIMEOUT   (500)

using ::testing::NiceMock;
using namespace WPEFramework;
using namespace std;

typedef enum : uint32_t {
    DownloadManager_invalidStatus = 0,
    DownloadManager_AppDownloadStatus
} DownloadManagerTest_status_t;

struct StatusParams {
    string downloadId;
    string fileLocator;
    Exchange::IDownloadManager::FailReason reason;
};

class DownloadManagerTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    ServiceMock* mServiceMock = nullptr;
    SubSystemMock* mSubSystemMock = nullptr;

    Core::ProxyType<Plugin::DownloadManager> plugin;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    Core::JSONRPC::Message message;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;
    string uri;

    PLUGINHOST_DISPATCHER *dispatcher;
    FactoriesImplementation factoriesImplementation;

    Core::ProxyType<Plugin::DownloadManagerImplementation> mDownloadManagerImpl;
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    Exchange::IDownloadManager* downloadManagerInterface = nullptr;
    Exchange::IDownloadManager::Options options;
    string downloadId;
    uint8_t progress;
    uint32_t quotaKB, usedKB;

    DownloadManagerTest()
        : mJsonRpcHandler(*(plugin->GetHandler()))
    {
    }

    string getDownloadParams(string &url, bool priority = false, 
                           uint32_t retries = 2, uint32_t rateLimit = 0)
    {
        Core::JSON::String jsonUrl(url);
        Core::JSON::Boolean jsonPriority(priority);
        Core::JSON::DecUInt32 jsonRetries(retries);
        Core::JSON::DecUInt32 jsonRateLimit(rateLimit);

        JsonObject params;
        params["url"] = jsonUrl.Value();
        params["priority"] = jsonPriority.Value();
        params["retries"] = jsonRetries.Value();
        params["rateLimit"] = jsonRateLimit.Value();

        JsonObject options;
        options["priority"] = jsonPriority.Value();
        options["retries"] = jsonRetries.Value();
        options["rateLimit"] = jsonRateLimit.Value();

        JsonObject jsonRequest;
        jsonRequest["url"] = jsonUrl.Value();
        jsonRequest["options"] = options;

        string parameters;
        jsonRequest.ToString(parameters);
        return parameters;
    }

    void setupGeneralExpectations()
    {
        ON_CALL(*mServiceMock, ConfigLine())
            .WillByDefault(::testing::Return("{}"));
        ON_CALL(*mServiceMock, PersistentPath())
            .WillByDefault(::testing::Return("/tmp/"));
        ON_CALL(*mServiceMock, VolatilePath())
            .WillByDefault(::testing::Return("/tmp/"));
        ON_CALL(*mServiceMock, DataPath())
            .WillByDefault(::testing::Return("/tmp/"));
        ON_CALL(*mServiceMock, SubSystems())
            .WillByDefault(::testing::Return(mSubSystemMock));
        
        EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
            .WillRepeatedly(::testing::Return(true));
    }

    void setupNoInternetExpectations()
    {
        ON_CALL(*mServiceMock, ConfigLine())
            .WillByDefault(::testing::Return("{}"));
        ON_CALL(*mServiceMock, PersistentPath())
            .WillByDefault(::testing::Return("/tmp/"));
        ON_CALL(*mServiceMock, VolatilePath())
            .WillByDefault(::testing::Return("/tmp/"));
        ON_CALL(*mServiceMock, DataPath())
            .WillByDefault(::testing::Return("/tmp/"));
        ON_CALL(*mServiceMock, SubSystems())
            .WillByDefault(::testing::Return(mSubSystemMock));
        
        EXPECT_CALL(*mSubSystemMock, IsActive(PluginHost::ISubSystem::INTERNET))
            .WillRepeatedly(::testing::Return(false));
    }

    void setupJsonRpcExpectations()
    {
        setupGeneralExpectations();
        mDownloadManagerImpl = Core::ProxyType<Plugin::DownloadManagerImplementation>::Create();
        ASSERT_TRUE(mDownloadManagerImpl != nullptr);
        ASSERT_EQ(mDownloadManagerImpl->Initialize(mServiceMock), Core::ERROR_NONE);
    }

    void setupComRpcExpectations()
    {
        setupGeneralExpectations();
        downloadManagerInterface = mServiceMock->QueryInterface<Exchange::IDownloadManager>();
        mDownloadManagerImpl = Core::ProxyType<Plugin::DownloadManagerImplementation>::Create();
        ASSERT_TRUE(mDownloadManagerImpl != nullptr);
        ASSERT_EQ(mDownloadManagerImpl->Initialize(mServiceMock), Core::ERROR_NONE);
        downloadManagerInterface = mDownloadManagerImpl;
    }

    void setupNoInternetJsonRpcExpectations()
    {
        setupNoInternetExpectations();
        mDownloadManagerImpl = Core::ProxyType<Plugin::DownloadManagerImplementation>::Create();
        ASSERT_TRUE(mDownloadManagerImpl != nullptr);
        ASSERT_EQ(mDownloadManagerImpl->Initialize(mServiceMock), Core::ERROR_NONE);
    }

    void setupNoInternetComRpcExpectations()
    {
        setupNoInternetExpectations();
        downloadManagerInterface = mServiceMock->QueryInterface<Exchange::IDownloadManager>();
        mDownloadManagerImpl = Core::ProxyType<Plugin::DownloadManagerImplementation>::Create();
        ASSERT_TRUE(mDownloadManagerImpl != nullptr);
        ASSERT_EQ(mDownloadManagerImpl->Initialize(mServiceMock), Core::ERROR_NONE);
        downloadManagerInterface = mDownloadManagerImpl;
    }

    void cleanupJsonRpcExpectations()
    {
        if (mDownloadManagerImpl != nullptr) {
            mDownloadManagerImpl->Deinitialize(mServiceMock);
        }
    }

    void cleanupComRpcExpectations()
    {
        if (downloadManagerInterface != nullptr) {
            downloadManagerInterface = nullptr;
        }
        if (mDownloadManagerImpl != nullptr) {
            mDownloadManagerImpl->Deinitialize(mServiceMock);
        }
    }
};

class NotificationTest : public Exchange::IDownloadManager::INotification
{
    private:
        BEGIN_INTERFACE_MAP(NotificationTest)
        INTERFACE_ENTRY(Exchange::IDownloadManager::INotification)
        END_INTERFACE_MAP

    public:
        NotificationTest() = default;
        ~NotificationTest() override = default;
        std::mutex m_mutex;
        std::condition_variable m_condition_variable;
        uint32_t m_status_signal = DownloadManager_invalidStatus;

        StatusParams m_status_param;

        void OnAppDownloadStatus(const string& downloadStatus) override {
            std::unique_lock<std::mutex> lock(m_mutex);
            
            JsonArray list;
            list.FromString(downloadStatus);
            
            if (list.Length() > 0) {
                JsonObject obj = list[0].Object();
                m_status_param.downloadId = obj["downloadId"].String();
                m_status_param.fileLocator = obj["fileLocator"].String();
                
                if (obj.HasLabel("failReason")) {
                    string reason = obj["failReason"].String();
                    if (reason == "DOWNLOAD_FAILURE") {
                        m_status_param.reason = Exchange::IDownloadManager::FailReason::DOWNLOAD_FAILURE;
                    } else if (reason == "DISK_PERSISTENCE_FAILURE") {
                        m_status_param.reason = Exchange::IDownloadManager::FailReason::DISK_PERSISTENCE_FAILURE;
                    }
                }
            }
            
            m_status_signal = DownloadManager_AppDownloadStatus;
            m_condition_variable.notify_all();
        }

        uint32_t Wait(uint32_t timeoutMs, uint32_t expected_status) {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::system_clock::now();
            auto timeout = now + std::chrono::milliseconds(timeoutMs);

            while ((m_status_signal != expected_status) && (std::chrono::system_clock::now() < timeout)) {
                m_condition_variable.wait_until(lock, timeout);
            }

            return m_status_signal;
        }

        void Reset() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_status_signal = DownloadManager_invalidStatus;
            m_status_param = {};
        }
};

void DownloadManagerTest::SetUp()
{
    mServiceMock = new NiceMock<ServiceMock>;
    mSubSystemMock = new NiceMock<SubSystemMock>;

    plugin = Core::ProxyType<Plugin::DownloadManager>::Create();
    ASSERT_TRUE(plugin != nullptr);

    dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(plugin.operator->());
    
    uri = "/Service/DownloadManager";
    mJsonRpcResponse.clear();
}

void DownloadManagerTest::TearDown()
{
    plugin.Release();

    if (mServiceMock != nullptr) {
        delete mServiceMock;
        mServiceMock = nullptr;
    }

    if (mSubSystemMock != nullptr) {
        delete mSubSystemMock;
        mSubSystemMock = nullptr;
    }
}

/* Test Case for verifying registered methods using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Check if the methods listed exist by using the Exists() from the JSON RPC handler
 * Verify the methods exist by asserting that Exists() returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, registeredMethodsusingJsonRpc) {
    setupJsonRpcExpectations();

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists("download"));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists("pause"));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists("resume"));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists("cancel"));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists("delete"));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists("progress"));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists("getStorageDetails"));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists("rateLimit"));

    cleanupJsonRpcExpectations();
}

/* Test Case for adding download request to a regular queue using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, notifications/events, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters 
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, downloadMethodusingJsonRpcSuccess) {
    setupJsonRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(mDownloadManagerImpl->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/file.zip";
    string parameters = getDownloadParams(url, false, 2, 100);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "download", parameters, mJsonRpcResponse));
    
    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    EXPECT_TRUE(jsonResponse.HasLabel("downloadId"));
    downloadId = jsonResponse["downloadId"].String();
    EXPECT_FALSE(downloadId.empty());

    // Wait for download status notification
    EXPECT_EQ(notification.Wait(TIMEOUT, DownloadManager_AppDownloadStatus), DownloadManager_AppDownloadStatus);
    EXPECT_EQ(notification.m_status_param.downloadId, downloadId);

    ASSERT_EQ(mDownloadManagerImpl->Unregister(&notification), Core::ERROR_NONE);
    cleanupJsonRpcExpectations();
}

/* Test Case for checking download request error when internet is unavailable using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the method using the JSON RPC handler, passing the required parameters
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, downloadMethodusingJsonRpcError) {
    setupNoInternetJsonRpcExpectations();

    string url = "http://example.com/file.zip";
    string parameters = getDownloadParams(url);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, mJsonRpcHandler.Invoke(connection, "download", parameters, mJsonRpcResponse));

    cleanupJsonRpcExpectations();
}

/* Test Case for adding download request to a priority queue using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters, setting priority as true and wait
 * Verify successful download request by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, downloadMethodsusingComRpcSuccess) {
    setupComRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(downloadManagerInterface->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/file.zip";
    options.priority = true;
    options.retries = 2;
    options.rateLimit = 100;

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(url, options, downloadId));
    EXPECT_FALSE(downloadId.empty());

    // Wait for download status notification
    EXPECT_EQ(notification.Wait(TIMEOUT, DownloadManager_AppDownloadStatus), DownloadManager_AppDownloadStatus);
    EXPECT_EQ(notification.m_status_param.downloadId, downloadId);

    ASSERT_EQ(downloadManagerInterface->Unregister(&notification), Core::ERROR_NONE);
    cleanupComRpcExpectations();
}

/* Test Case for checking download request error when internet is unavailable using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, downloadMethodsusingComRpcError) {
    setupNoInternetComRpcExpectations();

    string url = "http://example.com/file.zip";
    options.priority = false;
    options.retries = 2;
    options.rateLimit = 0;

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, downloadManagerInterface->Download(url, options, downloadId));

    cleanupComRpcExpectations();
}

/* Test Case for pausing download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, pauseMethodusingJsonRpcSuccess) {
    setupJsonRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(mDownloadManagerImpl->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/largefile.zip";
    string parameters = getDownloadParams(url);

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "download", parameters, mJsonRpcResponse));
    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    downloadId = jsonResponse["downloadId"].String();

    // Pause download
    JsonObject pauseParams;
    pauseParams["downloadId"] = downloadId;
    string pauseParamsStr;
    pauseParams.ToString(pauseParamsStr);
    
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "pause", pauseParamsStr, mJsonRpcResponse));

    // Cancel download for cleanup
    JsonObject cancelParams;
    cancelParams["downloadId"] = downloadId;
    string cancelParamsStr;
    cancelParams.ToString(cancelParamsStr);
    
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "cancel", cancelParamsStr, mJsonRpcResponse));

    ASSERT_EQ(mDownloadManagerImpl->Unregister(&notification), Core::ERROR_NONE);
    cleanupJsonRpcExpectations();
}

/* Test Case for pausing failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the pause method using the JSON RPC handler, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, pauseMethodusingJsonRpcFailure) {
    setupJsonRpcExpectations();

    JsonObject pauseParams;
    pauseParams["downloadId"] = "invalid_id";
    string pauseParamsStr;
    pauseParams.ToString(pauseParamsStr);

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, "pause", pauseParamsStr, mJsonRpcResponse));

    cleanupJsonRpcExpectations();
}

/* Test Case for pausing download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notificatios/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, pauseMethodusingComRpcSuccess) {
    setupComRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(downloadManagerInterface->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/largefile.zip";
    options.priority = false;
    options.retries = 2;
    options.rateLimit = 0;

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(url, options, downloadId));
    EXPECT_FALSE(downloadId.empty());

    // Pause download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Pause(downloadId));

    // Cancel download for cleanup
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Cancel(downloadId));

    ASSERT_EQ(downloadManagerInterface->Unregister(&notification), Core::ERROR_NONE);
    cleanupComRpcExpectations();
}

/* Test Case for pausing failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the pause method using the COM RPC interface, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, pauseMethodusingComRpcFailure) {
    setupComRpcExpectations();

    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Pause("invalid_id"));

    cleanupComRpcExpectations();
}

/* Test Case for resuming download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the resume method using the JSON RPC handler, passing the downloadId
 * Verify that the resume method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, resumeMethodusingJsonRpcSuccess) {
    setupJsonRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(mDownloadManagerImpl->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/largefile.zip";
    string parameters = getDownloadParams(url);

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "download", parameters, mJsonRpcResponse));
    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    downloadId = jsonResponse["downloadId"].String();

    // Pause download
    JsonObject pauseParams;
    pauseParams["downloadId"] = downloadId;
    string pauseParamsStr;
    pauseParams.ToString(pauseParamsStr);
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "pause", pauseParamsStr, mJsonRpcResponse));

    // Resume download
    JsonObject resumeParams;
    resumeParams["downloadId"] = downloadId;
    string resumeParamsStr;
    resumeParams.ToString(resumeParamsStr);
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "resume", resumeParamsStr, mJsonRpcResponse));

    // Cancel download for cleanup
    JsonObject cancelParams;
    cancelParams["downloadId"] = downloadId;
    string cancelParamsStr;
    cancelParams.ToString(cancelParamsStr);
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "cancel", cancelParamsStr, mJsonRpcResponse));

    ASSERT_EQ(mDownloadManagerImpl->Unregister(&notification), Core::ERROR_NONE);
    cleanupJsonRpcExpectations();
}

 /* Test Case for resuming failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the resume method using the JSON RPC handler, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(DownloadManagerTest, resumeMethodusingJsonRpcFailure) {
    setupJsonRpcExpectations();

    JsonObject resumeParams;
    resumeParams["downloadId"] = "invalid_id";
    string resumeParamsStr;
    resumeParams.ToString(resumeParamsStr);

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, "resume", resumeParamsStr, mJsonRpcResponse));

    cleanupJsonRpcExpectations();
}

 /* Test Case for resuming download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the resume method using the COM RPC interface, passing the downloadId
 * Verify successful resume by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, resumeMethodusingComRpcSuccess) {
    setupComRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(downloadManagerInterface->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/largefile.zip";
    options.priority = false;
    options.retries = 2;
    options.rateLimit = 0;

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(url, options, downloadId));
    EXPECT_FALSE(downloadId.empty());

    // Pause download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Pause(downloadId));

    // Resume download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Resume(downloadId));

    // Cancel download for cleanup
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Cancel(downloadId));

    ASSERT_EQ(downloadManagerInterface->Unregister(&notification), Core::ERROR_NONE);
    cleanupComRpcExpectations();
}

 /* Test Case for resuming failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the resume method using the COM RPC interface, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

  TEST_F(DownloadManagerTest, resumeMethodusingComRpcFailure) {
    setupComRpcExpectations();

    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Resume("invalid_id"));

    cleanupComRpcExpectations();
}

/* Test Case for cancelling download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, cancelMethodusingJsonRpcSuccess) {
    setupJsonRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(mDownloadManagerImpl->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/largefile.zip";
    string parameters = getDownloadParams(url);

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "download", parameters, mJsonRpcResponse));
    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    downloadId = jsonResponse["downloadId"].String();

    // Pause download
    JsonObject pauseParams;
    pauseParams["downloadId"] = downloadId;
    string pauseParamsStr;
    pauseParams.ToString(pauseParamsStr);
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "pause", pauseParamsStr, mJsonRpcResponse));

    // Cancel download
    JsonObject cancelParams;
    cancelParams["downloadId"] = downloadId;
    string cancelParamsStr;
    cancelParams.ToString(cancelParamsStr);
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "cancel", cancelParamsStr, mJsonRpcResponse));

    ASSERT_EQ(mDownloadManagerImpl->Unregister(&notification), Core::ERROR_NONE);
    cleanupJsonRpcExpectations();
}

/* Test Case for cancelling failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the cancel method using the JSON RPC handler, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(DownloadManagerTest, cancelMethodusingJsonRpcFailure) {
    setupJsonRpcExpectations();

    JsonObject cancelParams;
    cancelParams["downloadId"] = "invalid_id";
    string cancelParamsStr;
    cancelParams.ToString(cancelParamsStr);

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, "cancel", cancelParamsStr, mJsonRpcResponse));

    cleanupJsonRpcExpectations();
}

/* Test Case for cancelling download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, cancelMethodusingComRpcSuccess) {
    setupComRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(downloadManagerInterface->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/largefile.zip";
    options.priority = false;
    options.retries = 2;
    options.rateLimit = 0;

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(url, options, downloadId));
    EXPECT_FALSE(downloadId.empty());

    // Pause download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Pause(downloadId));

    // Cancel download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Cancel(downloadId));

    ASSERT_EQ(downloadManagerInterface->Unregister(&notification), Core::ERROR_NONE);
    cleanupComRpcExpectations();
}

/* Test Case for cancelling failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the cancel method using the COM RPC interface, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, cancelMethodusingComRpcFailure) {
    setupComRpcExpectations();

    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Cancel("invalid_id"));

    cleanupComRpcExpectations();
}

/* Test Case for delete download using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, notifications/events, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the delete method using the JSON RPC handler, passing the fileLocator
 * Verify successful delete by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingJsonRpcSuccess) {
    setupJsonRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(mDownloadManagerImpl->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/file.zip";
    string parameters = getDownloadParams(url);

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "download", parameters, mJsonRpcResponse));
    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    downloadId = jsonResponse["downloadId"].String();

    // Wait for download completion
    EXPECT_EQ(notification.Wait(TIMEOUT, DownloadManager_AppDownloadStatus), DownloadManager_AppDownloadStatus);
    string fileLocator = notification.m_status_param.fileLocator;
    EXPECT_FALSE(fileLocator.empty());

    // Delete file
    JsonObject deleteParams;
    deleteParams["fileLocator"] = fileLocator;
    string deleteParamsStr;
    deleteParams.ToString(deleteParamsStr);
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "delete", deleteParamsStr, mJsonRpcResponse));

    ASSERT_EQ(mDownloadManagerImpl->Unregister(&notification), Core::ERROR_NONE);
    cleanupJsonRpcExpectations();
}

/* Test Case for delete failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the delete method using the JSON RPC handler, passing fileLocator
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingJsonRpcFailure) {
    setupJsonRpcExpectations();

    JsonObject deleteParams;
    deleteParams["fileLocator"] = "/invalid/path/file.zip";
    string deleteParamsStr;
    deleteParams.ToString(deleteParamsStr);

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, "delete", deleteParamsStr, mJsonRpcResponse));

    cleanupJsonRpcExpectations();
}

/* Test Case for delete download using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the delete method using the COM RPC interface, passing the fileLocator
 * Verify successful delete by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingComRpcSuccess) {
    setupComRpcExpectations();

    NotificationTest notification;
    ASSERT_EQ(downloadManagerInterface->Register(&notification), Core::ERROR_NONE);

    string url = "http://example.com/file.zip";
    options.priority = false;
    options.retries = 2;
    options.rateLimit = 0;

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(url, options, downloadId));
    EXPECT_FALSE(downloadId.empty());

    // Wait for download completion
    EXPECT_EQ(notification.Wait(TIMEOUT, DownloadManager_AppDownloadStatus), DownloadManager_AppDownloadStatus);
    string fileLocator = notification.m_status_param.fileLocator;
    EXPECT_FALSE(fileLocator.empty());

    // Delete file
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Delete(fileLocator));

    ASSERT_EQ(downloadManagerInterface->Unregister(&notification), Core::ERROR_NONE);
    cleanupComRpcExpectations();
}

/* Test Case for delete failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the delete method using the COM RPC interface, passing fileLocator
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, deleteMethodusingComRpcFailure) {
    setupComRpcExpectations();

    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Delete("/invalid/path/file.zip"));

    cleanupComRpcExpectations();
}

/* Test Case for progress retrieval using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the progress method using the JSON RPC handler, passing the downloadId
 * Verify successful progress retrieval by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingJsonRpcSuccess) {
    setupJsonRpcExpectations();

    string url = "http://example.com/largefile.zip";
    string parameters = getDownloadParams(url);

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "download", parameters, mJsonRpcResponse));
    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    downloadId = jsonResponse["downloadId"].String();

    // Get progress
    JsonObject progressParams;
    progressParams["downloadId"] = downloadId;
    string progressParamsStr;
    progressParams.ToString(progressParamsStr);
    
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "progress", progressParamsStr, mJsonRpcResponse));
    
    JsonObject progressResponse;
    EXPECT_TRUE(progressResponse.FromString(mJsonRpcResponse));
    EXPECT_TRUE(progressResponse.HasLabel("percent"));

    // Cancel download for cleanup
    JsonObject cancelParams;
    cancelParams["downloadId"] = downloadId;
    string cancelParamsStr;
    cancelParams.ToString(cancelParamsStr);
    mJsonRpcHandler.Invoke(connection, "cancel", cancelParamsStr, mJsonRpcResponse);

    cleanupJsonRpcExpectations();
}

/* Test Case for progress retrieval failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the progress method using the JSON RPC handler, passing invalid downloadId
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingJsonRpcFailure) {
    setupJsonRpcExpectations();

    JsonObject progressParams;
    progressParams["downloadId"] = "invalid_id";
    string progressParamsStr;
    progressParams.ToString(progressParamsStr);

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, "progress", progressParamsStr, mJsonRpcResponse));

    cleanupJsonRpcExpectations();
}

/* Test Case for progress retrieval using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the progress method using the COM RPC interface, passing the downloadId
 * Verify successful progress retrieval by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingComRpcSuccess) {
    setupComRpcExpectations();

    string url = "http://example.com/largefile.zip";
    options.priority = false;
    options.retries = 2;
    options.rateLimit = 0;

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(url, options, downloadId));
    EXPECT_FALSE(downloadId.empty());

    // Get progress
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Progress(downloadId, progress));
    EXPECT_GE(progress, 0);
    EXPECT_LE(progress, 100);

    // Cancel download for cleanup
    downloadManagerInterface->Cancel(downloadId);

    cleanupComRpcExpectations();
}

/* Test Case for progress retrieval failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the progress method using the COM RPC interface, passing invalid downloadId
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, progressMethodusingComRpcFailure) {
    setupComRpcExpectations();

    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Progress("invalid_id", progress));

    cleanupComRpcExpectations();
}

/* Test Case for storage details retrieval using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getStorageDetails method using the JSON RPC handler
 * Verify successful storage details retrieval by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, getStorageDetailsMethodusingJsonRpcSuccess) {
    setupJsonRpcExpectations();

    JsonObject storageParams;
    string storageParamsStr;
    storageParams.ToString(storageParamsStr);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "getStorageDetails", storageParamsStr, mJsonRpcResponse));
    
    JsonObject storageResponse;
    EXPECT_TRUE(storageResponse.FromString(mJsonRpcResponse));
    EXPECT_TRUE(storageResponse.HasLabel("quotaKb"));
    EXPECT_TRUE(storageResponse.HasLabel("usedKb"));

    cleanupJsonRpcExpectations();
}

/* Test Case for storage details retrieval using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the GetStorageDetails method using the COM RPC interface
 * Verify successful storage details retrieval by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, getStorageDetailsMethodusingComRpcSuccess) {
    setupComRpcExpectations();

    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->GetStorageDetails(quotaKB, usedKB));

    cleanupComRpcExpectations();
}

/* Test Case for rate limit setting using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the rateLimit method using the JSON RPC handler, passing the downloadId and limit
 * Verify successful rate limit setting by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitMethodusingJsonRpcSuccess) {
    setupJsonRpcExpectations();

    string url = "http://example.com/largefile.zip";
    string parameters = getDownloadParams(url);

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "download", parameters, mJsonRpcResponse));
    JsonObject jsonResponse;
    EXPECT_TRUE(jsonResponse.FromString(mJsonRpcResponse));
    downloadId = jsonResponse["downloadId"].String();

    // Set rate limit
    JsonObject rateLimitParams;
    rateLimitParams["downloadId"] = downloadId;
    rateLimitParams["limit"] = 50;
    string rateLimitParamsStr;
    rateLimitParams.ToString(rateLimitParamsStr);
    
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, "rateLimit", rateLimitParamsStr, mJsonRpcResponse));

    // Cancel download for cleanup
    JsonObject cancelParams;
    cancelParams["downloadId"] = downloadId;
    string cancelParamsStr;
    cancelParams.ToString(cancelParamsStr);
    mJsonRpcHandler.Invoke(connection, "cancel", cancelParamsStr, mJsonRpcResponse);

    cleanupJsonRpcExpectations();
}

/* Test Case for rate limit setting failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the rateLimit method using the JSON RPC handler, passing invalid downloadId
 * Verify rate limit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitMethodusingJsonRpcFailure) {
    setupJsonRpcExpectations();

    JsonObject rateLimitParams;
    rateLimitParams["downloadId"] = "invalid_id";
    rateLimitParams["limit"] = 50;
    string rateLimitParamsStr;
    rateLimitParams.ToString(rateLimitParamsStr);

    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, "rateLimit", rateLimitParamsStr, mJsonRpcResponse));

    cleanupJsonRpcExpectations();
}

/* Test Case for rate limit setting using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the RateLimit method using the COM RPC interface, passing the downloadId and limit
 * Verify successful rate limit setting by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitMethodusingComRpcSuccess) {
    setupComRpcExpectations();

    string url = "http://example.com/largefile.zip";
    options.priority = false;
    options.retries = 2;
    options.rateLimit = 0;

    // Start download
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Download(url, options, downloadId));
    EXPECT_FALSE(downloadId.empty());

    // Set rate limit
    uint32_t limit = 50;
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->RateLimit(downloadId, limit));

    // Cancel download for cleanup
    downloadManagerInterface->Cancel(downloadId);

    cleanupComRpcExpectations();
}

/* Test Case for rate limit setting failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the RateLimit method using the COM RPC interface, passing invalid downloadId
 * Verify rate limit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, rateLimitMethodusingComRpcFailure) {
    setupComRpcExpectations();

    uint32_t limit = 50;
    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->RateLimit("invalid_id", limit));

    cleanupComRpcExpectations();
}

/* Test Case for notification registration and unregistration
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Register notification handler and verify successful registration
 * Unregister notification handler and verify successful unregistration
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, notificationRegistrationSuccess) {
    setupComRpcExpectations();

    NotificationTest notification;
    
    // Register notification
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Register(&notification));
    
    // Unregister notification
    EXPECT_EQ(Core::ERROR_NONE, downloadManagerInterface->Unregister(&notification));

    cleanupComRpcExpectations();
}

/* Test Case for notification unregistration failure
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Try to unregister a notification that was never registered
 * Verify unregistration failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(DownloadManagerTest, notificationUnregistrationFailure) {
    setupComRpcExpectations();

    NotificationTest notification;
    
    // Try to unregister notification that was never registered
    EXPECT_EQ(Core::ERROR_GENERAL, downloadManagerInterface->Unregister(&notification));

    cleanupComRpcExpectations();
}
