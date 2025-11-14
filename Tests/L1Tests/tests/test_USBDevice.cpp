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
#include <string>
#include <vector>
#include <cstdio>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "USBDevice.h"
#include "USBDeviceImplementation.h"
#include "libUSBMock.h"
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
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

/**
 * @brief Event type enumeration for USBDevice notifications
 * Uses bit flags (powers of 2) for efficient event handling
 */
typedef enum : uint32_t {
    USBDEVICE_EVENT_DEVICE_PLUGGED_IN  = 0x00000001,
    USBDEVICE_EVENT_DEVICE_PLUGGED_OUT = 0x00000002,
    USBDEVICE_EVENT_STATUS_INVALID     = 0x00000000
} USBDeviceEventType_t;

/**
 * @brief NotificationHandler class for USBDevice plugin notifications
 * 
 * This class implements the IUSBDevice::INotification interface and provides
 * thread-safe handling of USB device plug-in/plug-out events. It follows
 * established testing patterns for notification verification.
 * 
 * Usage:
 *   USBDeviceNotificationHandler handler;
 *   usbDevice->Register(&handler);
 *   // Trigger USB event
 *   handler.WaitForRequestStatus(timeout, USBDEVICE_EVENT_DEVICE_PLUGGED_IN);
 *   // Validate parameters
 *   auto device = handler.GetLastPluggedInDevice();
 */
class USBDeviceNotificationHandler : public Exchange::IUSBDevice::INotification {
private:
    /** @brief Mutex for thread-safe access */
    std::mutex m_mutex;

    /** @brief Condition variable for event signaling */
    std::condition_variable m_condition_variable;

    /** @brief Bit flags indicating which events have been signalled */
    uint32_t m_event_signalled;

    /** @brief Storage for last plugged-in device parameters */
    Exchange::IUSBDevice::USBDevice m_last_plugged_in_device;

    /** @brief Storage for last plugged-out device parameters */
    Exchange::IUSBDevice::USBDevice m_last_plugged_out_device;

    /** @brief Flag indicating if a device was plugged in */
    bool m_device_plugged_in_received;

    /** @brief Flag indicating if a device was plugged out */
    bool m_device_plugged_out_received;

    BEGIN_INTERFACE_MAP(Notification)
    INTERFACE_ENTRY(Exchange::IUSBDevice::INotification)
    END_INTERFACE_MAP

public:
    /**
     * @brief Constructor
     * Initializes all member variables to default values
     */
    USBDeviceNotificationHandler()
        : m_mutex()
        , m_condition_variable()
        , m_event_signalled(0)
        , m_last_plugged_in_device()
        , m_last_plugged_out_device()
        , m_device_plugged_in_received(false)
        , m_device_plugged_out_received(false)
    {
        TEST_LOG("USBDeviceNotificationHandler created");
    }

    /**
     * @brief Destructor
     */
    ~USBDeviceNotificationHandler()
    {
        TEST_LOG("USBDeviceNotificationHandler destroyed");
    }

    /**
     * @brief Notification callback for device plugged-in event
     * 
     * Called by the USBDevice plugin when a USB device is connected.
     * Stores device information and signals waiting threads.
     * 
     * @param device USB device information
     */
    void OnDevicePluggedIn(const Exchange::IUSBDevice::USBDevice &device) override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        TEST_LOG("OnDevicePluggedIn event received");
        TEST_LOG("  deviceClass: %u", device.deviceClass);
        TEST_LOG("  deviceSubclass: %u", device.deviceSubclass);
        TEST_LOG("  deviceName: %s", device.deviceName.c_str());
        TEST_LOG("  devicePath: %s", device.devicePath.c_str());

        // Store device information for validation
        m_last_plugged_in_device.deviceClass = device.deviceClass;
        m_last_plugged_in_device.deviceSubclass = device.deviceSubclass;
        m_last_plugged_in_device.deviceName = device.deviceName;
        m_last_plugged_in_device.devicePath = device.devicePath;
        m_device_plugged_in_received = true;

