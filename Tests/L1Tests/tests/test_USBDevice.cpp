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

// Add after existing includes and before the test fixture class

typedef enum : uint32_t {
    USBDevice_Event_DevicePluggedIn  = 0x00000001,
    USBDevice_Event_DevicePluggedOut = 0x00000002,
} USBDeviceEventType_t;

class USBDeviceNotificationHandler : public Exchange::IUSBDevice::INotification {
private:
    std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t m_event_signalled;
    
    // Event flags
    bool m_devicePluggedInReceived;
    bool m_devicePluggedOutReceived;
    
    // Parameter storage for validation
    Exchange::IUSBDevice::USBDevice m_pluggedInDevice;
    Exchange::IUSBDevice::USBDevice m_pluggedOutDevice;

public:
    USBDeviceNotificationHandler()
        : m_event_signalled(0)
        , m_devicePluggedInReceived(false)
        , m_devicePluggedOutReceived(false)
    {
    }

    virtual ~USBDeviceNotificationHandler()
    {
    }

    BEGIN_INTERFACE_MAP(USBDeviceNotificationHandler)
    INTERFACE_ENTRY(Exchange::IUSBDevice::INotification)
    END_INTERFACE_MAP

    // INotification interface implementation
    void OnDevicePluggedIn(const Exchange::IUSBDevice::USBDevice& device) override
    {
        TEST_LOG("OnDevicePluggedIn notification received");
        
        std::unique_lock<std::mutex> lock(m_mutex);
        
        m_devicePluggedInReceived = true;
        m_pluggedInDevice.deviceClass = device.deviceClass;
        m_pluggedInDevice.deviceSubclass = device.deviceSubclass;
        m_pluggedInDevice.deviceName = device.deviceName;
        m_pluggedInDevice.devicePath = device.devicePath;
        
        m_event_signalled |= USBDevice_Event_DevicePluggedIn;
        m_condition_variable.notify_all();
    }

    void OnDevicePluggedOut(const Exchange::IUSBDevice::USBDevice& device) override
    {
        TEST_LOG("OnDevicePluggedOut notification received");
        
        std::unique_lock<std::mutex> lock(m_mutex);
        
        m_devicePluggedOutReceived = true;
        m_pluggedOutDevice.deviceClass = device.deviceClass;
        m_pluggedOutDevice.deviceSubclass = device.deviceSubclass;
        m_pluggedOutDevice.deviceName = device.deviceName;
        m_pluggedOutDevice.devicePath = device.devicePath;
        
        m_event_signalled |= USBDevice_Event_DevicePluggedOut;
        m_condition_variable.notify_all();
    }

    // Wait for specific event(s) with timeout
    bool WaitForRequestStatus(uint32_t timeout_ms, USBDeviceEventType_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        auto now = std::chrono::steady_clock::now();
        auto timeout_time = now + std::chrono::milliseconds(timeout_ms);
        
        while (!(m_event_signalled & expected_status))
        {
            if (m_condition_variable.wait_until(lock, timeout_time) == std::cv_status::timeout)
            {
                TEST_LOG("WaitForRequestStatus timed out. Expected: 0x%08x, Received: 0x%08x", 
                         expected_status, m_event_signalled);
                return false;
            }
        }
        
        TEST_LOG("WaitForRequestStatus successful. Expected: 0x%08x, Received: 0x%08x", 
                 expected_status, m_event_signalled);
        return true;
    }

    // Getter methods for parameter validation
    bool GetDevicePluggedInReceived() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_devicePluggedInReceived;
    }

    bool GetDevicePluggedOutReceived() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_devicePluggedOutReceived;
    }

    Exchange::IUSBDevice::USBDevice GetPluggedInDevice() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_pluggedInDevice;
    }

    Exchange::IUSBDevice::USBDevice GetPluggedOutDevice() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_pluggedOutDevice;
    }

    // Reset methods for reusing handler across tests
    void ResetEvent(USBDeviceEventType_t event_type)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        if (event_type & USBDevice_Event_DevicePluggedIn)
        {
            m_devicePluggedInReceived = false;
            m_pluggedInDevice = Exchange::IUSBDevice::USBDevice();
        }
        
        if (event_type & USBDevice_Event_DevicePluggedOut)
        {
            m_devicePluggedOutReceived = false;
            m_pluggedOutDevice = Exchange::IUSBDevice::USBDevice();
        }
        
        m_event_signalled &= ~event_type;
    }

    void ResetAllEvents()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        m_event_signalled = 0;
        m_devicePluggedInReceived = false;
        m_devicePluggedOutReceived = false;
        m_pluggedInDevice = Exchange::IUSBDevice::USBDevice();
        m_pluggedOutDevice = Exchange::IUSBDevice::USBDevice();
    }

    uint32_t GetSignalledEvents() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_event_signalled;
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

