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

#include <gtest/gtest.h>
#include <mntent.h>
#include <fstream>
#include "USBDevice.h"
#include "USBDeviceImplementation.h"
#include "libUSBMock.h"
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include <fstream> // Added for file creation
#include <string>
#include <vector>
#include <cstdio>
#include "COMLinkMock.h"
#include "WorkerPoolImplementation.h"
#include "WrapsMock.h"
#include "secure_wrappermock.h"
#include "ThunderPortability.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

#define MOCK_USB_DEVICE_BUS_NUMBER_1    100
#define MOCK_USB_DEVICE_ADDRESS_1       001
#define MOCK_USB_DEVICE_PORT_1          123

#define MOCK_USB_DEVICE_BUS_NUMBER_2    101
#define MOCK_USB_DEVICE_ADDRESS_2       002
#define MOCK_USB_DEVICE_PORT_2          124

#define MOCK_USB_DEVICE_SERIAL_NO "0401805e4532973503374df52a239c898397d348"
#define MOCK_USB_DEVICE_MANUFACTURER "USB"
#define MOCK_USB_DEVICE_PRODUCT "SanDisk 3.2Gen1"
#define LIBUSB_CONFIG_ATT_BUS_POWERED 0x80
namespace {
const string callSign = _T("USBDevice");
}

// Add these type definitions and class after the existing includes and before the test class definition

typedef enum : uint32_t {
    USBDevice_onDevicePluggedIn  = 0x00000001,
    USBDevice_onDevicePluggedOut = 0x00000002,
} USBDeviceEventType_t;

class NotificationHandler : public Exchange::IUSBDevice::INotification {
private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t m_event_signalled;
    
    // Event-specific flags
    bool m_onDevicePluggedInReceived;
    bool m_onDevicePluggedOutReceived;
    
    // Parameter storage for validation
    Exchange::IUSBDevice::USBDevice m_pluggedInDevice;
    Exchange::IUSBDevice::USBDevice m_pluggedOutDevice;
    
    // Reference counting
    mutable Core::CriticalSection m_refCountLock;
    mutable uint32_t m_refCount;

public:
    NotificationHandler() 
        : m_event_signalled(0)
        , m_onDevicePluggedInReceived(false)
        , m_onDevicePluggedOutReceived(false)
        , m_refCount(1)
    {
        m_pluggedInDevice.deviceClass = 0;
        m_pluggedInDevice.deviceSubclass = 0;
        m_pluggedInDevice.deviceName = "";
        m_pluggedInDevice.devicePath = "";
        
        m_pluggedOutDevice.deviceClass = 0;
        m_pluggedOutDevice.deviceSubclass = 0;
        m_pluggedOutDevice.deviceName = "";
        m_pluggedOutDevice.devicePath = "";
    }

    virtual ~NotificationHandler() {}

    BEGIN_INTERFACE_MAP(NotificationHandler)
    INTERFACE_ENTRY(Exchange::IUSBDevice::INotification)
    END_INTERFACE_MAP

    // IReferenceCounted interface implementation
    void AddRef() const override
    {
        m_refCountLock.Lock();
        ++m_refCount;
        m_refCountLock.Unlock();
    }

    uint32_t Release() const override
    {
        m_refCountLock.Lock();
        --m_refCount;
        uint32_t refCount = m_refCount;
        m_refCountLock.Unlock();
        
        if (refCount == 0) {
            delete this;
            return Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return refCount;
    }

    // Notification interface implementations
    void OnDevicePluggedIn(const Exchange::IUSBDevice::USBDevice& device) override
    {
        TEST_LOG("OnDevicePluggedIn notification received - deviceClass: %u, deviceSubclass: %u, deviceName: %s, devicePath: %s",
                 device.deviceClass, device.deviceSubclass, device.deviceName.c_str(), device.devicePath.c_str());
        
        std::unique_lock<std::mutex> lock(m_mutex);
        m_onDevicePluggedInReceived = true;
        m_event_signalled |= USBDevice_onDevicePluggedIn;
        
        // Store device information for validation
        m_pluggedInDevice.deviceClass = device.deviceClass;
        m_pluggedInDevice.deviceSubclass = device.deviceSubclass;
        m_pluggedInDevice.deviceName = device.deviceName;
        m_pluggedInDevice.devicePath = device.devicePath;
        
        m_condition_variable.notify_one();
    }

    void OnDevicePluggedOut(const Exchange::IUSBDevice::USBDevice& device) override
    {
        TEST_LOG("OnDevicePluggedOut notification received - deviceClass: %u, deviceSubclass: %u, deviceName: %s, devicePath: %s",
                 device.deviceClass, device.deviceSubclass, device.deviceName.c_str(), device.devicePath.c_str());
        
        std::unique_lock<std::mutex> lock(m_mutex);
        m_onDevicePluggedOutReceived = true;
        m_event_signalled |= USBDevice_onDevicePluggedOut;
        
        // Store device information for validation
        m_pluggedOutDevice.deviceClass = device.deviceClass;
        m_pluggedOutDevice.deviceSubclass = device.deviceSubclass;
        m_pluggedOutDevice.deviceName = device.deviceName;
        m_pluggedOutDevice.devicePath = device.devicePath;
        
        m_condition_variable.notify_one();
    }

    // Wait for specific notification with timeout
    bool WaitForRequestStatus(uint32_t timeout_ms, USBDeviceEventType_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timeout(timeout_ms);
        auto status = m_condition_variable.wait_until(lock, now + timeout, [this, expected_status]() {
            return ((m_event_signalled & expected_status) == expected_status);
        });

        return status;
    }

    // Getter methods for event flags
    bool getOnDevicePluggedInReceived() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_onDevicePluggedInReceived;
    }