        // Signal the event
        m_event_signalled |= USBDEVICE_EVENT_DEVICE_PLUGGED_IN;
        m_condition_variable.notify_one();
    }

    /**
     * @brief Notification callback for device plugged-out event
     * 
     * Called by the USBDevice plugin when a USB device is disconnected.
     * Stores device information and signals waiting threads.
     * 
     * @param device USB device information
     */
    void OnDevicePluggedOut(const Exchange::IUSBDevice::USBDevice &device) override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        TEST_LOG("OnDevicePluggedOut event received");
        TEST_LOG("  deviceClass: %u", device.deviceClass);
        TEST_LOG("  deviceSubclass: %u", device.deviceSubclass);
        TEST_LOG("  deviceName: %s", device.deviceName.c_str());
        TEST_LOG("  devicePath: %s", device.devicePath.c_str());

        // Store device information for validation
        m_last_plugged_out_device.deviceClass = device.deviceClass;
        m_last_plugged_out_device.deviceSubclass = device.deviceSubclass;
        m_last_plugged_out_device.deviceName = device.deviceName;
        m_last_plugged_out_device.devicePath = device.devicePath;
        m_device_plugged_out_received = true;

        // Signal the event
        m_event_signalled |= USBDEVICE_EVENT_DEVICE_PLUGGED_OUT;
        m_condition_variable.notify_one();
    }

    /**
     * @brief Wait for a specific event to be signalled
     * 
     * Blocks until the expected event is received or timeout occurs.
     * Uses condition variable for efficient waiting.
     * 
     * @param timeout_ms Timeout in milliseconds
     * @param expected_status Event type to wait for (bit flag)
     * @return uint32_t Bit flags of events that were signalled (0 on timeout)
     */
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, USBDeviceEventType_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timeout(timeout_ms);
        uint32_t signalled = USBDEVICE_EVENT_STATUS_INVALID;

        TEST_LOG("Waiting for event: 0x%08X (timeout: %u ms)", expected_status, timeout_ms);

        while (!(expected_status & m_event_signalled)) {
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
                TEST_LOG("Timeout waiting for USB device event (expected: 0x%08X, received: 0x%08X)",
                         expected_status, m_event_signalled);
                break;
            }
        }

        signalled = m_event_signalled;
        TEST_LOG("Event wait completed with status: 0x%08X", signalled);
        return signalled;
    }

    /**
     * @brief Get the last plugged-in device information
     * 
     * @return const Exchange::IUSBDevice::USBDevice& Reference to stored device info
     */
    const Exchange::IUSBDevice::USBDevice& GetLastPluggedInDevice() const
    {
        return m_last_plugged_in_device;
    }

    /**
     * @brief Get the last plugged-out device information
     * 
     * @return const Exchange::IUSBDevice::USBDevice& Reference to stored device info
     */
    const Exchange::IUSBDevice::USBDevice& GetLastPluggedOutDevice() const
    {
        return m_last_plugged_out_device;
    }

    /**
     * @brief Check if a device plugged-in event was received
     * 
     * @return bool True if event was received
     */
    bool IsDevicePluggedInReceived() const
    {
        return m_device_plugged_in_received;
    }

    /**
     * @brief Check if a device plugged-out event was received
     * 
     * @return bool True if event was received
     */
    bool IsDevicePluggedOutReceived() const
    {
        return m_device_plugged_out_received;
    }

    /**
     * @brief Reset all event flags and stored data
     * 
     * Useful for running multiple test cases with the same handler instance.
     */
    void ResetEvents()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        TEST_LOG("Resetting all events and stored data");
        
        m_event_signalled = 0;
        m_device_plugged_in_received = false;
        m_device_plugged_out_received = false;
        
        // Clear device information
        m_last_plugged_in_device.deviceClass = 0;
        m_last_plugged_in_device.deviceSubclass = 0;
        m_last_plugged_in_device.deviceName.clear();
        m_last_plugged_in_device.devicePath.clear();
        
        m_last_plugged_out_device.deviceClass = 0;
        m_last_plugged_out_device.deviceSubclass = 0;
        m_last_plugged_out_device.deviceName.clear();
        m_last_plugged_out_device.devicePath.clear();
    }

    /**
     * @brief Get current event signalled flags
     * 
     * @return uint32_t Current event flags
     */
    uint32_t GetEventSignalled() const
    {
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

/*******************************************************************************************************************
 * L1 NOTIFICATION TESTS
 * Test notification system by directly invoking accessible methods that trigger notification events
 ********************************************************************************************************************/

/*******************************************************************************************************************
 * Test function for : OnDevicePluggedIn notification
 * OnDevicePluggedIn :
 *                Notification triggered when a USB device is plugged into the system
 *
 * Accessible Method: libUSBHotPlugCallbackDeviceAttached (static callback)
 * Trigger Path: libUSB hotplug callback → getUSBDeviceStructFromDeviceDescriptor → dispatchEvent → Dispatch → OnDevicePluggedIn
 * 
 * Use case coverage:
 *                @Success: 2
 *                @Failure: 1
 ********************************************************************************************************************/

/**
 * @brief : Test OnDevicePluggedIn notification via libUSBHotPlugCallbackDeviceAttached
 *          Simulates USB device arrival by directly invoking the libUSB hotplug callback
 *          with a mock USB device, then verifies notification is received with correct parameters
 *
 * @param[in]   :  Mock USB device (bus_number, device_address, port_number)
 * @param[out]  :  OnDevicePluggedIn notification with device information
 * @return      :  Notification received with expected device class, subclass, name, and path
 */
TEST_F(USBDeviceTest, NotificationViaLibUSBCallback_OnDevicePluggedIn_SingleMassStorageDevice_Success)
{
    TEST_LOG("Starting OnDevicePluggedIn notification test - Single Mass Storage Device");

    // Setup mock device descriptor
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetSerialNumberInUSBDevicePath();

    // Create mock USB device
    libusb_device* mock_device = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(nullptr, mock_device);
    mock_device->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mock_device->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mock_device->port_number = MOCK_USB_DEVICE_PORT_1;

    // Create and register notification handler
    USBDeviceNotificationHandler notificationHandler;
    
    if (Plugin::USBDeviceImplementation::instance() != nullptr)
    {
        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Register(&notificationHandler));
        TEST_LOG("Notification handler registered successfully");

        // Directly invoke the static hotplug callback (accessible method)
        TEST_LOG("Invoking libUSBHotPlugCallbackDeviceAttached with mock device");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(
            nullptr,  // ctx
            mock_device,
            LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
            nullptr   // user_data
        );

        // Wait for notification with timeout
        TEST_LOG("Waiting for OnDevicePluggedIn notification...");
        uint32_t result = notificationHandler.WaitForRequestStatus(5000, USBDEVICE_EVENT_DEVICE_PLUGGED_IN);

        // Verify notification was received
        EXPECT_TRUE(result & USBDEVICE_EVENT_DEVICE_PLUGGED_IN) << "OnDevicePluggedIn notification not received";
        EXPECT_TRUE(notificationHandler.IsDevicePluggedInReceived()) << "Device plugged-in flag not set";

        // Validate device parameters
        auto device = notificationHandler.GetLastPluggedInDevice();
        TEST_LOG("Validating device parameters:");
        TEST_LOG("  deviceClass: %u (expected: %u)", device.deviceClass, LIBUSB_CLASS_MASS_STORAGE);
        TEST_LOG("  deviceSubclass: %u (expected: %u)", device.deviceSubclass, LIBUSB_CLASS_MASS_STORAGE);
        TEST_LOG("  deviceName: %s", device.deviceName.c_str());
        TEST_LOG("  devicePath: %s", device.devicePath.c_str());

        EXPECT_EQ(LIBUSB_CLASS_MASS_STORAGE, device.deviceClass) << "Device class mismatch";
        EXPECT_EQ(LIBUSB_CLASS_MASS_STORAGE, device.deviceSubclass) << "Device subclass mismatch";
        EXPECT_EQ("100/001", device.deviceName) << "Device name mismatch";
        EXPECT_EQ("/dev/sda", device.devicePath) << "Device path mismatch";

        // Unregister handler
        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Unregister(&notificationHandler));
        TEST_LOG("Notification handler unregistered successfully");
    }
    else
    {
        FAIL() << "USBDeviceImplementation instance is null";
    }

    free(mock_device);
    TEST_LOG("OnDevicePluggedIn notification test completed successfully");
}

