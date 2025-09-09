#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mntent.h>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <chrono>
#include "USBMassStorage.h"
#include "USBMassStorageImplementation.h"
#include "WorkerPoolImplementation.h"
#include "ServiceMock.h"
#include "USBDeviceMock.h"
#include "COMLinkMock.h"
#include "WrapsMock.h"
#include "ThunderPortability.h"
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

class USBMassStorageTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::USBMassStorage> plugin;
    Core::JSONRPC::Handler& handler;
    Core::JSONRPC::Context connection;
    NiceMock<ServiceMock> service;
    Core::JSONRPC::Message message;
    NiceMock<COMLinkMock> comLinkMock;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    Core::ProxyType<Plugin::USBMassStorageImplementation> USBMassStorageImpl;
    Exchange::IUSBDevice::INotification *USBDeviceNotification_cb = nullptr;
    Exchange::IUSBMassStorage::INotification *notification = nullptr;
    string response;
    USBDeviceMock *p_usbDeviceMock = nullptr;
    WrapsImplMock *p_wrapsImplMock   = nullptr;
    ServiceMock  *p_serviceMock  = nullptr;
    USBMassStorageTest()
        : plugin(Core::ProxyType<Plugin::USBMassStorage>::Create())
        , handler(*plugin)
        , connection(1,0,"")
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        p_serviceMock = new NiceMock <ServiceMock>;

        p_usbDeviceMock = new NiceMock <USBDeviceMock>;

        p_wrapsImplMock  = new NiceMock <WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);

        EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .WillOnce(testing::Return(p_usbDeviceMock));

        ON_CALL(*p_usbDeviceMock, Register(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](Exchange::IUSBDevice::INotification *notification){
                USBDeviceNotification_cb = notification;
                return Core::ERROR_NONE;;
            }));

        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                USBMassStorageImpl = Core::ProxyType<Plugin::USBMassStorageImplementation>::Create();
                return &USBMassStorageImpl;
                }));

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        plugin->Initialize(&service);
    }

    virtual ~USBMassStorageTest() override
    {
        plugin->Deinitialize(&service);

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();


        if (p_serviceMock != nullptr)
        {
            delete p_serviceMock;
            p_serviceMock = nullptr;
        }

        if (p_usbDeviceMock != nullptr)
        {
            delete p_usbDeviceMock;
            p_usbDeviceMock = nullptr;
        }

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }
    }
};

TEST_F(USBMassStorageTest, getDeviceList_Exists)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDeviceList")));
}

TEST_F(USBMassStorageTest, getMountPoints_Exists)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getMountPoints")));
}

TEST_F(USBMassStorageTest, getPartitionInfo_Exists)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPartitionInfo")));
}

TEST_F(USBMassStorageTest, getDeviceList_Success)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "001/002";
    usbDevice1.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
}

TEST_F(USBMassStorageTest, getDeviceList_EmptyList)
{
    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = nullptr;
        return Core::ERROR_GENERAL;
    });

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
}

TEST_F(USBMassStorageTest, getDeviceList_EmptyDevicePath)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "002/003";
    usbDevice1.devicePath = "";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
}

TEST_F(USBMassStorageTest, getDeviceList_MissingDevicePath)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "004/005";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_GENERAL;
    });

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
}

// Does this identify an error in the plugin?
TEST_F(USBMassStorageTest, getDeviceList_NonMassStorageDevice)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_HID;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "006/007";
    usbDevice1.devicePath = "/dev/hidraw0";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
}