    bool getOnDevicePluggedOutReceived() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_onDevicePluggedOutReceived;
    }

    // Getter methods for device parameters
    Exchange::IUSBDevice::USBDevice getPluggedInDevice() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_pluggedInDevice;
    }

    Exchange::IUSBDevice::USBDevice getPluggedOutDevice() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_pluggedOutDevice;
    }

    // Reset methods
    void resetOnDevicePluggedIn()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_onDevicePluggedInReceived = false;
        m_event_signalled &= ~USBDevice_onDevicePluggedIn;
        m_pluggedInDevice.deviceClass = 0;
        m_pluggedInDevice.deviceSubclass = 0;
        m_pluggedInDevice.deviceName = "";
        m_pluggedInDevice.devicePath = "";
    }

    void resetOnDevicePluggedOut()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_onDevicePluggedOutReceived = false;
        m_event_signalled &= ~USBDevice_onDevicePluggedOut;
        m_pluggedOutDevice.deviceClass = 0;
        m_pluggedOutDevice.deviceSubclass = 0;
        m_pluggedOutDevice.deviceName = "";
        m_pluggedOutDevice.devicePath = "";
    }

    void resetAll()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled = 0;
        m_onDevicePluggedInReceived = false;
        m_onDevicePluggedOutReceived = false;
        
        m_pluggedInDevice.deviceClass = 0;
        m_pluggedInDevice.deviceSubclass = 0;
        m_pluggedInDevice.deviceName = "";
        m_pluggedInDevice.devicePath = "";
        
        m_pluggedOutDevice.deviceClass = 0;
        m_pluggedOutDevice.deviceSubclass = 0;
        m_pluggedOutDevice.deviceName = "";
        m_pluggedOutDevice.devicePath = "";
    }
};

class USBDeviceTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::USBDevice> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    libUSBImplMock  *p_libUSBImplMock   = nullptr;
    Core::ProxyType<Plugin::USBDeviceImplementation> USBDeviceImpl;
    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    libusb_hotplug_callback_fn libUSBHotPlugCbDeviceAttached = nullptr;
    libusb_hotplug_callback_fn libUSBHotPlugCbDeviceDetached = nullptr;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;

    USBDeviceTest()
        : plugin(Core::ProxyType<Plugin::USBDevice>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        p_libUSBImplMock  = new NiceMock <libUSBImplMock>;
        libusbApi::setImpl(p_libUSBImplMock);

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                        return &comLinkMock;
                    }));

#ifdef USE_THUNDER_R4
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
			.WillByDefault(::testing::Invoke(
                  [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        USBDeviceImpl = Core::ProxyType<Plugin::USBDeviceImplementation>::Create();
                        TEST_LOG("Pass created USBDeviceImpl: %p &USBDeviceImpl: %p", USBDeviceImpl, &USBDeviceImpl);
                        return &USBDeviceImpl;
                    }));
#else
	  ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
	    .WillByDefault(::testing::Return(USBDeviceImpl));
#endif /*USE_THUNDER_R4 */

        PluginHost::IFactories::Assign(&factoriesImplementation);

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
           plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);

        EXPECT_EQ(string(""), plugin->Initialize(&service));
    }
    virtual ~USBDeviceTest() override
    {
        TEST_LOG("USBDeviceTest Destructor");

        plugin->Deinitialize(&service);

        dispatcher->Deactivate();
        dispatcher->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        PluginHost::IFactories::Assign(nullptr);

        libusbApi::setImpl(nullptr);
        if (p_libUSBImplMock != nullptr)
        {
            delete p_libUSBImplMock;
            p_libUSBImplMock = nullptr;
        }
    }

    void Mock_SetDeviceDesc(uint8_t bus_number, uint8_t device_address);
    void Mock_SetSerialNumberInUSBDevicePath();
};

void USBDeviceTest::Mock_SetDeviceDesc(uint8_t bus_number, uint8_t device_address)
{
     ON_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillByDefault(
            [bus_number, device_address](libusb_device *dev, struct libusb_device_descriptor *desc) {
                 if ((bus_number == dev->bus_number) &&
                     (device_address == dev->device_address))
                 {
                      desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                      desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                 }
                 return LIBUSB_SUCCESS;
     });

    ON_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillByDefault(::testing::Return(device_address));

    ON_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillByDefault(::testing::Return(bus_number));

    ON_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
            if((nullptr != dev) && (nullptr != port_numbers))
            {
                port_numbers[0] = dev->port_number;
                return 1;
            }
            else
            {
                return 0;
            }
        });

    if (device_address == MOCK_USB_DEVICE_ADDRESS_1)
    {
        std::string vendorFileName = "/tmp/block/sda/device/vendor";
        std::ofstream outVendorStream(vendorFileName);

        if (!outVendorStream) {
            TEST_LOG("Error opening file for writing!");
        }
        outVendorStream << "Generic" << std::endl;
        outVendorStream.close();

        std::string modelFileName = "/tmp/block/sda/device/model";
        std::ofstream outModelStream(modelFileName);

        if (!outModelStream) {
            TEST_LOG("Error opening file for writing!");
        }
        outModelStream << "Flash Disk" << std::endl;
        outModelStream.close();
    }

    if (device_address == MOCK_USB_DEVICE_ADDRESS_2)
    {
        std::string vendorFileName = "/tmp/block/sdb/device/vendor";
        std::ofstream  outVendorStream(vendorFileName);

        if (!outVendorStream) {
            TEST_LOG("Error opening file for writing!");
        }
        outVendorStream << "JetFlash" << std::endl;
        outVendorStream.close();

        std::string modelFileName = "/tmp/block/sdb/device/model";
        std::ofstream outModelStream(modelFileName);

        if (!outModelStream) {
            TEST_LOG("Error opening file for writing!");
        }
        outModelStream << "Transcend_16GB" << std::endl;
        outModelStream.close();
    }
}