// Add after the existing USBDeviceNotificationHandler class and before USBDeviceTest class

/*******************************************************************************************************************
 * L1 Notification Tests for USBDevice Plugin
 * 
 * These tests verify that notification methods are properly triggered when:
 * 1. USB devices are plugged in (OnDevicePluggedIn)
 * 2. USB devices are plugged out (OnDevicePluggedOut)
 * 
 * Test Pattern:
 * - Register notification handler
 * - Trigger notification via accessible method (libUSB hotplug callbacks)
 * - Verify notification received with correct parameters
 * - Unregister and cleanup
 ********************************************************************************************************************/

/**
 * @brief Test OnDevicePluggedIn notification via libUSB hotplug callback
 *        Simulates USB mass storage device insertion event
 * 
 * Notification Flow:
 * libUSBHotPlugCallbackDeviceAttached() → dispatchEvent() → 
 * Job dispatch → Dispatch() → OnDevicePluggedIn()
 */
TEST_F(USBDeviceTest, OnDevicePluggedIn_ViaHotplugCallback_MassStorageDevice)
{
    USBDeviceNotificationHandler notificationHandler;
    libusb_device* mockDevice = nullptr;
    
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    
    // Setup mock device structure
    mockDevice = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(mockDevice, nullptr);
    
    mockDevice->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mockDevice->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mockDevice->port_number = MOCK_USB_DEVICE_PORT_1;
    
    // Mock device descriptor for mass storage class
    ON_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->idVendor = 0x0951;
                desc->idProduct = 0x1666;
                return LIBUSB_SUCCESS;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly(::testing::Return(MOCK_USB_DEVICE_ADDRESS_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
            if((nullptr != dev) && (nullptr != port_numbers))
            {
                port_numbers[0] = dev->port_number;
                return 1;
            }
            return 0;
        });
    
    // Register notification handler with implementation
    if (USBDeviceImpl.IsValid())
    {
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Register(&notificationHandler));
        
        // Trigger notification via libUSB hotplug callback (simulating device insertion)
        TEST_LOG("Triggering OnDevicePluggedIn via libUSBHotPlugCallbackDeviceAttached");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(
            nullptr, 
            mockDevice, 
            LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 
            nullptr
        );
        
        // Wait for notification to be dispatched through worker pool
        EXPECT_TRUE(notificationHandler.WaitForRequestStatus(5000, USBDevice_Event_DevicePluggedIn));
        
        // Verify notification was received
        EXPECT_TRUE(notificationHandler.GetDevicePluggedInReceived());
        
        // Verify device parameters
        Exchange::IUSBDevice::USBDevice pluggedInDevice = notificationHandler.GetPluggedInDevice();
        EXPECT_EQ(pluggedInDevice.deviceClass, LIBUSB_CLASS_MASS_STORAGE);
        EXPECT_EQ(pluggedInDevice.deviceSubclass, LIBUSB_CLASS_MASS_STORAGE);
        EXPECT_EQ(pluggedInDevice.deviceName, "100/001");
        EXPECT_EQ(pluggedInDevice.devicePath, "/dev/sda");
        
        TEST_LOG("OnDevicePluggedIn notification verified successfully");
        
        // Cleanup
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Unregister(&notificationHandler));
    }
    
    free(mockDevice);
}

/**
 * @brief Test OnDevicePluggedOut notification via libUSB hotplug callback
 *        Simulates USB mass storage device removal event
 * 
 * Notification Flow:
 * libUSBHotPlugCallbackDeviceDetached() → dispatchEvent() → 
 * Job dispatch → Dispatch() → OnDevicePluggedOut()
 */
