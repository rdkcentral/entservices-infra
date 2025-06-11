#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <cstring>
#include "StorageManager.h"
#include "StorageManagerImplementation.h"
#include "ServiceMock.h"
#include "Store2Mock.h"
#include "WrapsMock.h"
#include "ThunderPortability.h"
#include "COMLinkMock.h"
#include "RequestHandler.h"

using ::testing::NiceMock;
using ::testing::Return;
using namespace WPEFramework;

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

class StorageManagerTest : public ::testing::Test {
    protected:
        // //JSONRPC
        Core::ProxyType<Plugin::StorageManager> plugin; // create a proxy object
        Core::JSONRPC::Handler& handler;
        Core::JSONRPC::Context connection; // create a JSONRPC context
        string response; // create a string to hold the response
        Exchange::IConfiguration* storageManagerConfigure; // create a pointer to IConfiguration
        //comrpc 
        Exchange::IStorageManager* interface; // create a pointer to IStorageManager
        NiceMock<ServiceMock> service; // an instance of mock service object
        Core::ProxyType<Plugin::StorageManagerImplementation> StorageManagerImplementation; // declare an proxy object
        ServiceMock  *p_serviceMock  = nullptr;
        WrapsImplMock *p_wrapsImplMock   = nullptr;

        Store2Mock* mStore2Mock = nullptr;

        StorageManagerTest():
        plugin(Core::ProxyType<Plugin::StorageManager>::Create()),
        handler(*plugin),
        connection(0,1,"")
        {
            std::cout<<"veeksha entered constructor"<<std::endl;
            StorageManagerImplementation = Core::ProxyType<Plugin::StorageManagerImplementation>::Create();
            mStore2Mock = new NiceMock<Store2Mock>;

            p_wrapsImplMock  = new NiceMock <WrapsImplMock>;
            Wraps::setImpl(p_wrapsImplMock);
            static struct dirent mockDirent;
            std::memset(&mockDirent, 0, sizeof(mockDirent));
            std::strncpy(mockDirent.d_name, "mockApp", sizeof(mockDirent.d_name) - 1);
            mockDirent.d_type = DT_DIR;            
            ON_CALL(*mStore2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [](Exchange::IStore2::ScopeType scope, const std::string& appId, const std::string& key, std::string& value, uint32_t& ttl) -> uint32_t {
                    std::cout << "Mock GetValue called: scope=" << static_cast<int>(scope)
                            << ", appId=" << appId << ", key=" << key << std::endl;

                    if (key == "quotaSize") {
                        value = "1024"; // Simulate a valid numeric quota
                    } else {
                        value = "mockValue"; // Default value for other keys
                    }

                    ttl = 0;
                    return Core::ERROR_NONE;
                }));

            ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
                .WillByDefault(::testing::Invoke([](const char* pathname) {
                    std::cout << "Mock system called with command: " << pathname << std::endl;
                    // Simulate success
                    return reinterpret_cast<DIR*>(0xDEADBEEF); // Simulated DIR* pointer
                }));
            ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
                .WillByDefault([](DIR* dirp) -> struct dirent* {
                    static int call_count = 0;
                    static struct dirent entry;
                    if (call_count == 0) {
                        std::strncpy(entry.d_name, "mockApp", sizeof(entry.d_name) - 1);
                        entry.d_type = DT_DIR;
                        call_count++;
                        return &entry;
                    } else if (call_count == 1) {
                        std::strncpy(entry.d_name, "testApp", sizeof(entry.d_name) - 1);
                        entry.d_type = DT_DIR;
                        call_count++;
                        return &entry;
                    } else {
                        call_count = 0; // Reset for next traversal
                        return nullptr;
                    }
                });


            ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
                std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
                // Simulate success
                return 0;
            });

            ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
                .WillByDefault([](const char* path, struct stat* info) {
                    std::cout << "Mock stat called for path: " << path << std::endl;
                    // Simulate success
                    return 0;
                });

            ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
                .WillByDefault([](DIR* dirp) {
                    std::cout << "Mock closedir called for dirp: " << dirp << std::endl;
                    // Simulate success
                    return 0;
                });

            EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .WillRepeatedly(::testing::Invoke(
            [&](const uint32_t, const std::string& name) -> void* {
                std::cout << "[plugin call] name = " << name << std::endl;
                if (name == "org.rdk.PersistentStore") {
                    return reinterpret_cast<void*>(mStore2Mock);
                }
                std::cout << "[plugin call] name not matched" << std::endl;
                return nullptr;
            }));

            std::cout<<"Veeksha after QueryInterfaceByCallsign"<<std::endl;
            interface = static_cast<Exchange::IStorageManager*>(
                StorageManagerImplementation->QueryInterface(Exchange::IStorageManager::ID));

            storageManagerConfigure = static_cast<Exchange::IConfiguration*>(
            StorageManagerImplementation->QueryInterface(Exchange::IConfiguration::ID));
            StorageManagerImplementation->Configure(&service);
            plugin->Initialize(&service);
          
        }
        virtual ~StorageManagerTest() override {
            plugin->Deinitialize(&service);
            storageManagerConfigure->Release();
            std::cout<<"veeksha in destructor"<<std::endl;
            Wraps::setImpl(nullptr);
            if (p_wrapsImplMock != nullptr)
            {
                delete p_wrapsImplMock;
                p_wrapsImplMock = nullptr;
            }
            if (mStore2Mock != nullptr)
            {
                delete mStore2Mock;
                mStore2Mock = nullptr;
            }

        }
        virtual void SetUp()
        {
            ASSERT_TRUE(interface != nullptr);
        }
    
        virtual void TearDown()
        {
            ASSERT_TRUE(interface != nullptr);
        }
    };