void USBDeviceTest::Mock_SetSerialNumberInUSBDevicePath()
{
    std::string serialNumFileName1 = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile1(serialNumFileName1);

    if (!serialNumOutFile1) {
        TEST_LOG("Error opening file for writing!");
    }
    serialNumOutFile1 << "B32FD507" << std::endl;
    serialNumOutFile1.close();

    std::string serialNumFileName2 = "/tmp/bus/usb/devices/101-124/serial";
    std::ofstream serialNumOutFile2(serialNumFileName2);

    if (!serialNumOutFile2) {
        TEST_LOG("Error opening file for writing!");
    }
    serialNumOutFile2<< "UEUIRCXT" << std::endl;
    serialNumOutFile2.close();

    std::string serialNumFileSda = "/dev/sda";
    std::ofstream serialNumOutFileSda(serialNumFileSda);

    if (!serialNumOutFileSda) {
        TEST_LOG("Error opening file for writing!");
    }
    serialNumOutFileSda << "B32FD507 100-123" << std::endl;
    serialNumOutFileSda.close();


    std::string serialNumFileSdb = "/dev/sdb";
    std::ofstream serialNumOutFileSdb(serialNumFileSdb);

    if (!serialNumOutFileSdb) {
        TEST_LOG("Error opening file for writing!");
    }
    serialNumOutFileSdb << "UEUIRCXT 101-124" << std::endl;
    serialNumOutFileSdb.close();
}


TEST_F(USBDeviceTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDeviceList")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDeviceInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("bindDriver")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("unbindDriver")));
}

/*******************************************************************************************************************
 * Test function for :getDeviceList
 * getDeviceList :
 *                Gets a list of usb device details
 *
 *                @return Response object containing the usb device list
 * Use case coverage:
 *                @Success :2
 *                @Failure :0
 ********************************************************************************************************************/

/**
 * @brief : getDeviceList Method with single mass storage USB
 *          Check if  path parameter is missing from the parameters JSON object;
 *          then  getDeviceList shall be failed and return Erorr code: ERROR_BAD_REQUEST
 *
 * @param[out]   :  Iterator of USB Device List
 * @return      :  error code: ERROR_NONE
 */
TEST_F(USBDeviceTest, getDeviceListUsingWithSingleMassStorageUSBSuccessCase)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    /* Mock for Device List, 1 Device */
    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_context *ctx, libusb_device ***list) {
        struct libusb_device **ret = nullptr;
        ssize_t len = 1;

        ret = (struct libusb_device **)malloc(len * sizeof(struct libusb_device *));

        if (nullptr == ret)
        {
            std::cout << "malloc failed";
            len = 0;
        }
        else
        {
            uint8_t bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            uint8_t device_address = MOCK_USB_DEVICE_ADDRESS_1;
            uint8_t port_number = MOCK_USB_DEVICE_PORT_1;

            for (int index = 0; index < len; ++index)
            {
                ret[index] =  (struct libusb_device *)malloc(sizeof(struct libusb_device));

                if (nullptr == ret[index])
                {
                    std::cout << "malloc failed";
                    len = 0;
                }
                else
                {
                    ret[index]->bus_number = bus_number;
                    ret[index]->device_address = device_address;
                    ret[index]->port_number = port_number;

                    bus_number += 1;
                    device_address += 1;
                    port_number += 1;
                }
            }
            *list = ret;
        }

        return len;
    });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_device **list, int unref_devices) {
        for (int index = 0; index < 2; ++index)
        {
            free(list[index]);
        }
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev, struct libusb_device_descriptor *desc) {
        desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
        desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
        return LIBUSB_SUCCESS;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->device_address;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->bus_number;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
        if((nullptr != dev) && (nullptr != port_numbers))
        {
            port_numbers[0] = dev->port_number;
            return 1;
        }
        else
        {
            return 0;
        }
    });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_EQ(response, string("[{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"100\\/001\",\"devicePath\":\"\\/dev\\/sda\"}]"));
}

/**
 * @brief : getDeviceList Method with multiple mass storage USB
 *          Check if  path parameter is missing from the parameters JSON object;
 *          then  getDeviceList shall be failed and return Erorr code: ERROR_BAD_REQUEST
 *
 * @param[out]   :  Iterator of USB Device List
 * @return      :  error code: ERROR_NONE
 */
