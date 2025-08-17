#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mntent.h>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
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

TEST_F(USBMassStorageTest, getDeviceList_Failed)
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
    usbDevice1.deviceName = "001/002";
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

TEST_F(USBMassStorageTest, getDeviceList_NonMassStorageDevice)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice1;
    usbDevice1.deviceClass = LIBUSB_CLASS_HID;
    usbDevice1.deviceSubclass = 0x12;
    usbDevice1.deviceName = "001/002";
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
    // First setup a device in the internal list by getting device list
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

    // Populate internal device list first
    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    
    // Simulate device being plugged in and mounted
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);
    
    // Mock mounting operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const std::string& path, struct stat* info) {
        if (path.find("/tmp/media/usb") != std::string::npos) {
            return -1; // Directory doesn't exist, needs creation
        }
        if (info) info->st_mode = S_IFDIR;
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, umount(::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));
    
    // Then test getMountPoints
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"001/002\"}"), response));
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
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, handler.Invoke(connection, _T("getMountPoints"), _T("{invalid json}"), response));
}

TEST_F(USBMassStorageTest, getMountPoints_InvalidDeviceName)
{
    EXPECT_EQ(Core::ERROR_INVALID_DEVICENAME, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"invalidDeviceName\"}"), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_Success)
{
    // First setup a device with mount points
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

    // Populate internal device list first
    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    
    // Simulate device being plugged in and mounted
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);
    
    // Mock mounting operations to create mount point
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const std::string& path, struct stat* info) {
        if (path.find("/tmp/media/usb") != std::string::npos) {
            return -1; // Directory doesn't exist, needs creation
        }
        if (info) info->st_mode = S_IFDIR;
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, umount(::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Create mount points first
    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"001/002\"}"), response);

    // Now setup filesystem operation mocks for getPartitionInfo
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct statfs* buf) -> int {
        if (buf) buf->f_type = 0x565a; // NTFS
        return 0;
    });
    
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct statvfs* stat) -> int {
        if (stat) {
            stat->f_blocks = 1024000;
            stat->f_frsize = 4096;
            stat->f_bfree = 256000;
        }
        return 0;
    });
    
    EXPECT_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::Return(3));

    EXPECT_CALL(*p_wrapsImplMock, ioctl(::testing::_, ::testing::_, ::testing::_))
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

    // Remove the close() call since it's not available in WrapsImplMock
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\":\"/tmp/media/usb1\"}"), response));
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
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, handler.Invoke(connection, _T("getPartitionInfo"), _T("{invalid json}"), response));
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
    usbDevice1.deviceName = "001/002";
    usbDevice1.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice1);
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(::testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);

    // Mock mounting operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const std::string& path, struct stat* info) {
        if (path.find("/tmp/media/usb") != std::string::npos) {
            return -1;
        }
        if (info) info->st_mode = S_IFDIR;
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, umount(::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"001/002\"}"), response);

    // Now test statfs failure
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce(::testing::Return(-1));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\":\"/tmp/media/usb1\"}"), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_StatvfsFailed)
{
    // Setup device and mount point first
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

    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);

    // Mock mounting operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const std::string& path, struct stat* info) {
        if (path.find("/tmp/media/usb") != std::string::npos) {
            return -1;
        }
        if (info) info->st_mode = S_IFDIR;
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, umount(::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"001/002\"}"), response);

    // Now test statfs success but statvfs failure
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct statfs* buf) -> int {
        if (buf) buf->f_type = 0x565a;
        return 0;
    });
    
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillOnce(::testing::Return(-1));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\":\"/tmp/media/usb1\"}"), response));
}

TEST_F(USBMassStorageTest, getPartitionInfo_OpenFailed)
{
    // Setup device and mount point first
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

    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);

    // Mock mounting operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const std::string& path, struct stat* info) {
        if (path.find("/tmp/media/usb") != std::string::npos) {
            return -1;
        }
        if (info) info->st_mode = S_IFDIR;
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, umount(::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"001/002\"}"), response);

    // Now test filesystem operations with open failure
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct statfs* buf) -> int {
        if (buf) buf->f_type = 0x565a;
        return 0;
    });
    
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct statvfs* stat) -> int {
        if (stat) {
            stat->f_blocks = 1024000;
            stat->f_frsize = 4096;
            stat->f_bfree = 256000;
        }
        return 0;
    });
    
    EXPECT_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::Return(-1));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\":\"/tmp/media/usb1\"}"), response));
}

// Fixed: Removed duplicate test definition and removed close() call
TEST_F(USBMassStorageTest, getPartitionInfo_IoctlFailed)
{
    // Setup device and mount point first
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

    handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response);
    USBMassStorageImpl->OnDevicePluggedIn(usbDevice1);

    // Mock mounting operations
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const std::string& path, struct stat* info) {
        if (path.find("/tmp/media/usb") != std::string::npos) {
            return -1;
        }
        if (info) info->st_mode = S_IFDIR;
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, umount(::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\":\"001/002\"}"), response);

    // Now test filesystem operations with ioctl failure
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct statfs* buf) -> int {
        if (buf) buf->f_type = 0x565a;
        return 0;
    });
    
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct statvfs* stat) -> int {
        if (stat) {
            stat->f_blocks = 1024000;
            stat->f_frsize = 4096;
            stat->f_bfree = 256000;
        }
        return 0;
    });
    
    EXPECT_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::Return(3));

    EXPECT_CALL(*p_wrapsImplMock, ioctl(::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::Return(-1));

    // Removed close() call as it's not available in WrapsImplMock
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\":\"/tmp/media/usb1\"}"), response));
}

TEST_F(USBMassStorageTest, Register_Success)
{
    Exchange::IUSBMassStorage::INotification* testNotification = reinterpret_cast<Exchange::IUSBMassStorage::INotification*>(0x1234);
    EXPECT_EQ(Core::ERROR_NONE, USBMassStorageImpl->Register(testNotification));
}

TEST_F(USBMassStorageTest, Register_NullNotification)
{
    EXPECT_EQ(Core::ERROR_NONE, USBMassStorageImpl->Register(nullptr));
}

TEST_F(USBMassStorageTest, Register_SameNotificationTwice)
{
    Exchange::IUSBMassStorage::INotification* testNotification = reinterpret_cast<Exchange::IUSBMassStorage::INotification*>(0x1234);
    USBMassStorageImpl->Register(testNotification);
    EXPECT_EQ(Core::ERROR_NONE, USBMassStorageImpl->Register(testNotification));
}

TEST_F(USBMassStorageTest, Unregister_Success)
{
    Exchange::IUSBMassStorage::INotification* testNotification = reinterpret_cast<Exchange::IUSBMassStorage::INotification*>(0x1234);
    USBMassStorageImpl->Register(testNotification);
    EXPECT_EQ(Core::ERROR_NONE, USBMassStorageImpl->Unregister(testNotification));
}

TEST_F(USBMassStorageTest, Unregister_NullNotification)
{
    EXPECT_EQ(Core::ERROR_GENERAL, USBMassStorageImpl->Unregister(nullptr));
}

TEST_F(USBMassStorageTest, Unregister_NotRegistered)
{
    Exchange::IUSBMassStorage::INotification* testNotification = reinterpret_cast<Exchange::IUSBMassStorage::INotification*>(0x1234);
    EXPECT_EQ(Core::ERROR_GENERAL, USBMassStorageImpl->Unregister(testNotification));
}