// Test for CreateStorage
TEST_F(StorageManagerTest, CreateStorage_Success){
    std::cout<<"veeksha CreateStorage_Success"<<std::endl;
    // Create a storage object
    std::string appId = "testApp";

    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            std::cout << "Mock mkdir called for path: " << path << " with mode: " << mode << std::endl;
            return 0;
        });

    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            std::cout << "Mock access called for path: " << path << std::endl;
            // Simulate file exists
            return 0;
        });
        
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
            // Simulate success
            return 0;
        });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            std::cout << "Mock statvfs called for path: " << path << std::endl;
                  // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 500000; // Free blocks
            buf->f_bavail = 500000; // Available blocks
            return 0;
            });

    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, struct stat* info) {
            std::cout << "Mock stat called for path: " << path << std::endl;
            // Simulate success
            return 0;
        });
    
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        std::cout << "Mock SetValue called: scope=" << static_cast<int>(scope)
                  << ", appId=" << appId
                  << ", key=" << key
                  << ", value=" << value
                  << ", ttl=" << ttl << std::endl;
        // Simulate success
        return Core::ERROR_NONE;
    }));

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = "";   

    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));
}

TEST_F(StorageManagerTest, CreateStorage_PathExists_Success){
    // Create a storage object
    std::cout<<"veeksha CreateStorage_BasePathExists_Success"<<std::endl;

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
           std::cout << "Mock mkdir called for path: " << path << " with mode: " << mode << std::endl;
           errno = EEXIST;  // Directory already exists
            return -1;
        });
    
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
    .WillByDefault([](const char* pathname, int mode) {
        std::cout << "Mock access called for path: "<< std::endl;
        // Simulate file exists
        return 0;
    });
    
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
        std::cout << "Mock nftw called for dirpath: "<< std::endl;
        // Simulate success
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            std::cout << "Mock statvfs called for path: " << path << std::endl;
                  // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 500000; // Free blocks
            buf->f_bavail = 500000; // Available blocks
            return 0;
            });

    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, struct stat* info) {
            std::cout << "Mock stat called for path: " << path << std::endl;
            // Simulate success
            return 0;
        });
    
        ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        std::cout << "Mock SetValue called: scope=" << static_cast<int>(scope)
                  << ", appId=" << appId
                  << ", key=" << key
                  << ", value=" << value
                  << ", ttl=" << ttl << std::endl;
        // Simulate success
        return Core::ERROR_NONE;
    }));

   // initialize the StorageManagerImplementation
    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = "";
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));

}

//Test for CreateStorage
TEST_F(StorageManagerTest, CreateStoragemkdirFail_Failure){
    // Create a storage object
    std::cout<<"veeksha CreateStoragemkdirFail_Failure"<<std::endl;
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillOnce([](const char* path, mode_t mode) {
            errno = ENOTDIR;  // Serious error that should trigger failure
            return -1;
        });
    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = "";
    EXPECT_EQ(Core::ERROR_GENERAL, interface->CreateStorage(appId, size, path, errorReason));
    std::cout<<"veeksha CreateStoragemkdirFail_Failure errorReason = "<<errorReason<<std::endl;
}