TEST_F(USBDeviceTest, getDeviceListUsingWithMultipleMassStorageUSBSuccessCase)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);

    /* Mock for Device List, 2 Devices */
    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_context *ctx, libusb_device ***list) {
        struct libusb_device **ret = nullptr;
        ssize_t len = 2;

        ret = (struct libusb_device **)malloc(len * sizeof(struct libusb_device *));

        if (nullptr == ret)
        {
            std::cout << "malloc failed";
            len = 0;
        }
        else
        {
            uint8_t bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            uint8_t device_address = MOCK_USB_DEVICE_ADDRESS_1;
            uint8_t port_number = MOCK_USB_DEVICE_PORT_1;

            for (int index = 0; index < len; ++index)
            {
                ret[index] =  (struct libusb_device *)malloc(sizeof(struct libusb_device));

                if (nullptr == ret[index])
                {
                    std::cout << "malloc failed";
                    len = 0;
                }
                else
                {
                    ret[index]->bus_number = bus_number;
                    ret[index]->device_address = device_address;
                    ret[index]->port_number = port_number;

                    bus_number += 1;
                    device_address += 1;
                    port_number += 1;
                }
            }
            *list = ret;
        }

        return len;
    });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_device **list, int unref_devices) {
        for (int index = 0; index < 2; ++index)
        {
        free(list[index]);
        }
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev, struct libusb_device_descriptor *desc) {
            desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
            desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
            return LIBUSB_SUCCESS;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->device_address;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->bus_number;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
    if((nullptr != dev) && (nullptr != port_numbers))
    {
        port_numbers[0] = dev->port_number;
        return 1;
    }
    else
    {
        return 0;
    }
    });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_EQ(response, string("[{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"100\\/001\",\"devicePath\":\"\\/dev\\/sda\"},{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"101\\/002\",\"devicePath\":\"\\/dev\\/sdb\"}]"));
}
/*Test cases for getDeviceList ends here*/

/*******************************************************************************************************************
 * Test function for :bindDriver
 * getDeviceList :
 *                Binds the respective driver for the device.
 *
 *                @return sucessed
 * Use case coverage:
 *                @Success :1
 *                @Failure :0
 ********************************************************************************************************************/

