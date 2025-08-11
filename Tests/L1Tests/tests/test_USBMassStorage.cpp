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

// First, we need to add a mock for the USBMassStorage notification
class USBStorageNotificationMock : public Exchange::IUSBMassStorage::INotification {
public:
    USBStorageNotificationMock() = default;
    virtual ~USBStorageNotificationMock() = default;

    MOCK_METHOD(void, OnDeviceMounted, (const Exchange::IUSBMassStorage::USBStorageDeviceInfo&, Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator* const), (override));
    MOCK_METHOD(void, OnDeviceUnmounted, (const Exchange::IUSBMassStorage::USBStorageDeviceInfo&, Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator* const), (override));
};

TEST_F(USBMassStorageTest, VerifyExistingMethodsExist)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDeviceList")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getMountPoints")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPartitionInfo")));
}

TEST_F(USBMassStorageTest, GetDeviceList_Success)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice); 

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    
    EXPECT_TRUE(response.find("devicePath") != std::string::npos);
    EXPECT_TRUE(response.find("/dev/sda") != std::string::npos);
    EXPECT_TRUE(response.find("deviceName") != std::string::npos);
    EXPECT_TRUE(response.find("001/001") != std::string::npos);
}

TEST_F(USBMassStorageTest, GetDeviceList_Empty)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    
    EXPECT_TRUE(response.find("[]") != std::string::npos);
}

TEST_F(USBMassStorageTest, GetDeviceList_Error)
{
    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        return Core::ERROR_GENERAL;
    });

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
}

TEST_F(USBMassStorageTest, GetMountPoints_Success)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice); 

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    // Set up filesystem mocks
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct stat* info) {
        if (strstr(path, "/tmp/media")) {
            info->st_mode = S_IFDIR;
            return 0;
        }
        return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Setup mock for partitions file reading - using a different approach
    std::ifstream partitionsFile;
    
    // Create a test vector for partition lines
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    // Mock getline to read from our vector
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"001/001\"}"), response));
    
    EXPECT_TRUE(response.find("partitionName") != std::string::npos);
    EXPECT_TRUE(response.find("mountFlags") != std::string::npos);
    EXPECT_TRUE(response.find("mountPath") != std::string::npos);
    EXPECT_TRUE(response.find("fileSystem") != std::string::npos);
}

TEST_F(USBMassStorageTest, GetMountPoints_EmptyDeviceName)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"\"}"), response));
}

TEST_F(USBMassStorageTest, GetMountPoints_InvalidDeviceName)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice);

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    EXPECT_EQ(Core::ERROR_INVALID_DEVICENAME, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"invalid_device\"}"), response));
}

TEST_F(USBMassStorageTest, GetMountPoints_MountFailure)
{
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice);

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    // Set up filesystem mocks
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct stat* info) {
        if (strstr(path, "/tmp/media")) {
            info->st_mode = S_IFDIR;
            return 0;
        }
        return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(-1));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"001/001\"}"), response));
}

TEST_F(USBMassStorageTest, GetPartitionInfo_Success)
{
    // First add a mounted device to the system
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice);

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    // Set up filesystem mocks for mounting
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct stat* info) {
        if (strstr(path, "/tmp/media")) {
            info->st_mode = S_IFDIR;
            return 0;
        }
        return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"001/001\"}"), response);

    // Now test getPartitionInfo
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct statfs* buf) {
        buf->f_blocks = 1000000;
        buf->f_bsize = 4096;
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct statvfs* buf) {
        buf->f_blocks = 1000000;
        buf->f_frsize = 4096;
        buf->f_bfree = 500000;
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::Return(3)); // Return valid fd

    EXPECT_CALL(*p_wrapsImplMock, ioctl(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](int fd, unsigned long request, void* arg) {
        if (request == BLKGETSIZE64) {
            *((uint64_t*)arg) = 4294967296; // 4GB in bytes
            return 0;
        } else if (request == BLKGETSIZE) {
            *((long*)arg) = 8388608; // Sectors
            return 0;
        } else if (request == BLKSSZGET) {
            *((int*)arg) = 512; // Sector size
            return 0;
        }
        return -1;
    });

    // Use pclose instead of close since that's what's in the mock
    EXPECT_CALL(*p_wrapsImplMock, pclose(::testing::_))
    .WillOnce(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\": \"/tmp/media/usb1\"}"), response));

    EXPECT_TRUE(response.find("fileSystem") != std::string::npos);
    EXPECT_TRUE(response.find("size") != std::string::npos);
    EXPECT_TRUE(response.find("startSector") != std::string::npos);
    EXPECT_TRUE(response.find("numSectors") != std::string::npos);
    EXPECT_TRUE(response.find("sectorSize") != std::string::npos);
    EXPECT_TRUE(response.find("totalSpace") != std::string::npos);
    EXPECT_TRUE(response.find("usedSpace") != std::string::npos);
    EXPECT_TRUE(response.find("availableSpace") != std::string::npos);
}