TEST_F(StorageManagerTest, CreateStorage_Failure){
    // Create a storage object
    std::cout<<"veeksha CreateStorage_Failure"<<std::endl;
    std::string appId = "";
    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = "";
    EXPECT_EQ(Core::ERROR_GENERAL, interface->CreateStorage(appId, size, path, errorReason));
    std::cout<<"veeksha CreateStorage_Failure errorReason = "<<errorReason<<std::endl;

}

//Test for CreateStorage
TEST_F(StorageManagerTest, CreateStorageSizeExceeded_Failure){
    // Create a storage object
    std::cout<<"veeksha CreateStorageSizeExceeded_Failure"<<std::endl;
    std::string appId = "testApp";
    uint32_t size = 1000000;
    std::string path = " ";
    std::string errorReason = "";
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            std::cout << "Mock mkdir called for path: " << path << " with mode: " << mode << std::endl;

            return 0;
        });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            std::cout << "Mock access called for path: " << path << std::endl;
            // Simulate file exists
            return 0;
        });
    EXPECT_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
            // Simulate success
            return 0;
        });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct statvfs* buf) {
        std::cout << "Mock statvfs called for path: " << path << std::endl;
                // Simulate success
        buf->f_bsize = 4096; // Block size
        buf->f_frsize = 4096; // Fragment size
        buf->f_blocks = 100000; // Total blocks
        buf->f_bfree = 0; // Free blocks
        buf->f_bavail = 0; // Available blocks
        std::cout << "Mock statvfs: f_bsize = " << buf->f_bsize
          << ", f_frsize = " << buf->f_frsize
          << ", f_blocks = " << buf->f_blocks
          << ", f_bfree = " << buf->f_bfree
          << ", f_bavail = " << buf->f_bavail << std::endl;

        return 0;
        });
    
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        std::cout << "Mock SetValue called: scope=" << static_cast<int>(scope)
                  << ", appId=" << appId
                  << ", key=" << key
                  << ", value=" << value
                  << ", ttl=" << ttl << std::endl;
        // Simulate success
        return Core::ERROR_NONE;
    }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->CreateStorage(appId, size, path, errorReason));
    std::cout<<"veeksha CreateStorageSizeExceeded_Failure errorReason = "<<errorReason<<std::endl;
}

// // Test for GetStorageInfo success
TEST_F(StorageManagerTest, GetStorage_Success){
    std::cout<<"veeksha GetStorage_Success"<<std::endl;
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            std::cout << "Mock mkdir called for path: " << path << " with mode: " << mode << std::endl;

            return 0;
        });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            std::cout << "Mock access called for path: " << path << std::endl;
            // Simulate file exists
            return 0;
        });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
            // Simulate success
            return 0;
        });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            std::cout << "Mock statvfs called for path: " << path << std::endl;
                  // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
            });
    
    EXPECT_CALL(*p_wrapsImplMock, chown(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, int32_t owner, int32_t group) {
            std::cout << "Mock chown called for path: " << path << " with owner: " << owner << " and group: " << group << std::endl;
            // Simulate success
            return 0;
        });
    

    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string errorReason = "";
    uint32_t userId = 100;
    uint32_t groupId = 101;
    std::string path = "";
    uint32_t used = 0;
    
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, interface->GetStorage(appId, userId, groupId, path, size, used));
    std::cout<<"veeksha GetStorage_Success path = "<<path<<std::endl;
    std::cout<<"veeksha GetStorage_Success size = "<<size<<std::endl;
    std::cout<<"veeksha GetStorage_Success used = "<<used<<std::endl;
}

TEST_F(StorageManagerTest, GetStorage_Failure){
    // Delete a storage object
    std::cout<<"veeksha GetStorage_Failure"<<std::endl;
    std::string appId = "";
    uint32_t userId = 100;
    uint32_t groupId = 101;
    std::string path = "";
    uint32_t size = 0;
    uint32_t used = 0;
    
    EXPECT_EQ(Core::ERROR_GENERAL, interface->GetStorage(appId, userId, groupId, path, size, used));
}