/**
 * @brief : bindDriver Method with given device name
 *          Check if  path parameter is missing from the parameters JSON object;
 *          then  bindDriver shall be failed and return Erorr code: ERROR_BAD_REQUEST
 *
 * @param[in]   :  deviceName
 * @return      :  error code: ERROR_NONE
 */
 TEST_F(USBDeviceTest, bindDriverSuccessCase)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);

    /* Mock for Device List, 2 Devices */
    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_context *ctx, libusb_device ***list) {
        struct libusb_device **ret = nullptr;
        ssize_t len = 2;

        ret = (struct libusb_device **)malloc(len * sizeof(struct libusb_device *));

        if (nullptr == ret)
        {
            std::cout << "malloc failed";
            len = 0;
        }
        else
        {
            uint8_t bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            uint8_t device_address = MOCK_USB_DEVICE_ADDRESS_1;
            uint8_t port_number = MOCK_USB_DEVICE_PORT_1;

            for (int index = 0; index < len; ++index)
            {
                ret[index] =  (struct libusb_device *)malloc(sizeof(struct libusb_device));

                if (nullptr == ret[index])
                {
                    std::cout << "malloc failed";
                    len = 0;
                }
                else
                {
                    ret[index]->bus_number = bus_number;
                    ret[index]->device_address = device_address;
                    ret[index]->port_number = port_number;

                    bus_number += 1;
                    device_address += 1;
                    port_number += 1;
                }
            }
            *list = ret;
        }

        return len;
    });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_device **list, int unref_devices) {
        for (int index = 0; index < 2; ++index)
        {
        free(list[index]);
        }
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev, struct libusb_device_descriptor *desc) {
            desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
            desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
            return LIBUSB_SUCCESS;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->device_address;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->bus_number;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
    if((nullptr != dev) && (nullptr != port_numbers))
    {
        port_numbers[0] = dev->port_number;
        return 1;
    }
    else
    {
        return 0;
    }
    });

    /* Call bindDriver method */
    TEST_LOG("call BindDriver");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("bindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}
/*Test cases for bindDriver ends here*/

/*******************************************************************************************************************
 * Test function for :unbindDriver
 * getDeviceList :
 *                UnBinds the respective driver for the device.
 *
 *                @return sucessed
 * Use case coverage:
 *                @Success :1
 *                @Failure :0
 ********************************************************************************************************************/

/**
 * @brief : unbindDriver Method with given device name
 *          Check if  path parameter is missing from the parameters JSON object;
 *          then  bindDriver shall be failed and return Erorr code: ERROR_BAD_REQUEST
 *
 * @param[in]   :  deviceName
 * @return      :  error code: ERROR_NONE
 */
 TEST_F(USBDeviceTest, unbindDriverSuccessCase)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);

    /* Mock for Device List, 2 Devices */
    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_context *ctx, libusb_device ***list) {
        struct libusb_device **ret = nullptr;
        ssize_t len = 2;

        ret = (struct libusb_device **)malloc(len * sizeof(struct libusb_device *));

        if (nullptr == ret)
        {
            std::cout << "malloc failed";
            len = 0;
        }
        else
        {
            uint8_t bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            uint8_t device_address = MOCK_USB_DEVICE_ADDRESS_1;
            uint8_t port_number = MOCK_USB_DEVICE_PORT_1;

            for (int index = 0; index < len; ++index)
            {
                ret[index] =  (struct libusb_device *)malloc(sizeof(struct libusb_device));

                if (nullptr == ret[index])
                {
                    std::cout << "malloc failed";
                    len = 0;
                }
                else
                {
                    ret[index]->bus_number = bus_number;
                    ret[index]->device_address = device_address;
                    ret[index]->port_number = port_number;

                    bus_number += 1;
                    device_address += 1;
                    port_number += 1;
                }
            }
            *list = ret;
        }

        return len;
    });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_device **list, int unref_devices) {
        for (int index = 0; index < 2; ++index)
        {
        free(list[index]);
        }
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev, struct libusb_device_descriptor *desc) {
            desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
            desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
            return LIBUSB_SUCCESS;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->device_address;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->bus_number;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
    if((nullptr != dev) && (nullptr != port_numbers))
    {
        port_numbers[0] = dev->port_number;
        return 1;
    }
    else
    {
        return 0;
    }
    });

    /* Call unbindDriver method */
    TEST_LOG("call UnBindDriver");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("unbindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}
/*Test cases for unbindDriver ends here*/

/*******************************************************************************************************************
 * Test function for :getDeviceInfo
 * getDeviceInfo :
 *                Gets a usb device details
 *
 *                @return Response object containing the usb device info
 * Use case coverage:
 *                @Success :2
 *                @Failure :0
 ********************************************************************************************************************/
/**
 * @brief : getDeviceInfo Method with single mass storage USB
 *          Check if  path parameter is missing from the parameters JSON object;
 *          then  getDeviceInfo shall be failed and return Erorr code: ERROR_BAD_REQUEST
 *
 * @param[out]   :  USB Device Info
 * @return      :  error code: ERROR_NONE
 */
TEST_F(USBDeviceTest, getDeviceInfoSuccessCase)
{
    struct libusb_config_descriptor *temp_config_desc = nullptr;
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    /* Mock for Device List, 1 Device */
    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_context *ctx, libusb_device ***list) {
        struct libusb_device **ret = nullptr;
        ssize_t len = 1;
        ret = (struct libusb_device **)malloc(len * sizeof(struct libusb_device *));
        if (nullptr == ret)
        {
            std::cout << "malloc failed";
            len = 0;
        }
        else
        {
            uint8_t bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            uint8_t device_address = MOCK_USB_DEVICE_ADDRESS_1;
            uint8_t port_number = MOCK_USB_DEVICE_PORT_1;
            for (int index = 0; index < len; ++index)
            {
                ret[index] =  (struct libusb_device *)malloc(sizeof(struct libusb_device));
                if (nullptr == ret[index])
                {
                    std::cout << "malloc failed";
                    len = 0;
                }
                else
                {
                    ret[index]->bus_number = bus_number;
                    ret[index]->device_address = device_address;
                    ret[index]->port_number = port_number;
                    bus_number += 1;
                    device_address += 1;
                    port_number += 1;
                }
            }
            *list = ret;
        }
        return len;
    });
    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_device **list, int unref_devices) {
        for (int index = 0; index < 2; ++index)
        {
            free(list[index]);
        }
        free(list);
    });
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev, struct libusb_device_descriptor *desc) {
            desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
            desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
            desc->idVendor = 0x1234;
            desc->idProduct = 0x5678;
            desc->iManufacturer = 1;
            desc->iProduct = 2;
            desc->iSerialNumber = 3;
            return LIBUSB_SUCCESS;
    });
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->device_address;
    });
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->bus_number;
    });
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
        if((nullptr != dev) && (nullptr != port_numbers))
        {
            port_numbers[0] = dev->port_number;
            return 1;
        }
        else
        {
            return 0;
        }
    });
    ON_CALL(*p_libUSBImplMock, libusb_get_active_config_descriptor(::testing::_, ::testing::_))
    .WillByDefault([&temp_config_desc](libusb_device* pDev, struct libusb_config_descriptor** config_desc) {
        *config_desc =  (libusb_config_descriptor *)malloc(sizeof(libusb_config_descriptor));
        if (nullptr == *config_desc)
        {
            std::cout << "malloc failed";
            return (int)1;
        }
        else
        {
            temp_config_desc = *config_desc;
            (*config_desc)->bmAttributes = LIBUSB_CONFIG_ATT_BUS_POWERED;
        }
        return (int)LIBUSB_SUCCESS;
    });
    ON_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillByDefault([](libusb_device_handle *dev_handle,
        uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
        data[1] = LIBUSB_DT_STRING;
        if ( desc_index == 0 )
        {
            data[0] = 4;
            data[3] = 0x04;
            data[2] = 0x09;
        }
        else if ( desc_index == 1 /* Manufacturer */ )
        {
            const char *buf = MOCK_USB_DEVICE_MANUFACTURER;
            int buffer_len = strlen(buf) * 2,j = 0,index=2;
            memset(&data[2],0,length-2);
            while((data[index] = buf[j++]) != '\0')
            {
                index+=2;
            }
            data[0] = buffer_len+2;
        }
        else if ( desc_index == 2 /* ProductID */ )
        {
            const char *buf = MOCK_USB_DEVICE_PRODUCT;
            int buffer_len = strlen(buf) * 2,j = 0,index=2;
            memset(&data[2],0,length-2);
            while((data[index] = buf[j++]) != '\0')
            {
                index+=2;
            }
            data[0] = buffer_len+2;
        }
        else if ( desc_index == 3 /* SerialNumber */ )
        {
            const char *buf = MOCK_USB_DEVICE_SERIAL_NO;
            int buffer_len = strlen(buf) * 2,j = 0,index=2;
            memset(&data[2],0,length-2);
            while((data[index] = buf[j++]) != '\0')
            {
                index+=2;
            }
            data[0] = buffer_len+2;
        }
        return (int)data[0];
    });
    /* Call getDeviceInfo method */
    TEST_LOG("call getDeviceInfo");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string("{\"parentId\":0,\"deviceStatus\":1,\"deviceLevel\":0,\"portNumber\":1,\"vendorId\":4660,\"productId\":22136,\"protocol\":0,\"serialNumber\":\"\",\"device\":{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"100\\/001\",\"devicePath\":\"\"},\"flags\":\"AVAILABLE\",\"features\":0,\"busSpeed\":\"High\",\"numLanguageIds\":1,\"productInfo1\":{\"languageId\":1033,\"serialNumber\":\"0401805e4532973503374df52a239c898397d348\",\"manufacturer\":\"USB\",\"product\":\"SanDisk 3.2Gen1\"},\"productInfo2\":{\"languageId\":0,\"serialNumber\":\"\",\"manufacturer\":\"\",\"product\":\"\"},\"productInfo3\":{\"languageId\":0,\"serialNumber\":\"\",\"manufacturer\":\"\",\"product\":\"\"},\"productInfo4\":{\"languageId\":0,\"serialNumber\":\"\",\"manufacturer\":\"\",\"product\":\"\"}}"));
}