TEST_F(USBMassStorageTest, GetPartitionInfo_EmptyMountPath)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\": \"\"}"), response));
}

TEST_F(USBMassStorageTest, GetPartitionInfo_InvalidMountPath)
{
    EXPECT_EQ(Core::ERROR_INVALID_MOUNTPOINT, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\": \"/invalid/path\"}"), response));
}

TEST_F(USBMassStorageTest, GetPartitionInfo_StatFsFailure)
{
    // First add a mounted device to the system
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice);

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    // Set up filesystem mocks for mounting
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct stat* info) {
        if (strstr(path, "/tmp/media")) {
            info->st_mode = S_IFDIR;
            return 0;
        }
        return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"001/001\"}"), response);

    // Now test getPartitionInfo with statfs failure
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce(::testing::Return(-1));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\": \"/tmp/media/usb1\"}"), response));
}

TEST_F(USBMassStorageTest, GetPartitionInfo_StatVfsFailure)
{
    // First add a mounted device to the system
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice);

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    // Set up filesystem mocks for mounting
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct stat* info) {
        if (strstr(path, "/tmp/media")) {
            info->st_mode = S_IFDIR;
            return 0;
        }
        return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"001/001\"}"), response);

    // Now test getPartitionInfo with statvfs failure
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillOnce(::testing::Return(-1));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\": \"/tmp/media/usb1\"}"), response));
}

TEST_F(USBMassStorageTest, GetPartitionInfo_DeviceOpenFailure)
{
    // First add a mounted device to the system
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice);

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    // Set up filesystem mocks for mounting
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct stat* info) {
        if (strstr(path, "/tmp/media")) {
            info->st_mode = S_IFDIR;
            return 0;
        }
        return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"001/001\"}"), response);

    // Now test getPartitionInfo with device open failure
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillOnce(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::Return(-1));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\": \"/tmp/media/usb1\"}"), response));
}

TEST_F(USBMassStorageTest, GetPartitionInfo_IoctlFailure)
{
    // First add a mounted device to the system
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice);

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    // Set up filesystem mocks for mounting
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct stat* info) {
        if (strstr(path, "/tmp/media")) {
            info->st_mode = S_IFDIR;
            return 0;
        }
        return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"001/001\"}"), response);

    // Now test getPartitionInfo with ioctl failure
    EXPECT_CALL(*p_wrapsImplMock, statfs(::testing::_, ::testing::_))
    .WillOnce(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillOnce(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, open(::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::Return(3)); // Return valid fd

    EXPECT_CALL(*p_wrapsImplMock, ioctl(::testing::_, ::testing::_, ::testing::_))
    .WillOnce(::testing::Return(-1)); // First ioctl fails

    EXPECT_CALL(*p_wrapsImplMock, pclose(::testing::_))
    .WillOnce(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPartitionInfo"), _T("{\"mountPath\": \"/tmp/media/usb1\"}"), response));
}