TEST_F(USBMassStorageTest, getMountPoints_Success)
{
    // Setup: Create a device and ensure it gets mounted first
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "008/009";
    usbDevice1.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    // Mock device list retrieval (this we care about)
    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
        .Times(1)
        .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
            devices = mockIterator;
            return Core::ERROR_NONE;
        });

    // Infrastructure setup - use ON_CALL since we don't care about exact behavior
    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const std::string& path, struct stat* info) {
            if (path.find("/tmp/media/usb") != std::string::npos) {
                return -1; // Directory doesn't exist, needs creation
            }
            if (info) info->st_mode = S_IFDIR;
            return 0;
        });

    ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(3)); // Valid file descriptor

    // Execute: Setup the device and mount it
    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);

    // ADD: Wait for asynchronous mounting to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Test: Now getMountPoints should return the mount info (no additional mocking needed)
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"008/009\"}"), response));
}

TEST_F(USBMassStorageTest, getMountPoints_EmptyDeviceName)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"\"}"), response));
}

TEST_F(USBMassStorageTest, getMountPoints_MissingDeviceName)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("getMountPoints"), _T("{}"), response));
}

TEST_F(USBMassStorageTest, getMountPoints_InvalidJson)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("getMountPoints"), _T("{invalidjson}"), response));
}