/**
 * @brief : Test OnDevicePluggedIn notification with multiple USB devices
 *          Tests notification system handles multiple sequential device arrivals correctly
 *
 * @param[in]   :  Two mock USB devices with different parameters
 * @param[out]  :  OnDevicePluggedIn notifications for both devices
 * @return      :  Both notifications received with correct device-specific parameters
 */
TEST_F(USBDeviceTest, NotificationViaLibUSBCallback_OnDevicePluggedIn_MultipleDevices_Success)
{
    TEST_LOG("Starting OnDevicePluggedIn notification test - Multiple Devices");

    // Setup mock device descriptors for both devices
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);
    Mock_SetSerialNumberInUSBDevicePath();

    // Create first mock USB device
    libusb_device* mock_device1 = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(nullptr, mock_device1);
    mock_device1->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mock_device1->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mock_device1->port_number = MOCK_USB_DEVICE_PORT_1;

    // Create second mock USB device
    libusb_device* mock_device2 = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(nullptr, mock_device2);
    mock_device2->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_2;
    mock_device2->device_address = MOCK_USB_DEVICE_ADDRESS_2;
    mock_device2->port_number = MOCK_USB_DEVICE_PORT_2;

    // Create and register notification handler
    USBDeviceNotificationHandler notificationHandler;
    
    if (Plugin::USBDeviceImplementation::instance() != nullptr)
    {
        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Register(&notificationHandler));

        // Test first device arrival
        TEST_LOG("Simulating first device arrival (100/001)");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(
            nullptr, mock_device1, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

        uint32_t result1 = notificationHandler.WaitForRequestStatus(5000, USBDEVICE_EVENT_DEVICE_PLUGGED_IN);
        EXPECT_TRUE(result1 & USBDEVICE_EVENT_DEVICE_PLUGGED_IN);

        auto device1 = notificationHandler.GetLastPluggedInDevice();
        EXPECT_EQ("100/001", device1.deviceName);
        EXPECT_EQ("/dev/sda", device1.devicePath);

        // Reset for second device
        notificationHandler.ResetEvents();

        // Test second device arrival
        TEST_LOG("Simulating second device arrival (101/002)");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(
            nullptr, mock_device2, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

        uint32_t result2 = notificationHandler.WaitForRequestStatus(5000, USBDEVICE_EVENT_DEVICE_PLUGGED_IN);
        EXPECT_TRUE(result2 & USBDEVICE_EVENT_DEVICE_PLUGGED_IN);

        auto device2 = notificationHandler.GetLastPluggedInDevice();
        EXPECT_EQ("101/002", device2.deviceName);
        EXPECT_EQ("/dev/sdb", device2.devicePath);

        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Unregister(&notificationHandler));
    }
    else
    {
        FAIL() << "USBDeviceImplementation instance is null";
    }

    free(mock_device1);
    free(mock_device2);
    TEST_LOG("Multiple device OnDevicePluggedIn notification test completed");
}