TEST_F(USBDeviceTest, OnDevicePluggedOut_ViaHotplugCallback_MassStorageDevice)
{
    USBDeviceNotificationHandler notificationHandler;
    libusb_device* mockDevice = nullptr;
    
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    
    // Setup mock device structure
    mockDevice = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(mockDevice, nullptr);
    
    mockDevice->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mockDevice->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mockDevice->port_number = MOCK_USB_DEVICE_PORT_1;
    
    // Mock device descriptor for mass storage class
    ON_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->idVendor = 0x0951;
                desc->idProduct = 0x1666;
                return LIBUSB_SUCCESS;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly(::testing::Return(MOCK_USB_DEVICE_ADDRESS_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
            if((nullptr != dev) && (nullptr != port_numbers))
            {
                port_numbers[0] = dev->port_number;
                return 1;
            }
            return 0;
        });
    
    // Register notification handler with implementation
    if (USBDeviceImpl.IsValid())
    {
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Register(&notificationHandler));
        
        // Trigger notification via libUSB hotplug callback (simulating device removal)
        TEST_LOG("Triggering OnDevicePluggedOut via libUSBHotPlugCallbackDeviceDetached");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(
            nullptr, 
            mockDevice, 
            LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 
            nullptr
        );
        
        // Wait for notification to be dispatched through worker pool
        EXPECT_TRUE(notificationHandler.WaitForRequestStatus(5000, USBDevice_Event_DevicePluggedOut));
        
        // Verify notification was received
        EXPECT_TRUE(notificationHandler.GetDevicePluggedOutReceived());
        
        // Verify device parameters
        Exchange::IUSBDevice::USBDevice pluggedOutDevice = notificationHandler.GetPluggedOutDevice();
        EXPECT_EQ(pluggedOutDevice.deviceClass, LIBUSB_CLASS_MASS_STORAGE);
        EXPECT_EQ(pluggedOutDevice.deviceSubclass, LIBUSB_CLASS_MASS_STORAGE);
        EXPECT_EQ(pluggedOutDevice.deviceName, "100/001");
        EXPECT_EQ(pluggedOutDevice.devicePath, "/dev/sda");
        
        TEST_LOG("OnDevicePluggedOut notification verified successfully");
        
        // Cleanup
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Unregister(&notificationHandler));
    }
    
    free(mockDevice);
}

/**
 * @brief Test OnDevicePluggedIn notification with multiple USB devices
 *        Verifies notifications for sequential device insertions
 */