TEST_F(USBMassStorageTest, getMountPoints_InvalidDeviceName)
{
    EXPECT_EQ(Core::ERROR_INVALID_DEVICENAME, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"invalidDeviceName\"}"), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_Success)
{
    // Setup: Create device and ensure it gets mounted
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "010/011";
    usbDevice1.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
        .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
            devices = mockIterator;
            return Core::ERROR_NONE;
        });

    // Infrastructure setup - only what's needed for mounting
    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const std::string& path, struct stat* info) {
            if (path.find("/tmp/media/usb") != std::string::npos) {
                return -1; // Directory doesn't exist, needs creation
            }
            if (info) info->st_mode = S_IFDIR;
            return 0;
        });

    ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));
    
    ON_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(3)); // Valid file descriptor

    // Setup the device and create mount points
    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);
    
    // GET THE ACTUAL MOUNT PATH instead of assuming
    string mountResponse;
    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"010/011\"}"), mountResponse);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Parse the mount path from the response
    string actualMountPath;
    size_t mountPathPos = mountResponse.find("\"mountPath\":\"");
    if (mountPathPos != string::npos) {
        size_t start = mountPathPos + 13; // Length of "\"mountPath\":\""
        size_t end = mountResponse.find("\"", start);
        if (end != string::npos) {
            actualMountPath = mountResponse.substr(start, end - start);
        }
    }
    
    EXPECT_FALSE(actualMountPath.empty()) << "Failed to extract mount path from response: " << mountResponse;

    // Now test getPartitionInfo with the ACTUAL mount path
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([](const char* path, struct statfs* buf) -> int {
            if (buf) buf->f_type = 0x565a; // NTFS
            return 0;
        });
    
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([](const char* path, struct statvfs* stat) -> int {
            if (stat) {
                stat->f_blocks = 1024000;
                stat->f_frsize = 4096;
                stat->f_bfree = 256000;
            }
            return 0;
        });

    EXPECT_CALL(*p_wrapsImplMock, ioctl(::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly([](int fd, unsigned long request, void* argp) -> int {
            if (request == BLKGETSIZE64) {
                *(reinterpret_cast<uint64_t*>(argp)) = 50ULL * 1024 * 1024 * 1024;
            } else if (request == BLKGETSIZE) {
                *(reinterpret_cast<unsigned long*>(argp)) = 1024;
            } else if (request == BLKSSZGET) {
                *(reinterpret_cast<uint32_t*>(argp)) = 512;
            }
            return 0;
        });

    // Test with the actual mount path
    string testJson = "{\"mountPath\":\"" + actualMountPath + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPartitionInfo"), testJson.c_str(), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_EmptyMountPath)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\":\"\"}"), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_MissingMountPath)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("getPartitionInfo"), _T("{}"), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_InvalidJson)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("getPartitionInfo"), _T("{invalidjson}"), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_InvalidMountPoint)
{
    EXPECT_EQ(Core::ERROR_INVALID_MOUNTPOINT, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\":\"/invalid/mount/path\"}"), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_StatfsFailed)
{
    // Setup device and mount point first
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "012/013";
    usbDevice1.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
        .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
            devices = mockIterator;
            return Core::ERROR_NONE;
        });

    // Infrastructure setup for mounting (use ON_CALL)
    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const std::string& path, struct stat* info) {
            if (path.find("/tmp/media/usb") != std::string::npos) {
                return -1;
            }
            if (info) info->st_mode = S_IFDIR;
            return 0;
        });

    ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(3)); // Valid file descriptor

    // Setup the device and create mount points
    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);
    
    // Get the actual mount path
    string mountResponse;
    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"012/013\"}"), mountResponse);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    string actualMountPath;
    size_t mountPathPos = mountResponse.find("\"mountPath\":\"");
    if (mountPathPos != string::npos) {
        size_t start = mountPathPos + 13;
        size_t end = mountResponse.find("\"", start);
        if (end != string::npos) {
            actualMountPath = mountResponse.substr(start, end - start);
        }
    }
    
    EXPECT_FALSE(actualMountPath.empty()) << "Failed to get mount path";

    // NOW test the actual failure - this is what we care about
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(-1));

    string testJson = "{\"mountPath\":\"" + actualMountPath + "\"}";
    std::cout << actualMountPath << std::endl;
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), testJson.c_str(), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_StatvfsFailed)
{
    // Setup device and mount point first
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "014/015";
    usbDevice1.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
        .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
            devices = mockIterator;
            return Core::ERROR_NONE;
        });

    // Infrastructure setup for mounting
    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const std::string& path, struct stat* info) {
            if (path.find("/tmp/media/usb") != std::string::npos) {
                return -1;
            }
            if (info) info->st_mode = S_IFDIR;
            return 0;
        });

    ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(3)); // Valid file descriptor

    // Setup the device and create mount points
    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Get the actual mount path
    string mountResponse;
    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"014/015\"}"), mountResponse);
    
    string actualMountPath;
    size_t mountPathPos = mountResponse.find("\"mountPath\":\"");
    if (mountPathPos != string::npos) {
        size_t start = mountPathPos + 13;
        size_t end = mountResponse.find("\"", start);
        if (end != string::npos) {
            actualMountPath = mountResponse.substr(start, end - start);
        }
    }

    // NOW test the actual failure sequence
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([](const char* path, struct statfs* buf) -> int {
            if (buf) buf->f_type = 0x565a;
            return 0;
        });
    
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(-1));

    string testJson = "{\"mountPath\":\"" + actualMountPath + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), testJson.c_str(), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_OpenFailed)
{
    // Setup device and mount point first
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "016/017";
    usbDevice1.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
        .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
            devices = mockIterator;
            return Core::ERROR_NONE;
        });

    // Infrastructure setup for mounting
    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const std::string& path, struct stat* info) {
            if (path.find("/tmp/media/usb") != std::string::npos) {
                return -1; // Directory doesn't exist, needs creation
            }
            if (info) info->st_mode = S_IFDIR;
            return 0;
        });

    ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(3)); // Valid file descriptor for mounting

    // Setup the device and create mount points
    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);
    
    // Get the actual mount path
    string mountResponse;
    uint32_t mountResult = handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"016/017\"}"), mountResponse);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ADD: Verify mounting succeeded before proceeding
    EXPECT_EQ(Core::ERROR_NONE, mountResult) << "Mount failed, response: " << mountResponse;
    
    string actualMountPath;
    size_t mountPathPos = mountResponse.find("\"mountPath\":\"");
    if (mountPathPos != string::npos) {
        size_t start = mountPathPos + 13;
        size_t end = mountResponse.find("\"", start);
        if (end != string::npos) {
            actualMountPath = mountResponse.substr(start, end - start);
        }
    }
    
    EXPECT_FALSE(actualMountPath.empty()) << "Failed to get mount path";

    // NOW test the actual failure sequence - statfs and statvfs succeed, but open fails
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([](const char* path, struct statfs* buf) -> int {
            if (buf) buf->f_type = 0x565a; // NTFS
            return 0;
        });
    
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([](const char* path, struct statvfs* stat) -> int {
            if (stat) {
                stat->f_blocks = 1024000;
                stat->f_frsize = 4096;
                stat->f_bfree = 256000;
            }
            return 0;
        });

    // This is the failure we're testing - open fails
    EXPECT_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(-1)); // Open failure

    string testJson = "{\"mountPath\":\"" + actualMountPath + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), testJson.c_str(), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_IoctlFailed)
{
    // Setup device and mount point first
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "018/019";
    usbDevice1.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
        .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
            devices = mockIterator;
            return Core::ERROR_NONE;
        });

    // Infrastructure setup for mounting
    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const std::string& path, struct stat* info) {
            if (path.find("/tmp/media/usb") != std::string::npos) {
                return -1; // Directory doesn't exist, needs creation
            }
            if (info) info->st_mode = S_IFDIR;
            return 0;
        });

    ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    // ADD: Mock open() for partition detection during mounting
    ON_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(3)); // Valid file descriptor for mounting

    // Setup the device and create mount points
    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);
    
    // ADD: Wait for asynchronous mounting to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Get the actual mount path
    string mountResponse;
    uint32_t mountResult = handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"018/019\"}"), mountResponse);
    
    // ADD: Verify mounting succeeded before proceeding
    EXPECT_EQ(Core::ERROR_NONE, mountResult) << "Mount failed, response: " << mountResponse;
    
    string actualMountPath;
    size_t mountPathPos = mountResponse.find("\"mountPath\":\"");
    if (mountPathPos != string::npos) {
        size_t start = mountPathPos + 13;
        size_t end = mountResponse.find("\"", start);
        if (end != string::npos) {
            actualMountPath = mountResponse.substr(start, end - start);
        }
    }
    
    EXPECT_FALSE(actualMountPath.empty()) << "Failed to get mount path";

    // NOW test the actual failure sequence - statfs, statvfs, and open succeed, but ioctl fails
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([](const char* path, struct statfs* buf) -> int {
            if (buf) buf->f_type = 0x565a; // NTFS
            return 0;
        });
    
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce([](const char* path, struct statvfs* stat) -> int {
            if (stat) {
                stat->f_blocks = 1024000;
                stat->f_frsize = 4096;
                stat->f_bfree = 256000;
            }
            return 0;
        });

    // This is the failure we're testing - open succeeds but ioctl fails
    EXPECT_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(4)); // Valid FD for getPartitionInfo

    EXPECT_CALL(*p_wrapsImplMock, ioctl(::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(-1)); // ioctl failure

    string testJson = "{\"mountPath\":\"" + actualMountPath + "\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), testJson.c_str(), response));
}