/**
 * @brief : Test OnDevicePluggedIn notification with invalid device descriptor
 *          Verifies notification is not sent when device descriptor retrieval fails
 *
 * @param[in]   :  Mock USB device with invalid descriptor setup
 * @param[out]  :  No notification (timeout expected)
 * @return      :  Timeout occurs, no notification received
 */
TEST_F(USBDeviceTest, NotificationViaLibUSBCallback_OnDevicePluggedIn_InvalidDescriptor_NoNotification)
{
    TEST_LOG("Starting OnDevicePluggedIn notification test - Invalid Descriptor");

    // Create mock device but DO NOT setup valid descriptor
    libusb_device* mock_device = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(nullptr, mock_device);
    mock_device->bus_number = 255;  // Invalid bus number
    mock_device->device_address = 255;  // Invalid address
    mock_device->port_number = 0;

    // Mock descriptor call to fail
    ON_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(LIBUSB_ERROR_NOT_FOUND));

    USBDeviceNotificationHandler notificationHandler;
    
    if (Plugin::USBDeviceImplementation::instance() != nullptr)
    {
        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Register(&notificationHandler));

        TEST_LOG("Invoking callback with invalid device descriptor");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(
            nullptr, mock_device, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

        // Wait for notification - should timeout
        TEST_LOG("Waiting for notification (expecting timeout)...");
        uint32_t result = notificationHandler.WaitForRequestStatus(2000, USBDEVICE_EVENT_DEVICE_PLUGGED_IN);

        // Verify NO notification was received
        EXPECT_FALSE(result & USBDEVICE_EVENT_DEVICE_PLUGGED_IN) << "Unexpected notification received for invalid device";
        EXPECT_FALSE(notificationHandler.IsDevicePluggedInReceived()) << "Device plugged-in flag should not be set";

        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Unregister(&notificationHandler));
    }

    free(mock_device);
    TEST_LOG("Invalid descriptor notification test completed");
}