TEST_F(USBDeviceTest, OnDevicePluggedIn_ViaHotplugCallback_MultipleDevices)
{
    USBDeviceNotificationHandler notificationHandler;
    libusb_device* mockDevice1 = nullptr;
    libusb_device* mockDevice2 = nullptr;
    
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);
    
    // Setup first mock device
    mockDevice1 = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(mockDevice1, nullptr);
    mockDevice1->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mockDevice1->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mockDevice1->port_number = MOCK_USB_DEVICE_PORT_1;
    
    // Setup second mock device
    mockDevice2 = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(mockDevice2, nullptr);
    mockDevice2->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_2;
    mockDevice2->device_address = MOCK_USB_DEVICE_ADDRESS_2;
    mockDevice2->port_number = MOCK_USB_DEVICE_PORT_2;
    
    ON_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                return LIBUSB_SUCCESS;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly(::testing::Invoke(
            [](libusb_device *dev) {
                return dev->bus_number;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly(::testing::Invoke(
            [](libusb_device *dev) {
                return dev->device_address;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
            if((nullptr != dev) && (nullptr != port_numbers))
            {
                port_numbers[0] = dev->port_number;
                return 1;
            }
            return 0;
        });
    
    if (USBDeviceImpl.IsValid())
    {
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Register(&notificationHandler));
        
        // Trigger first device insertion
        TEST_LOG("Triggering first device insertion");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(
            nullptr, mockDevice1, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);
        
        EXPECT_TRUE(notificationHandler.WaitForRequestStatus(5000, USBDevice_Event_DevicePluggedIn));
        EXPECT_TRUE(notificationHandler.GetDevicePluggedInReceived());
        
        Exchange::IUSBDevice::USBDevice device1 = notificationHandler.GetPluggedInDevice();
        EXPECT_EQ(device1.deviceName, "100/001");
        
        // Reset for second device
        notificationHandler.ResetEvent(USBDevice_Event_DevicePluggedIn);
        
        // Trigger second device insertion
        TEST_LOG("Triggering second device insertion");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(
            nullptr, mockDevice2, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);
        
        EXPECT_TRUE(notificationHandler.WaitForRequestStatus(5000, USBDevice_Event_DevicePluggedIn));
        EXPECT_TRUE(notificationHandler.GetDevicePluggedInReceived());
        
        Exchange::IUSBDevice::USBDevice device2 = notificationHandler.GetPluggedInDevice();
        EXPECT_EQ(device2.deviceName, "101/002");
        
        TEST_LOG("Multiple device notifications verified successfully");
        
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Unregister(&notificationHandler));
    }
    
    free(mockDevice1);
    free(mockDevice2);
}

/**
 * @brief Test OnDevicePluggedOut notification with multiple USB devices
 *        Verifies notifications for sequential device removals
 */
TEST_F(USBDeviceTest, OnDevicePluggedOut_ViaHotplugCallback_MultipleDevices)
{
    USBDeviceNotificationHandler notificationHandler;
    libusb_device* mockDevice1 = nullptr;
    libusb_device* mockDevice2 = nullptr;
    
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);
    
    // Setup first mock device
    mockDevice1 = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(mockDevice1, nullptr);
    mockDevice1->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mockDevice1->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mockDevice1->port_number = MOCK_USB_DEVICE_PORT_1;
    
    // Setup second mock device
    mockDevice2 = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(mockDevice2, nullptr);
    mockDevice2->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_2;
    mockDevice2->device_address = MOCK_USB_DEVICE_ADDRESS_2;
    mockDevice2->port_number = MOCK_USB_DEVICE_PORT_2;
    
    ON_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                return LIBUSB_SUCCESS;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly(::testing::Invoke(
            [](libusb_device *dev) {
                return dev->bus_number;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly(::testing::Invoke(
            [](libusb_device *dev) {
                return dev->device_address;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
            if((nullptr != dev) && (nullptr != port_numbers))
            {
                port_numbers[0] = dev->port_number;
                return 1;
            }
            return 0;
        });
    
    if (USBDeviceImpl.IsValid())
    {
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Register(&notificationHandler));
        
        // Trigger first device removal
        TEST_LOG("Triggering first device removal");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(
            nullptr, mockDevice1, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, nullptr);
        
        EXPECT_TRUE(notificationHandler.WaitForRequestStatus(5000, USBDevice_Event_DevicePluggedOut));
        EXPECT_TRUE(notificationHandler.GetDevicePluggedOutReceived());
        
        Exchange::IUSBDevice::USBDevice device1 = notificationHandler.GetPluggedOutDevice();
        EXPECT_EQ(device1.deviceName, "100/001");
        
        // Reset for second device
        notificationHandler.ResetEvent(USBDevice_Event_DevicePluggedOut);
        
        // Trigger second device removal
        TEST_LOG("Triggering second device removal");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(
            nullptr, mockDevice2, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, nullptr);
        
        EXPECT_TRUE(notificationHandler.WaitForRequestStatus(5000, USBDevice_Event_DevicePluggedOut));
        EXPECT_TRUE(notificationHandler.GetDevicePluggedOutReceived());
        
        Exchange::IUSBDevice::USBDevice device2 = notificationHandler.GetPluggedOutDevice();
        EXPECT_EQ(device2.deviceName, "101/002");
        
        TEST_LOG("Multiple device removal notifications verified successfully");
        
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Unregister(&notificationHandler));
    }
    
    free(mockDevice1);
    free(mockDevice2);
}

/**
 * @brief Test notification handler reset functionality
 *        Verifies that event flags can be properly reset between tests
 */
TEST_F(USBDeviceTest, NotificationHandler_ResetEvents_Success)
{
    USBDeviceNotificationHandler notificationHandler;
    libusb_device* mockDevice = nullptr;
    
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    
    mockDevice = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(mockDevice, nullptr);
    mockDevice->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mockDevice->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mockDevice->port_number = MOCK_USB_DEVICE_PORT_1;
    
    ON_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                return LIBUSB_SUCCESS;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly(::testing::Return(MOCK_USB_DEVICE_ADDRESS_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
            if((nullptr != dev) && (nullptr != port_numbers))
            {
                port_numbers[0] = dev->port_number;
                return 1;
            }
            return 0;
        });
    
    if (USBDeviceImpl.IsValid())
    {
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Register(&notificationHandler));
        
        // Trigger device plugged in event
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(
            nullptr, mockDevice, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);
        
        EXPECT_TRUE(notificationHandler.WaitForRequestStatus(5000, USBDevice_Event_DevicePluggedIn));
        EXPECT_TRUE(notificationHandler.GetDevicePluggedInReceived());
        
        // Reset the specific event
        notificationHandler.ResetEvent(USBDevice_Event_DevicePluggedIn);
        EXPECT_FALSE(notificationHandler.GetDevicePluggedInReceived());
        EXPECT_EQ(notificationHandler.GetSignalledEvents(), 0);
        
        // Trigger device plugged out event
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(
            nullptr, mockDevice, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, nullptr);
        
        EXPECT_TRUE(notificationHandler.WaitForRequestStatus(5000, USBDevice_Event_DevicePluggedOut));
        EXPECT_TRUE(notificationHandler.GetDevicePluggedOutReceived());
        
        // Reset all events
        notificationHandler.ResetAllEvents();
        EXPECT_FALSE(notificationHandler.GetDevicePluggedInReceived());
        EXPECT_FALSE(notificationHandler.GetDevicePluggedOutReceived());
        EXPECT_EQ(notificationHandler.GetSignalledEvents(), 0);
        
        TEST_LOG("Event reset functionality verified successfully");
        
        EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->Unregister(&notificationHandler));
    }
    
    free(mockDevice);
}

/*Test cases for L1 Notifications end here*/