/*******************************************************************************************************************
 * L1 Notification Tests for USBDevice Plugin
 * 
 * Test Coverage:
 * - OnDevicePluggedIn notification via Job dispatch mechanism
 * - OnDevicePluggedOut notification via Job dispatch mechanism
 * - Multiple notification handlers
 * - Notification parameter validation
 ********************************************************************************************************************/

/**
 * @brief Test OnDevicePluggedIn notification using Job dispatch mechanism
 *        Verifies that when a USB device arrival event is dispatched,
 *        registered notification handlers receive OnDevicePluggedIn callback
 *        with correct device parameters
 * 
 * @return Core::ERROR_NONE on success
 */
TEST_F(USBDeviceTest, OnDevicePluggedIn_ViaJobDispatch_Success)
{
    TEST_LOG("Testing OnDevicePluggedIn notification via Job dispatch");
    
    // Create local notification handler
    NotificationHandler* notificationHandler = new NotificationHandler();
    
    // Register notification handler
    USBDeviceImpl->Register(notificationHandler);
    
    // Reset notification state
    notificationHandler->resetAll();
    
    // Create test device data
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 8;  // Mass Storage
    testDevice.deviceSubclass = 6;  // SCSI
    testDevice.deviceName = "100/001";
    testDevice.devicePath = "/dev/sda";
    
    TEST_LOG("Creating Job for DEVICE_ARRIVED event");
    
    // Use Job dispatch mechanism for natural notification flow
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        testDevice
    );
    
    TEST_LOG("Dispatching Job");
    job->Dispatch();
    
    // Wait for notification with timeout
    TEST_LOG("Waiting for OnDevicePluggedIn notification");
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_onDevicePluggedIn));
    
    // Verify notification was received
    EXPECT_TRUE(notificationHandler->getOnDevicePluggedInReceived());
    
    // Validate device parameters
    Exchange::IUSBDevice::USBDevice receivedDevice = notificationHandler->getPluggedInDevice();
    EXPECT_EQ(testDevice.deviceClass, receivedDevice.deviceClass);
    EXPECT_EQ(testDevice.deviceSubclass, receivedDevice.deviceSubclass);
    EXPECT_EQ(testDevice.deviceName, receivedDevice.deviceName);
    EXPECT_EQ(testDevice.devicePath, receivedDevice.devicePath);
    
    TEST_LOG("OnDevicePluggedIn notification received and validated successfully");
    
    // Cleanup
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/**
 * @brief Test OnDevicePluggedOut notification using Job dispatch mechanism
 *        Verifies that when a USB device removal event is dispatched,
 *        registered notification handlers receive OnDevicePluggedOut callback
 *        with correct device parameters
 * 
 * @return Core::ERROR_NONE on success
 */
TEST_F(USBDeviceTest, OnDevicePluggedOut_ViaJobDispatch_Success)
{
    TEST_LOG("Testing OnDevicePluggedOut notification via Job dispatch");
    
    // Create local notification handler
    NotificationHandler* notificationHandler = new NotificationHandler();
    
    // Register notification handler
    USBDeviceImpl->Register(notificationHandler);
    
    // Reset notification state
    notificationHandler->resetAll();
    
    // Create test device data
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 8;  // Mass Storage
    testDevice.deviceSubclass = 6;  // SCSI
    testDevice.deviceName = "101/002";
    testDevice.devicePath = "/dev/sdb";
    
    TEST_LOG("Creating Job for DEVICE_LEFT event");
    
    // Use Job dispatch mechanism for natural notification flow
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
        testDevice
    );
    
    TEST_LOG("Dispatching Job");
    job->Dispatch();
    
    // Wait for notification with timeout
    TEST_LOG("Waiting for OnDevicePluggedOut notification");
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_onDevicePluggedOut));
    
    // Verify notification was received
    EXPECT_TRUE(notificationHandler->getOnDevicePluggedOutReceived());
    
    // Validate device parameters
    Exchange::IUSBDevice::USBDevice receivedDevice = notificationHandler->getPluggedOutDevice();
    EXPECT_EQ(testDevice.deviceClass, receivedDevice.deviceClass);
    EXPECT_EQ(testDevice.deviceSubclass, receivedDevice.deviceSubclass);
    EXPECT_EQ(testDevice.deviceName, receivedDevice.deviceName);
    EXPECT_EQ(testDevice.devicePath, receivedDevice.devicePath);
    
    TEST_LOG("OnDevicePluggedOut notification received and validated successfully");
    
    // Cleanup
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/**
 * @brief Test multiple notification handlers receive OnDevicePluggedIn
 *        Verifies that all registered handlers are notified when device is plugged in
 * 
 * @return Core::ERROR_NONE on success
 */