/*******************************************************************************************************************
 * Test function for : OnDevicePluggedOut notification
 * OnDevicePluggedOut :
 *                Notification triggered when a USB device is unplugged from the system
 *
 * Accessible Method: libUSBHotPlugCallbackDeviceDetached (static callback)
 * Trigger Path: libUSB hotplug callback → getUSBDeviceStructFromDeviceDescriptor → dispatchEvent → Dispatch → OnDevicePluggedOut
 * 
 * Use case coverage:
 *                @Success: 2
 *                @Failure: 1
 ********************************************************************************************************************/

/**
 * @brief : Test OnDevicePluggedOut notification via libUSBHotPlugCallbackDeviceDetached
 *          Simulates USB device removal by directly invoking the libUSB hotplug callback
 *          with a mock USB device, then verifies notification is received with correct parameters
 *
 * @param[in]   :  Mock USB device being removed
 * @param[out]  :  OnDevicePluggedOut notification with device information
 * @return      :  Notification received with expected device details
 */
TEST_F(USBDeviceTest, NotificationViaLibUSBCallback_OnDevicePluggedOut_SingleDevice_Success)
{
    TEST_LOG("Starting OnDevicePluggedOut notification test - Single Device");

    // Setup mock device descriptor
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetSerialNumberInUSBDevicePath();

    // Create mock USB device
    libusb_device* mock_device = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(nullptr, mock_device);
    mock_device->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mock_device->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mock_device->port_number = MOCK_USB_DEVICE_PORT_1;

    // Create and register notification handler
    USBDeviceNotificationHandler notificationHandler;
    
    if (Plugin::USBDeviceImplementation::instance() != nullptr)
    {
        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Register(&notificationHandler));
        TEST_LOG("Notification handler registered successfully");

        // Directly invoke the static hotplug callback for device removal
        TEST_LOG("Invoking libUSBHotPlugCallbackDeviceDetached with mock device");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(
            nullptr,  // ctx
            mock_device,
            LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
            nullptr   // user_data
        );

        // Wait for notification with timeout
        TEST_LOG("Waiting for OnDevicePluggedOut notification...");
        uint32_t result = notificationHandler.WaitForRequestStatus(5000, USBDEVICE_EVENT_DEVICE_PLUGGED_OUT);

        // Verify notification was received
        EXPECT_TRUE(result & USBDEVICE_EVENT_DEVICE_PLUGGED_OUT) << "OnDevicePluggedOut notification not received";
        EXPECT_TRUE(notificationHandler.IsDevicePluggedOutReceived()) << "Device plugged-out flag not set";

        // Validate device parameters
        auto device = notificationHandler.GetLastPluggedOutDevice();
        TEST_LOG("Validating device parameters:");
        TEST_LOG("  deviceClass: %u", device.deviceClass);
        TEST_LOG("  deviceSubclass: %u", device.deviceSubclass);
        TEST_LOG("  deviceName: %s", device.deviceName.c_str());
        TEST_LOG("  devicePath: %s", device.devicePath.c_str());

        EXPECT_EQ(LIBUSB_CLASS_MASS_STORAGE, device.deviceClass) << "Device class mismatch";
        EXPECT_EQ(LIBUSB_CLASS_MASS_STORAGE, device.deviceSubclass) << "Device subclass mismatch";
        EXPECT_EQ("100/001", device.deviceName) << "Device name mismatch";

        // Unregister handler
        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Unregister(&notificationHandler));
        TEST_LOG("Notification handler unregistered successfully");
    }
    else
    {
        FAIL() << "USBDeviceImplementation instance is null";
    }

    free(mock_device);
    TEST_LOG("OnDevicePluggedOut notification test completed successfully");
}