TEST_F(USBMassStorageTest, Information_ReturnsCorrectString)
{
    string result = plugin->Information();
    EXPECT_EQ(result, "The USBMassStorage Plugin manages device mounting and stores mount information.");
}

// TEST_F(USBMassStorageTest, OnDeviceUnmounted_ValidMountPoints_CreatesCorrectPayloadAndNotifies)
// {
//     // First, setup and mount a real device to get actual mount paths
//     std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
//     Exchange::IUSBDevice::USBDevice usbDevice1;
//     usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
//     usbDevice1.deviceSubclass = 0x12;
//     usbDevice1.deviceName = "020/021";
//     usbDevice1.devicePath = "/dev/sda2021";
//     usbDeviceList.emplace_back(usbDevice1);
//     auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

//     EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
//         .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
//             devices = mockIterator;
//             return Core::ERROR_NONE;
//         });

//     // Setup mounting infrastructure
//     ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
//         .WillByDefault([](const std::string& path, struct stat* info) {
//             if (path.find("/tmp/media/usb") != std::string::npos) {
//                 return -1;
//             }
//             if (info) info->st_mode = S_IFDIR;
//             return 0;
//         });

//     ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
//         .WillByDefault(::testing::Return(0));

//     ON_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
//         .WillByDefault(::testing::Return(0));