TEST_F(USBDeviceTest, OnDevicePluggedIn_MultipleHandlers_AllNotified)
{
    TEST_LOG("Testing OnDevicePluggedIn with multiple notification handlers");
    
    // Create multiple notification handlers
    NotificationHandler* handler1 = new NotificationHandler();
    NotificationHandler* handler2 = new NotificationHandler();
    NotificationHandler* handler3 = new NotificationHandler();
    
    // Register all handlers
    USBDeviceImpl->Register(handler1);
    USBDeviceImpl->Register(handler2);
    USBDeviceImpl->Register(handler3);
    
    // Reset all notification states
    handler1->resetAll();
    handler2->resetAll();
    handler3->resetAll();
    
    // Create test device data
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 3;  // HID
    testDevice.deviceSubclass = 1;  // Boot Interface
    testDevice.deviceName = "100/003";
    testDevice.devicePath = "/dev/hidraw0";
    
    TEST_LOG("Dispatching device arrival event");
    
    // Dispatch event
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        testDevice
    );
    job->Dispatch();
    
    // Verify all handlers received notification
    TEST_LOG("Verifying all handlers received notification");
    EXPECT_TRUE(handler1->WaitForRequestStatus(2000, USBDevice_onDevicePluggedIn));
    EXPECT_TRUE(handler2->WaitForRequestStatus(2000, USBDevice_onDevicePluggedIn));
    EXPECT_TRUE(handler3->WaitForRequestStatus(2000, USBDevice_onDevicePluggedIn));
    
    EXPECT_TRUE(handler1->getOnDevicePluggedInReceived());
    EXPECT_TRUE(handler2->getOnDevicePluggedInReceived());
    EXPECT_TRUE(handler3->getOnDevicePluggedInReceived());
    
    // Verify all handlers received same device data
    Exchange::IUSBDevice::USBDevice received1 = handler1->getPluggedInDevice();
    Exchange::IUSBDevice::USBDevice received2 = handler2->getPluggedInDevice();
    Exchange::IUSBDevice::USBDevice received3 = handler3->getPluggedInDevice();
    
    EXPECT_EQ(testDevice.deviceName, received1.deviceName);
    EXPECT_EQ(testDevice.deviceName, received2.deviceName);
    EXPECT_EQ(testDevice.deviceName, received3.deviceName);
    
    TEST_LOG("All handlers notified successfully");
    
    // Cleanup
    USBDeviceImpl->Unregister(handler1);
    USBDeviceImpl->Unregister(handler2);
    USBDeviceImpl->Unregister(handler3);
    
    handler1->Release();
    handler2->Release();
    handler3->Release();
}

/**
 * @brief Test multiple notification handlers receive OnDevicePluggedOut
 *        Verifies that all registered handlers are notified when device is removed
 * 
 * @return Core::ERROR_NONE on success
 */
TEST_F(USBDeviceTest, OnDevicePluggedOut_MultipleHandlers_AllNotified)
{
    TEST_LOG("Testing OnDevicePluggedOut with multiple notification handlers");
    
    // Create multiple notification handlers
    NotificationHandler* handler1 = new NotificationHandler();
    NotificationHandler* handler2 = new NotificationHandler();
    NotificationHandler* handler3 = new NotificationHandler();
    
    // Register all handlers
    USBDeviceImpl->Register(handler1);
    USBDeviceImpl->Register(handler2);
    USBDeviceImpl->Register(handler3);
    
    // Reset all notification states
    handler1->resetAll();
    handler2->resetAll();
    handler3->resetAll();
    
    // Create test device data
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 9;  // Hub
    testDevice.deviceSubclass = 0;
    testDevice.deviceName = "101/005";
    testDevice.devicePath = "";
    
    TEST_LOG("Dispatching device removal event");
    
    // Dispatch event
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
        testDevice
    );
    job->Dispatch();
    
    // Verify all handlers received notification
    TEST_LOG("Verifying all handlers received notification");
    EXPECT_TRUE(handler1->WaitForRequestStatus(2000, USBDevice_onDevicePluggedOut));
    EXPECT_TRUE(handler2->WaitForRequestStatus(2000, USBDevice_onDevicePluggedOut));
    EXPECT_TRUE(handler3->WaitForRequestStatus(2000, USBDevice_onDevicePluggedOut));
    
    EXPECT_TRUE(handler1->getOnDevicePluggedOutReceived());
    EXPECT_TRUE(handler2->getOnDevicePluggedOutReceived());
    EXPECT_TRUE(handler3->getOnDevicePluggedOutReceived());
    
    // Verify all handlers received same device data
    Exchange::IUSBDevice::USBDevice received1 = handler1->getPluggedOutDevice();
    Exchange::IUSBDevice::USBDevice received2 = handler2->getPluggedOutDevice();
    Exchange::IUSBDevice::USBDevice received3 = handler3->getPluggedOutDevice();
    
    EXPECT_EQ(testDevice.deviceClass, received1.deviceClass);
    EXPECT_EQ(testDevice.deviceClass, received2.deviceClass);
    EXPECT_EQ(testDevice.deviceClass, received3.deviceClass);
    
    TEST_LOG("All handlers notified successfully");
    
    // Cleanup
    USBDeviceImpl->Unregister(handler1);
    USBDeviceImpl->Unregister(handler2);
    USBDeviceImpl->Unregister(handler3);
    
    handler1->Release();
    handler2->Release();
    handler3->Release();
}

/**
 * @brief Test sequential device events (plug in followed by plug out)
 *        Verifies that handlers receive both notifications in sequence
 * 
 * @return Core::ERROR_NONE on success
 */