/**
 * @brief : Test OnDevicePluggedOut notification for multiple devices
 *          Tests that multiple device removals are handled correctly
 *
 * @param[in]   :  Two mock USB devices being removed sequentially
 * @param[out]  :  OnDevicePluggedOut notifications for each device
 * @return      :  Both removal notifications received with correct parameters
 */
TEST_F(USBDeviceTest, NotificationViaLibUSBCallback_OnDevicePluggedOut_MultipleDevices_Success)
{
    TEST_LOG("Starting OnDevicePluggedOut notification test - Multiple Devices");

    // Setup mock device descriptors
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);
    Mock_SetSerialNumberInUSBDevicePath();

    // Create mock USB devices
    libusb_device* mock_device1 = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(nullptr, mock_device1);
    mock_device1->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mock_device1->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mock_device1->port_number = MOCK_USB_DEVICE_PORT_1;

    libusb_device* mock_device2 = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(nullptr, mock_device2);
    mock_device2->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_2;
    mock_device2->device_address = MOCK_USB_DEVICE_ADDRESS_2;
    mock_device2->port_number = MOCK_USB_DEVICE_PORT_2;

    USBDeviceNotificationHandler notificationHandler;
    
    if (Plugin::USBDeviceImplementation::instance() != nullptr)
    {
        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Register(&notificationHandler));

        // Test first device removal
        TEST_LOG("Simulating first device removal (100/001)");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(
            nullptr, mock_device1, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, nullptr);

        uint32_t result1 = notificationHandler.WaitForRequestStatus(5000, USBDEVICE_EVENT_DEVICE_PLUGGED_OUT);
        EXPECT_TRUE(result1 & USBDEVICE_EVENT_DEVICE_PLUGGED_OUT);

        auto device1 = notificationHandler.GetLastPluggedOutDevice();
        EXPECT_EQ("100/001", device1.deviceName);

        // Reset for second device
        notificationHandler.ResetEvents();

        // Test second device removal
        TEST_LOG("Simulating second device removal (101/002)");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(
            nullptr, mock_device2, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, nullptr);

        uint32_t result2 = notificationHandler.WaitForRequestStatus(5000, USBDEVICE_EVENT_DEVICE_PLUGGED_OUT);
        EXPECT_TRUE(result2 & USBDEVICE_EVENT_DEVICE_PLUGGED_OUT);

        auto device2 = notificationHandler.GetLastPluggedOutDevice();
        EXPECT_EQ("101/002", device2.deviceName);

        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Unregister(&notificationHandler));
    }
    else
    {
        FAIL() << "USBDeviceImplementation instance is null";
    }

    free(mock_device1);
    free(mock_device2);
    TEST_LOG("Multiple device OnDevicePluggedOut notification test completed");
}

/**
 * @brief : Test OnDevicePluggedOut notification with invalid device
 *          Verifies notification is not sent when device descriptor is invalid
 *
 * @param[in]   :  Mock USB device with invalid descriptor
 * @param[out]  :  No notification (timeout expected)
 * @return      :  Timeout occurs, no notification received
 */
TEST_F(USBDeviceTest, NotificationViaLibUSBCallback_OnDevicePluggedOut_InvalidDevice_NoNotification)
{
    TEST_LOG("Starting OnDevicePluggedOut notification test - Invalid Device");

    // Create mock device with invalid configuration
    libusb_device* mock_device = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(nullptr, mock_device);
    mock_device->bus_number = 255;
    mock_device->device_address = 255;
    mock_device->port_number = 0;

    // Mock descriptor call to fail
    ON_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(LIBUSB_ERROR_NO_DEVICE));

    USBDeviceNotificationHandler notificationHandler;
    
    if (Plugin::USBDeviceImplementation::instance() != nullptr)
    {
        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Register(&notificationHandler));

        TEST_LOG("Invoking callback with invalid device");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(
            nullptr, mock_device, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, nullptr);

        // Wait for notification - should timeout
        TEST_LOG("Waiting for notification (expecting timeout)...");
        uint32_t result = notificationHandler.WaitForRequestStatus(2000, USBDEVICE_EVENT_DEVICE_PLUGGED_OUT);

        // Verify NO notification was received
        EXPECT_FALSE(result & USBDEVICE_EVENT_DEVICE_PLUGGED_OUT) << "Unexpected notification for invalid device";
        EXPECT_FALSE(notificationHandler.IsDevicePluggedOutReceived()) << "Device plugged-out flag should not be set";

        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Unregister(&notificationHandler));
    }

    free(mock_device);
    TEST_LOG("Invalid device OnDevicePluggedOut notification test completed");
}