//     // Mount the device and get actual mount info
//     handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
//     USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);
    
//     // Get the actual mount points that were created
//     string mountResponse;
//     EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMountPoints"), 
//               _T("{\"deviceName\":\"020/021\"}"), mountResponse));
    
//     // Parse the actual mount information from the response
//     // This ensures we test with real mount paths that the implementation created
//     EXPECT_FALSE(mountResponse.empty());
//     EXPECT_NE(mountResponse.find("mountPath"), string::npos);
    
//     // Test that the notification method executes without throwing exceptions
//     // when called with real mount data
//     EXPECT_NO_THROW({
//         if (notification != nullptr) {
//             // Create device info from our actual device
//             Exchange::IUSBMassStorage::USBStorageDeviceInfo deviceInfo;
//             deviceInfo.devicePath = usbDevice1.devicePath;
//             deviceInfo.deviceName = usbDevice1.deviceName;
            
//             // Get actual mount info iterator (would need to extract from implementation)
//             // For now, test that the method can be called without crashing
//             std::list<Exchange::IUSBMassStorage::USBStorageMountInfo> emptyList;
//             auto emptyIterator = Core::Service<RPC::IteratorType<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>>::Create<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>(emptyList);
            
//             notification->OnDeviceUnmounted(deviceInfo, emptyIterator);
//         }
//     });
// }