// Test for DeleteStorage
TEST_F(StorageManagerTest, DeleteStorage_Success){
    // Delete a storage object
    std::cout<<"veeksha DeleteStorage_Success"<<std::endl;
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            std::cout << "Mock mkdir called for path: " << path << " with mode: " << mode << std::endl;

            return 0;
        });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            std::cout << "Mock access called for path: " << path << std::endl;
            // Simulate file exists
            return 0;
        });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
            // Simulate success
            return 0;
        });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            std::cout << "Mock statvfs called for path: " << path << std::endl;
                  // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
            });
    
    EXPECT_CALL(*p_wrapsImplMock, rmdir(::testing::_))
        .WillRepeatedly([](const char* pathname) {
            std::cout << "Mock rmdir called for path: " << pathname << std::endl;
            // Simulate success
            return 0;
        });
    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string errorReason = "";
    std::string path = "";
    
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, interface->DeleteStorage(appId, errorReason));
    std::cout<<"veeksha DeleteStorage_Success errorReason = "<<errorReason<<std::endl;
    std::cout<<"veeksha DeleteStorage_Success appId = "<<appId<<std::endl;
}

TEST_F(StorageManagerTest, DeleteStorage_Failure){
    // Delete a storage object
    std::cout<<"veeksha DeleteStorage_Failure"<<std::endl;
    std::string appId = "";
    std::string errorReason = "";
    EXPECT_EQ(Core::ERROR_GENERAL, interface->DeleteStorage(appId, errorReason));
}

// TEST_F(StorageManagerTest, RegisteredMethods){
//     std::cout<<"veeksha RegisteredMethods"<<std::endl;
//     // Check if the methods are registered
//     EXPECT_TRUE(handler.IsMethodRegistered(_T("clear")));
//     EXPECT_TRUE(handler.IsMethodRegistered(_T("clearAll")));
// }

//JSON RPC
TEST_F(StorageManagerTest, test_clear_failure_json){
    std::cout<<"veeksha Clear_Failure"<<std::endl;
    std::string appId = "";
    std::string errorReason = "";

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("clear"), _T("{\"appId\":\"\"}"), response));

    std::cout<<"veeksha Clear_Failure errorReason = "<<errorReason<<std::endl;
    std::cout<<"veeksha Clear_Failure appId = "<<appId<<std::endl;
}

TEST_F(StorageManagerTest, test_clear_success_json){
    std::cout<<"veeksha Clear_Success"<<std::endl;
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            std::cout << "Mock mkdir called for path: " << path << " with mode: " << mode << std::endl;

            return 0;
        });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            std::cout << "Mock access called for path: " << path << std::endl;
            // Simulate file exists
            return 0;
        });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
            // Simulate success
            return 0;
        });
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            std::cout << "Mock statvfs called for path: " << path << std::endl;
                  // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
            });
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        std::cout << "Mock SetValue called: scope=" << static_cast<int>(scope)
                  << ", appId=" << appId
                  << ", key=" << key
                  << ", value=" << value
                  << ", ttl=" << ttl << std::endl;
        // Simulate success
        return Core::ERROR_NONE;
        }));
    std::string path = "";
    std::string errorReason = "";   
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testApp", 1024, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clear"), _T("{\"appId\":\"testApp\"}"), response));
    std::cout<<"veeksha Clear_Success errorReason = "<<errorReason<<std::endl;
}

TEST_F(StorageManagerTest, test_clearall_without_exemption_json){
    std::cout<<"veeksha test_clearall_without_exemption_json"<<std::endl;
    std::string exemptionAppIds = "";
    std::string errorReason = "";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clearAll"), _T("{}"), response));
    std::cout<<"veeksha test_clearall_without_exemption_json errorReason = "<<errorReason<<std::endl;
}