/*******************************************************************************************************************
 * Test function for : Combined notification flow
 * Combined Tests :
 *                Tests realistic USB device lifecycle - arrival followed by removal
 *
 * Accessible Methods: Both libUSBHotPlugCallbackDeviceAttached and libUSBHotPlugCallbackDeviceDetached
 * Trigger Path: Device arrival → OnDevicePluggedIn → Device removal → OnDevicePluggedOut
 * 
 * Use case coverage:
 *                @Success: 1
 ********************************************************************************************************************/

/**
 * @brief : Test complete USB device lifecycle with both arrival and removal notifications
 *          Simulates realistic scenario where a device is plugged in, then removed
 *
 * @param[in]   :  Mock USB device for arrival and removal events
 * @param[out]  :  OnDevicePluggedIn followed by OnDevicePluggedOut notifications
 * @return      :  Both notifications received in correct sequence with matching device data
 */
TEST_F(USBDeviceTest, NotificationViaLibUSBCallback_DeviceLifecycle_PlugInThenPlugOut_Success)
{
    TEST_LOG("Starting USB device lifecycle notification test");

    // Setup mock device descriptor
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetSerialNumberInUSBDevicePath();

    // Create mock USB device
    libusb_device* mock_device = (libusb_device*)malloc(sizeof(libusb_device));
    ASSERT_NE(nullptr, mock_device);
    mock_device->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    mock_device->device_address = MOCK_USB_DEVICE_ADDRESS_1;
    mock_device->port_number = MOCK_USB_DEVICE_PORT_1;

    USBDeviceNotificationHandler notificationHandler;
    
    if (Plugin::USBDeviceImplementation::instance() != nullptr)
    {
        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Register(&notificationHandler));

        // Step 1: Device arrival
        TEST_LOG("Step 1: Simulating device plug-in");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(
            nullptr, mock_device, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

        uint32_t result1 = notificationHandler.WaitForRequestStatus(5000, USBDEVICE_EVENT_DEVICE_PLUGGED_IN);
        EXPECT_TRUE(result1 & USBDEVICE_EVENT_DEVICE_PLUGGED_IN) << "Plug-in notification not received";

        auto deviceIn = notificationHandler.GetLastPluggedInDevice();
        EXPECT_EQ("100/001", deviceIn.deviceName);
        EXPECT_EQ(LIBUSB_CLASS_MASS_STORAGE, deviceIn.deviceClass);

        // Step 2: Device removal
        TEST_LOG("Step 2: Simulating device plug-out");
        Plugin::USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(
            nullptr, mock_device, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, nullptr);

        uint32_t result2 = notificationHandler.WaitForRequestStatus(5000, USBDEVICE_EVENT_DEVICE_PLUGGED_OUT);
        EXPECT_TRUE(result2 & USBDEVICE_EVENT_DEVICE_PLUGGED_OUT) << "Plug-out notification not received";

        auto deviceOut = notificationHandler.GetLastPluggedOutDevice();
        EXPECT_EQ("100/001", deviceOut.deviceName);
        EXPECT_EQ(LIBUSB_CLASS_MASS_STORAGE, deviceOut.deviceClass);

        // Verify both events were received
        EXPECT_TRUE(notificationHandler.IsDevicePluggedInReceived());
        EXPECT_TRUE(notificationHandler.IsDevicePluggedOutReceived());

        // Verify device parameters match for both notifications
        EXPECT_EQ(deviceIn.deviceName, deviceOut.deviceName) << "Device names should match";
        EXPECT_EQ(deviceIn.deviceClass, deviceOut.deviceClass) << "Device classes should match";

        EXPECT_EQ(Core::ERROR_NONE, Plugin::USBDeviceImplementation::instance()->Unregister(&notificationHandler));
    }
    else
    {
        FAIL() << "USBDeviceImplementation instance is null";
    }

    free(mock_device);
    TEST_LOG("Device lifecycle notification test completed successfully");
}