TEST_F(USBMassStorageTest, OnDeviceUnmounted_ThroughImplementation_ValidBehavior)
{
    // Setup device and mount points through the implementation first
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "022/023";
    usbDevice1.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
        .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
            devices = mockIterator;
            return Core::ERROR_NONE;
        });

    // Populate internal device list first
    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    
    // Mock mounting operations
    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const std::string& path, struct stat* info) {
            if (path.find("/tmp/media/usb") != std::string::npos) {
                return -1; // Directory doesn't exist, needs creation
            }
            if (info) info->st_mode = S_IFDIR;
            return 0;
        });

    ON_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));

    ON_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(0));
    
    // ADD: Mock open() for partition detection during mounting
    ON_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(3)); // Valid file descriptor for mounting

    // Trigger mount first
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);
    
    // ADD: Wait for asynchronous mounting to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify device is mounted - should be able to get mount points
    string mountResponse;
    uint32_t mountResult = handler.Invoke(connection, _T("getMountPoints"), 
              _T("{\"deviceName\":\"022/023\"}"), mountResponse);
    
    // ADD: Verify mounting succeeded before proceeding
    EXPECT_EQ(Core::ERROR_NONE, mountResult) << "Mount failed, response: " << mountResponse;
    EXPECT_FALSE(mountResponse.empty());
    EXPECT_NE(mountResponse.find("mountPath"), string::npos) << "Mount response should contain mountPath";
    
    // Set up expectations for unmount operations
    EXPECT_CALL(*p_wrapsImplMock, umount(::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(0));
    
    // ADD: Expect rmdir to be called to remove mount directories
    EXPECT_CALL(*p_wrapsImplMock, rmdir(::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(0));
    
    // Now test unmount behavior which should trigger OnDeviceUnmounted
    EXPECT_NO_THROW(USBMassStorageImpl->OnDevicePluggedOut(usbDevice1));
    
    // Wait for the asynchronous unmount operation to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Verify device is no longer mounted
    string unmountResponse;
    uint32_t result = handler.Invoke(connection, _T("getMountPoints"), 
                                   _T("{\"deviceName\":\"022/023\"}"), unmountResponse);

    bool deviceProperlyUnmounted = false;
    
    if (result == Core::ERROR_INVALID_DEVICENAME) {
        deviceProperlyUnmounted = true;
    } else if (result == Core::ERROR_NONE) {
        if (unmountResponse.find("[]") != string::npos || 
            unmountResponse.find("mountPath") == string::npos ||
            unmountResponse.empty()) {
            deviceProperlyUnmounted = true;
        }
    }
    
    EXPECT_TRUE(deviceProperlyUnmounted) 
        << "Device should no longer have valid mount points after unplug. "
        << "Result: " << result << ", Response: " << unmountResponse;
}

TEST_F(USBMassStorageTest, Initialize_CallsConfigure_AndMountDevicesOnBootUp)
{
    // Create a new instance of the plugin for this specific test
    Core::ProxyType<Plugin::USBMassStorage> testPlugin = Core::ProxyType<Plugin::USBMassStorage>::Create();
    
    // Create mock service
    NiceMock<ServiceMock> testService;
    
    // Create a mock USB device
    USBDeviceMock* mockUsbDevice = new NiceMock<USBDeviceMock>();
    
    // Create test devices with realistic device paths
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    
    // Mass storage device with valid path - use a path that won't conflict
    Exchange::IUSBDevice::USBDevice validDevice;
    validDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    validDevice.deviceSubclass = 0x06;
    validDevice.deviceName = "valid_device";
    validDevice.devicePath = "/dev/sda1"; // Use realistic path that could exist
    usbDeviceList.push_back(validDevice);
    
    // Mass storage device with empty path
    Exchange::IUSBDevice::USBDevice emptyPathDevice;
    emptyPathDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    emptyPathDevice.deviceSubclass = 0x06;
    emptyPathDevice.deviceName = "empty_path_device";
    emptyPathDevice.devicePath = "";
    usbDeviceList.push_back(emptyPathDevice);
    
    // Non-mass storage device
    Exchange::IUSBDevice::USBDevice nonMassStorageDevice;
    nonMassStorageDevice.deviceClass = LIBUSB_CLASS_HID;
    nonMassStorageDevice.deviceSubclass = 0x01;
    nonMassStorageDevice.deviceName = "hid_device";
    nonMassStorageDevice.devicePath = "/dev/hidraw0";
    usbDeviceList.push_back(nonMassStorageDevice);
    
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);
    
    // Set up service mock expectations
    EXPECT_CALL(testService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(mockUsbDevice));
    
    EXPECT_CALL(testService, AddRef()).WillRepeatedly(::testing::Return());
    EXPECT_CALL(testService, Register(::testing::_)).WillRepeatedly(::testing::Return());
    
    // Set up USB device mock expectations
    EXPECT_CALL(*mockUsbDevice, GetDeviceList(::testing::_))
        .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
            devices = mockIterator;
            return Core::ERROR_NONE;
        });
    
    EXPECT_CALL(*mockUsbDevice, Register(::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    // Mock filesystem operations for boot-up mounting
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillRepeatedly([](const std::string& path, struct stat* info) {
            if (path.find("/tmp/media") != std::string::npos) {
                return -1; // Directories don't exist yet
            }
            if (info) info->st_mode = S_IFDIR;
            return 0;
        });
    
    // Expect creation of /tmp/media and dynamic /tmp/media/usbX directories
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock partition detection (this simulates reading /proc/partitions)
    // The implementation reads partition info to determine what to mount
    EXPECT_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(3)); // Valid file descriptor

    // Mock mount operations - these will use dynamic mount paths
    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));
    
    // Mock COM link to return our implementation
    ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
        [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
            connectionId = 12345;
            return &USBMassStorageImpl;
        }));
    
    // Call Initialize - this will trigger Configure and MountDevicesOnBootUp
    string result = testPlugin->Initialize(&testService);
    
    // Verify initialization succeeded
    EXPECT_EQ(result, "");
    
    // After initialization, we can verify that devices were processed
    // by checking if we can get device list (this would only work if initialization succeeded)
    Core::JSONRPC::Handler& testHandler(*testPlugin);
    string deviceListResponse;
    uint32_t deviceListResult = testHandler.Invoke(connection, _T("getDeviceList"), _T("{}"), deviceListResponse);
    
    // Should succeed and return the valid device (not empty path or HID device)
    EXPECT_EQ(deviceListResult, Core::ERROR_NONE);
    EXPECT_NE(deviceListResponse.find("valid_device"), string::npos);
    
    // Clean up
    testPlugin->Deinitialize(&testService);
    delete mockUsbDevice;
}