TEST_F(StorageManagerTest, test_clearall_success_json){
    std::cout<<"veeksha ClearAll_Success"<<std::endl;
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            std::cout << "Mock mkdir called for path: " << path << " with mode: " << mode << std::endl;

            return 0;
        });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            std::cout << "Mock access called for path: " << path << std::endl;
            // Simulate file exists
            return 0;
        });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
            // Simulate success
            return 0;
        });
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            std::cout << "Mock statvfs called for path: " << path << std::endl;
                  // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
            });
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        std::cout << "Mock SetValue called: scope=" << static_cast<int>(scope)
                  << ", appId=" << appId
                  << ", key=" << key
                  << ", value=" << value
                  << ", ttl=" << ttl << std::endl;
        // Simulate success
        return Core::ERROR_NONE;
        }));
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
                .WillByDefault(::testing::Invoke([](const char* pathname) {
                    std::cout << "Mock system called with command: " << pathname << std::endl;
                    // Simulate success
                    return reinterpret_cast<DIR*>(0xDEADBEEF); // Simulated DIR* pointer
                }));
    static int callCount = 0;
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
    .WillByDefault([](DIR* dirp) -> struct dirent* {
        static struct dirent entry;
        switch (callCount++) {
            case 0:
                std::strcpy(entry.d_name, "testApp");
                entry.d_type = DT_DIR;
                return &entry;
            case 1:
                std::strcpy(entry.d_name, "testexempt");
                entry.d_type = DT_DIR;
                return &entry;
            default:
                return nullptr;
        }
    });
    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
    .WillByDefault([](DIR* dirp) {
        std::cout << "Mock closedir called for dirp: " << dirp << std::endl;
        // Simulate success
        return 0;
    });
    std::string path = "";
    std::string errorReason = "";   
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testApp", 1024, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testexempt", 1024, path, errorReason));
    // callCount = 0; // Reset call count for readdir mock
    // Replace the JSON construction with:
    std::string wrappedJson = "{\"exemptionAppIds\": \"{\\\"exemptionAppIds\\\": [\\\"testexempt\\\"]}\"}";
    std::cout << "Sending JSON: " << wrappedJson << std::endl;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clearAll"), wrappedJson, response));

    std::cout<<"veeksha ClearAll_Success errorReason = "<<errorReason<<std::endl;
}

TEST_F(StorageManagerTest, test_clearall_failure_json){

    std::cout<<"veeksha ClearAll_failure"<<std::endl;
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            std::cout << "Mock mkdir called for path: " << path << " with mode: " << mode << std::endl;

            return 0;
        });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            std::cout << "Mock access called for path: " << path << std::endl;
            // Simulate file exists
            return 0;
        });
    EXPECT_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
            // Simulate success
            return 0;
        })
        .WillOnce([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
            // Simulate success
            return 0;
        })
        .WillOnce([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            std::cout << "Mock nftw called for dirpath: " << dirpath << std::endl;
            // Simulate success
            return -1;
        });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            std::cout << "Mock statvfs called for path: " << path << std::endl;
                  // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
            });
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        std::cout << "Mock SetValue called: scope=" << static_cast<int>(scope)
                  << ", appId=" << appId
                  << ", key=" << key
                  << ", value=" << value
                  << ", ttl=" << ttl << std::endl;
        // Simulate success
        return Core::ERROR_NONE;
        }));
    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
                .WillByDefault(::testing::Invoke([](const char* pathname) {
                    std::cout << "Mock system called with command: " << pathname << std::endl;
                    // Simulate success
                    return reinterpret_cast<DIR*>(0xDEADBEEF); // Simulated DIR* pointer
                }));
    static int callCount = 0;
    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
    .WillByDefault([](DIR* dirp) -> struct dirent* {
        static struct dirent entry;
        switch (callCount++) {
            case 0:
                std::strcpy(entry.d_name, "testApp");
                entry.d_type = DT_DIR;
                return &entry;
            case 1:
                std::strcpy(entry.d_name, "testexempt");
                entry.d_type = DT_DIR;
                return &entry;
            default:
                return nullptr;
        }
    });
    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
    .WillByDefault([](DIR* dirp) {
        std::cout << "Mock closedir called for dirp: " << dirp << std::endl;
        // Simulate success
        return 0;
    });
    std::string path = "";
    std::string errorReason = "";
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testApp", 1024, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testexempt", 1024, path, errorReason));
    std::string wrappedJson = "{\"exemptionAppIds\": \"{\\\"exemptionAppIds\\\": [\\\"testexempt\\\"]}\"}";
    std::cout << "Sending JSON: " << wrappedJson << std::endl;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clearAll"), wrappedJson, response));
    std::cout<<"veeksha ClearAll_Success errorReason = "<<errorReason<<std::endl;
}