// Test device plug events with our callback mechanism
TEST_F(USBMassStorageTest, OnDevicePluggedIn_MassStorage)
{
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";

    // Set up filesystem mocks
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct stat* info) {
        if (strstr(path, "/tmp/media")) {
            info->st_mode = S_IFDIR;
            return 0;
        }
        return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Trigger the device plugged in event
    ASSERT_NE(USBDeviceNotification_cb, nullptr);
    USBDeviceNotification_cb->OnDevicePluggedIn(usbDevice);
    
    // Allow worker thread to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(USBMassStorageTest, OnDevicePluggedIn_NonMassStorage)
{
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_HID; // Not a mass storage device
    usbDevice.deviceSubclass = 0x01;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/input0";

    // Trigger the device plugged in event
    ASSERT_NE(USBDeviceNotification_cb, nullptr);
    USBDeviceNotification_cb->OnDevicePluggedIn(usbDevice);
    
    // Allow worker thread to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(USBMassStorageTest, OnDevicePluggedOut_MassStorage)
{
    // First we need to have a device plugged in
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";

    // Set up filesystem mocks for mounting
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct stat* info) {
        if (strstr(path, "/tmp/media")) {
            info->st_mode = S_IFDIR;
            return 0;
        }
        return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Trigger the device plugged in event
    ASSERT_NE(USBDeviceNotification_cb, nullptr);
    USBDeviceNotification_cb->OnDevicePluggedIn(usbDevice);
    
    // Allow worker thread to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Now test the unplug event
    EXPECT_CALL(*p_wrapsImplMock, umount(::testing::_))
    .WillRepeatedly(::testing::Return(0));
    
    EXPECT_CALL(*p_wrapsImplMock, rmdir(::testing::_))
    .WillRepeatedly(::testing::Return(0));

    // Trigger the device plugged out event
    USBDeviceNotification_cb->OnDevicePluggedOut(usbDevice);
    
    // Allow worker thread to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(USBMassStorageTest, OnDevicePluggedOut_NonMassStorage)
{
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_HID; // Not a mass storage device
    usbDevice.deviceSubclass = 0x01;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/input0";

    // Trigger the device plugged out event
    ASSERT_NE(USBDeviceNotification_cb, nullptr);
    USBDeviceNotification_cb->OnDevicePluggedOut(usbDevice);
    
    // Allow worker thread to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// For the JSON RPC register/unregister methods
TEST_F(USBMassStorageTest, Register_And_Unregister)
{
    // Test register
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("register"), _T("{}"), response));
    
    // Test unregister
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("unregister"), _T("{}"), response));
}

// Test for directory existence check
TEST_F(USBMassStorageTest, DirectoryExists_True)
{
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct stat* info) {
        info->st_mode = S_IFDIR;
        return 0;
    });

    // We can't directly test the private static method, but we can indirectly test it
    // by calling a public method that uses it, such as the mount functionality
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice);

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    // Since directory exists, mkdir should not be called
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .Times(0);

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"001/001\"}"), response));
}

TEST_F(USBMassStorageTest, DirectoryExists_False)
{
    EXPECT_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
    .WillOnce([](const char* path, struct stat* info) {
        return -1; // Directory doesn't exist
    });

    // We can't directly test the private static method, but we can indirectly test it
    // by calling a public method that uses it, such as the mount functionality
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    Exchange::IUSBDevice::USBDevice usbDevice;
    usbDevice.deviceClass = LIBUSB_CLASS_MASS_STORAGE;
    usbDevice.deviceSubclass = 0x06;
    usbDevice.deviceName = "001/001";
    usbDevice.devicePath = "/dev/sda";
    usbDeviceList.emplace_back(usbDevice);

    auto mockIterator = Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList);

    EXPECT_CALL(*p_usbDeviceMock, GetDeviceList(testing::_))
    .WillOnce([&](Exchange::IUSBDevice::IUSBDeviceIterator*& devices) {
        devices = mockIterator;
        return Core::ERROR_NONE;
    });

    // Setup mock for partitions file reading
    std::vector<std::string> partitionLines = {
        "major minor  #blocks  name",
        "   8        0  125034840 sda",
        "   8        1  125034809 sda1"
    };
    
    EXPECT_CALL(*p_wrapsImplMock, getline(::testing::_, ::testing::_))
    .WillRepeatedly([partitionLines](std::istream& is, std::string& line) -> std::istream& {
        static size_t lineIndex = 0;
        if (lineIndex < partitionLines.size()) {
            line = partitionLines[lineIndex++];
        } else {
            is.setstate(std::ios::eofbit);
        }
        return is;
    });

    // Since directory doesn't exist, mkdir should be called
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_wrapsImplMock, mount(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMountPoints"), _T("{\"deviceName\": \"001/001\"}"), response));
}