TEST_F(USBDeviceTest, SequentialEvents_PlugInThenPlugOut_BothNotifications)
{
    TEST_LOG("Testing sequential plug in and plug out events");
    
    // Create notification handler
    NotificationHandler* notificationHandler = new NotificationHandler();
    
    // Register handler
    USBDeviceImpl->Register(notificationHandler);
    
    // Reset state
    notificationHandler->resetAll();
    
    // Create test device data
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 8;
    testDevice.deviceSubclass = 6;
    testDevice.deviceName = "100/010";
    testDevice.devicePath = "/dev/sdc";
    
    // First event: Device plugged in
    TEST_LOG("Dispatching device plug in event");
    auto jobPlugIn = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        testDevice
    );
    jobPlugIn->Dispatch();
    
    // Verify plug in notification
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_onDevicePluggedIn));
    EXPECT_TRUE(notificationHandler->getOnDevicePluggedInReceived());
    EXPECT_EQ(testDevice.deviceName, notificationHandler->getPluggedInDevice().deviceName);
    
    TEST_LOG("Plug in notification verified, now testing plug out");
    
    // Reset only plug out state
    notificationHandler->resetOnDevicePluggedOut();
    
    // Second event: Device plugged out
    TEST_LOG("Dispatching device plug out event");
    auto jobPlugOut = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
        testDevice
    );
    jobPlugOut->Dispatch();
    
    // Verify plug out notification
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_onDevicePluggedOut));
    EXPECT_TRUE(notificationHandler->getOnDevicePluggedOutReceived());
    EXPECT_EQ(testDevice.deviceName, notificationHandler->getPluggedOutDevice().deviceName);
    
    // Verify plug in is still recorded
    EXPECT_TRUE(notificationHandler->getOnDevicePluggedInReceived());
    
    TEST_LOG("Sequential events handled successfully");
    
    // Cleanup
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/**
 * @brief Test notification with various device classes
 *        Verifies notifications work correctly for different USB device types
 * 
 * @return Core::ERROR_NONE on success
 */
TEST_F(USBDeviceTest, OnDevicePluggedIn_VariousDeviceClasses_AllNotified)
{
    TEST_LOG("Testing notifications with various device classes");
    
    NotificationHandler* notificationHandler = new NotificationHandler();
    USBDeviceImpl->Register(notificationHandler);
    
    // Test Mass Storage device
    TEST_LOG("Testing Mass Storage device (class 8)");
    notificationHandler->resetAll();
    Exchange::IUSBDevice::USBDevice massStorageDevice;
    massStorageDevice.deviceClass = 8;  // Mass Storage
    massStorageDevice.deviceSubclass = 6;
    massStorageDevice.deviceName = "100/020";
    massStorageDevice.devicePath = "/dev/sdd";
    
    auto job1 = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        massStorageDevice
    );
    job1->Dispatch();
    
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_onDevicePluggedIn));
    EXPECT_EQ(8, notificationHandler->getPluggedInDevice().deviceClass);
    
    // Test HID device
    TEST_LOG("Testing HID device (class 3)");
    notificationHandler->resetAll();
    Exchange::IUSBDevice::USBDevice hidDevice;
    hidDevice.deviceClass = 3;  // HID
    hidDevice.deviceSubclass = 1;
    hidDevice.deviceName = "100/021";
    hidDevice.devicePath = "/dev/hidraw1";
    
    auto job2 = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        hidDevice
    );
    job2->Dispatch();
    
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_onDevicePluggedIn));
    EXPECT_EQ(3, notificationHandler->getPluggedInDevice().deviceClass);
    
    // Test Audio device
    TEST_LOG("Testing Audio device (class 1)");
    notificationHandler->resetAll();
    Exchange::IUSBDevice::USBDevice audioDevice;
    audioDevice.deviceClass = 1;  // Audio
    audioDevice.deviceSubclass = 2;
    audioDevice.deviceName = "100/022";
    audioDevice.devicePath = "";
    
    auto job3 = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        audioDevice
    );
    job3->Dispatch();
    
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_onDevicePluggedIn));
    EXPECT_EQ(1, notificationHandler->getPluggedInDevice().deviceClass);
    
    TEST_LOG("All device classes notified successfully");
    
    // Cleanup
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/**
 * @brief Test unregistered handler does not receive notifications
 *        Verifies that after unregistering, no further notifications are received
 * 
 * @return Core::ERROR_NONE on success
 */
TEST_F(USBDeviceTest, UnregisteredHandler_NoNotifications_Success)
{
    TEST_LOG("Testing that unregistered handler does not receive notifications");
    
    NotificationHandler* notificationHandler = new NotificationHandler();
    
    // Register and then immediately unregister
    USBDeviceImpl->Register(notificationHandler);
    USBDeviceImpl->Unregister(notificationHandler);
    
    // Reset state
    notificationHandler->resetAll();
    
    // Create test device
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 8;
    testDevice.deviceSubclass = 6;
    testDevice.deviceName = "100/030";
    testDevice.devicePath = "/dev/sde";
    
    // Dispatch event
    TEST_LOG("Dispatching event to unregistered handler");
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        testDevice
    );
    job->Dispatch();
    
    // Wait a bit to ensure no notification is received
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify no notification was received
    EXPECT_FALSE(notificationHandler->getOnDevicePluggedInReceived());
    
    TEST_LOG("Confirmed unregistered handler did not receive notification");
    
    // Cleanup
    notificationHandler->Release();
}

/*Test cases for L1 Notifications end here*/