// TEST_F(USBMassStorageTest, Deinitialize_ConnectionNull_SkipsConnectionTermination)
// {
//     // Create a new instance of the plugin for this specific test
//     Core::ProxyType<Plugin::USBMassStorage> testPlugin = Core::ProxyType<Plugin::USBMassStorage>::Create();
    
//     // Create mock service
//     NiceMock<ServiceMock> testService;
    
//     // Create a mock USB device
//     USBDeviceMock* mockUsbDevice = new NiceMock<USBDeviceMock>();
    
//     // Set up expectations for initialization - ensure these are called at least once
//     EXPECT_CALL(testService, AddRef())
//         .Times(::testing::AtLeast(1));
    
//     EXPECT_CALL(testService, Register(::testing::_))
//         .Times(::testing::AtLeast(1));
    
//     EXPECT_CALL(testService, QueryInterfaceByCallsign(::testing::_, ::testing::_))
//         .Times(::testing::AtLeast(1))
//         .WillRepeatedly(::testing::Return(mockUsbDevice));
    
//     // THIS IS KEY: Ensure QueryInterface is called at least once during deinitialize
//     // This will return nullptr to trigger our target branch
//     EXPECT_CALL(testService, QueryInterface(::testing::_))
//         .Times(::testing::AtLeast(1))
//         .WillRepeatedly(::testing::Return(nullptr));
    
//     // Set up expectations for Register on the USB device
//     EXPECT_CALL(*mockUsbDevice, Register(::testing::_))
//         .Times(::testing::AtLeast(1))
//         .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
//     // Mock the COM link to return our implementation (this ensures _usbMassStorage is not null)
//     ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
//         .WillByDefault(::testing::Invoke(
//         [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
//             connectionId = 12345; // Set a connection ID
//             return &USBMassStorageImpl;
//         }));
    
//     // Initialize the plugin - this should set up _usbMassStorage
//     string result = testPlugin->Initialize(&testService);
//     EXPECT_EQ(result, "");
    
//     // Now set up expectations for deinitialization
//     EXPECT_CALL(testService, Unregister(::testing::_))
//         .Times(::testing::AtLeast(1));
    
//     // Mock USB device unregister if it gets called
//     EXPECT_CALL(*mockUsbDevice, Unregister(::testing::_))
//         .Times(::testing::AnyNumber());
    
//     // Set up Release expectations - should be called at least once
//     EXPECT_CALL(testService, Release())
//         .Times(::testing::AtLeast(1));
    
//     // Call Deinitialize - The QueryInterface mock will return nullptr,
//     // which will cause RemoteConnection() to return nullptr, hitting our target branch
//     EXPECT_NO_THROW(testPlugin->Deinitialize(&testService));
    
//     // The test passes if:
//     // 1. No exception is thrown
//     // 2. QueryInterface was called at least once (verified by EXPECT_CALL)
//     // 3. The method completes successfully without trying to call connection->Terminate()
//     // 4. The connection cleanup block is skipped (since connection == nullptr)
//     // 5. All initialization calls (AddRef, Register, QueryInterfaceByCallsign) happened at least once
    
//     // Clean up allocated mock
//     delete mockUsbDevice;
// }
