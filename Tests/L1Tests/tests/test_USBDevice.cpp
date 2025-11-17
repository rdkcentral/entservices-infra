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
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "USBDevice.h"
#include "USBDeviceImplementation.h"
#include "libUSBMock.h"
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include <fstream> // Added for file creation
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

typedef enum : uint32_t {
    USBDevice_StateInvalid = 0x00000000,
    USBDevice_OnDevicePluggedIn = 0x00000001,
    USBDevice_OnDevicePluggedOut = 0x00000002
} USBDeviceEventType_t;

class L1USBDeviceNotificationHandler : public Exchange::IUSBDevice::INotification {
    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_condition_variable;
        uint32_t m_event_signalled;
        
        // Parameter storage for validation
        Exchange::IUSBDevice::USBDevice m_pluggedInDevice;
        Exchange::IUSBDevice::USBDevice m_pluggedOutDevice;
        bool m_pluggedInDeviceReceived;
        bool m_pluggedOutDeviceReceived;
        
        mutable Core::CriticalSection m_refCountLock;
        mutable uint32_t m_refCount;

        BEGIN_INTERFACE_MAP(L1USBDeviceNotificationHandler)
        INTERFACE_ENTRY(Exchange::IUSBDevice::INotification)
        END_INTERFACE_MAP

    public:
        L1USBDeviceNotificationHandler() 
            : m_event_signalled(USBDevice_StateInvalid)
            , m_pluggedInDeviceReceived(false)
            , m_pluggedOutDeviceReceived(false)
            , m_refCount(1) {
            // Initialize devices using value initialization
            m_pluggedInDevice = Exchange::IUSBDevice::USBDevice();
            m_pluggedOutDevice = Exchange::IUSBDevice::USBDevice();
        }
        
        ~L1USBDeviceNotificationHandler() = default;

        // IReferenceCounted interface implementation
        void AddRef() const override {
            m_refCountLock.Lock();
            ++m_refCount;
            m_refCountLock.Unlock();
        }

        uint32_t Release() const override {
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

        // IUSBDevice::INotification interface implementation
        void OnDevicePluggedIn(const Exchange::IUSBDevice::USBDevice &device) override {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_pluggedInDevice = device;
            m_pluggedInDeviceReceived = true;
            m_event_signalled |= USBDevice_OnDevicePluggedIn;
            m_condition_variable.notify_one();
        }

        void OnDevicePluggedOut(const Exchange::IUSBDevice::USBDevice &device) override {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_pluggedOutDevice = device;
            m_pluggedOutDeviceReceived = true;
            m_event_signalled |= USBDevice_OnDevicePluggedOut;
            m_condition_variable.notify_one();
        }

        // Wait for specific notification events with timeout
        bool WaitForRequestStatus(uint32_t timeout_ms, USBDeviceEventType_t expected_status) {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            auto timeout = std::chrono::milliseconds(timeout_ms);
            
            while (!(expected_status & m_event_signalled)) {
                if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
                    TEST_LOG("Timeout waiting for USB device notification event: expected=0x%x, received=0x%x", 
                             expected_status, m_event_signalled);
                    return false;
                }
            }
            return true;
        }

        // Getter methods for parameter validation
        const Exchange::IUSBDevice::USBDevice& GetPluggedInDevice() const {
            return m_pluggedInDevice;
        }

        const Exchange::IUSBDevice::USBDevice& GetPluggedOutDevice() const {
            return m_pluggedOutDevice;
        }

        bool IsPluggedInDeviceReceived() const {
            return m_pluggedInDeviceReceived;
        }

        bool IsPluggedOutDeviceReceived() const {
            return m_pluggedOutDeviceReceived;
        }

        uint32_t GetSignalledEvents() const {
            return m_event_signalled;
        }

        // Getter methods for last received notification parameters
        const Exchange::IUSBDevice::USBDevice& GetLastPluggedInDevice() const {
            return m_pluggedInDevice;
        }

        const Exchange::IUSBDevice::USBDevice& GetLastPluggedOutDevice() const {
            return m_pluggedOutDevice;
        }

        string GetLastPluggedInDeviceName() const {
            return m_pluggedInDevice.deviceName;
        }

        string GetLastPluggedInDevicePath() const {
            return m_pluggedInDevice.devicePath;
        }

        uint8_t GetLastPluggedInDeviceClass() const {
            return m_pluggedInDevice.deviceClass;
        }

        uint8_t GetLastPluggedInDeviceSubclass() const {
            return m_pluggedInDevice.deviceSubclass;
        }

        string GetLastPluggedOutDeviceName() const {
            return m_pluggedOutDevice.deviceName;
        }

        string GetLastPluggedOutDevicePath() const {
            return m_pluggedOutDevice.devicePath;
        }

        uint8_t GetLastPluggedOutDeviceClass() const {
            return m_pluggedOutDevice.deviceClass;
        }

        uint8_t GetLastPluggedOutDeviceSubclass() const {
            return m_pluggedOutDevice.deviceSubclass;
        }

        // Reset methods for reusing handler
        void ResetEvents() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_event_signalled = USBDevice_StateInvalid;
            m_pluggedInDeviceReceived = false;
            m_pluggedOutDeviceReceived = false;
            m_pluggedInDevice = Exchange::IUSBDevice::USBDevice();
            m_pluggedOutDevice = Exchange::IUSBDevice::USBDevice();
        }

        void ResetPluggedInEvent() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_event_signalled &= ~USBDevice_OnDevicePluggedIn;
            m_pluggedInDeviceReceived = false;
            m_pluggedInDevice = Exchange::IUSBDevice::USBDevice();
        }

        void ResetPluggedOutEvent() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_event_signalled &= ~USBDevice_OnDevicePluggedOut;
            m_pluggedOutDeviceReceived = false;
            m_pluggedOutDevice = Exchange::IUSBDevice::USBDevice();
        }
};


namespace {
const string callSign = _T("USBDevice");
}

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

        /* Set all the asynchronouse event handler with libusb to handle various events*/
        ON_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](libusb_context *ctx, int events, int flags, int vendor_id, int product_id, int dev_class,
                 libusb_hotplug_callback_fn cb_fn, void *user_data, libusb_hotplug_callback_handle *callback_handle) {
                if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == events) {
                    libUSBHotPlugCbDeviceAttached = cb_fn;
                    *callback_handle = 1;
                }
                if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == events) {
                    libUSBHotPlugCbDeviceDetached = cb_fn;
                    *callback_handle = 2;
                }
                return LIBUSB_SUCCESS;
            }));

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

    virtual void SetUp()
    {
        ASSERT_TRUE(libUSBHotPlugCbDeviceAttached != nullptr);
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
*Test function for Event:onDevicePluggedIn
*Event : onDevicePluggedIn
*             Triggered when a USB drive is plugged in
*
*                @return (i) Exchange::IUSBDevice::USBDevice structure
* Use case coverage:
*                @Success :1
********************************************************************************************************************/

/**
 * @brief : onDevicePluggedIn when a USB drive is connected
 *          Check onDevicePluggedIn triggered successfully when a USB drive is connected
 *          with USBDevice structure value
 * @param[in] : This method takes no parameters.
 * @return : \"params\":{"deviceclass":8,"devicesubclass":6,"devicename":"002\/002","devicepath":"\/dev\/sda"}
 *
 */
TEST_F(USBDeviceTest, OnDevicePluggedInSuccess)
{
    Core::Event onDevicePluggedIn(false, true);

    Mock_SetSerialNumberInUSBDevicePath();

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                TEST_LOG("json to string!");
                EXPECT_TRUE(json->ToString(text));
                EXPECT_EQ(text, "{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.USBDevice.onDevicePluggedIn\",\"params\":{\"device\":{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"100\\/001\",\"devicePath\":\"\\/dev\\/sda\"}}}");
                onDevicePluggedIn.SetEvent();

                return Core::ERROR_NONE;
            }));

    /* HotPlug Attach Device 1 Verification */
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    EVENT_SUBSCRIBE(0, _T("onDevicePluggedIn"), _T("org.rdk.USBDevice"), message);

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    libUSBHotPlugCbDeviceAttached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 0);
    TEST_LOG("After libUSBHotPlugCbDeviceAttached");

    EXPECT_EQ(Core::ERROR_NONE, onDevicePluggedIn.Lock());
    TEST_LOG("After EVENT_UNSUBSCRIBE");

    EVENT_UNSUBSCRIBE(0, _T("onDevicePluggedIn"), _T("org.rdk.USBDevice"), message);
 }
/*Test cases for onDevicePluggedIn ends here*/

/*******************************************************************************************************************
*Test function for Event:onDevicePluggedOut
*Event : onDevicePluggedOut
*             Triggered when a USB drive is plugged out
*
*                @return (i) Exchange::IUSBDevice::USBDevice structure
* Use case coverage:
*                @Success :1
********************************************************************************************************************/

/**
 * @brief : onDevicePluggedOut when a USB drive is connected
 *          Check onDevicePluggedOut triggered successfully when a USB drive is disconnected
 *          with USBDevice structure value
 * @param[in] : This method takes no parameters.
 * @return : \"params\":{"deviceclass":8,"devicesubclass":6,"devicename":"002\/002","devicepath":"\/dev\/sda"}
 *
 */
TEST_F(USBDeviceTest, onDevicePluggedOutSuccess)
{
    Core::Event onDevicePluggedOut(false, true);

    Mock_SetSerialNumberInUSBDevicePath();

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_EQ(text, "{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.USBDevice.onDevicePluggedOut\",\"params\":{\"device\":{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"100\\/001\",\"devicePath\":\"\\/dev\\/sda\"}}}");

                onDevicePluggedOut.SetEvent();

                return Core::ERROR_NONE;
            }));

    /* HotPlug Attach Device 1 Verification */
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    EVENT_SUBSCRIBE(0, _T("onDevicePluggedOut"), _T("org.rdk.USBDevice"), message);

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    libUSBHotPlugCbDeviceDetached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0);

    EXPECT_EQ(Core::ERROR_NONE, onDevicePluggedOut.Lock());
    EVENT_UNSUBSCRIBE(0, _T("onDevicePluggedOut"), _T("org.rdk.USBDevice"), message);
 }
/*Test cases for onDevicePluggedOut ends here*/

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

/*Test cases for getDeviceInfo ends here*/

/*******************************************************************************************************************
 * L1 Notification Tests - CORRECTED VERSION
 * Test notification system using public accessible methods and direct notification calls
 ********************************************************************************************************************/

/*Test cases for L1 Notifications end here*/

/*******************************************************************************************************************
 * L1 Notification Tests - CORRECTED VERSION 
 * Test notification system using public Dispatch() method through Job mechanism
 ********************************************************************************************************************/

/**
 * @brief Test OnDevicePluggedIn notification via Job::Dispatch mechanism
 *        Using the public Job::Create and Job::Dispatch methods to trigger notifications naturally
 * @param[in] : None
 * @return : Notification should be triggered with correct device parameters
 */
TEST_F(USBDeviceTest, L1Notification_OnDevicePluggedIn_ViaJobDispatch_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    if (USBDeviceImpl.IsValid() && USBDeviceImpl.operator->() != nullptr)
    {
        // Register our notification handler with the USBDevice implementation
        USBDeviceImpl->Register(notificationHandler);
        
        // Create test USB device structure
        Exchange::IUSBDevice::USBDevice testDevice = {0};
        testDevice.deviceClass = 8;           // Mass storage class
        testDevice.deviceSubclass = 6;        // SCSI subclass
        testDevice.deviceName = "100/001";
        testDevice.devicePath = "/dev/sda";
        
        // Use the public Job::Create mechanism to create a job and dispatch it
        auto job = Plugin::USBDeviceImplementation::Job::Create(
            USBDeviceImpl.operator->(),
            Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
            testDevice
        );
        
        // Execute the job's Dispatch method - this is the natural flow
        job->Dispatch();
        
        // Wait for notification with timeout (since it goes through worker pool)
        EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_OnDevicePluggedIn));
        
        // Validate notification parameters
        EXPECT_EQ(8, notificationHandler->GetLastPluggedInDeviceClass());
        EXPECT_EQ(6, notificationHandler->GetLastPluggedInDeviceSubclass());
        EXPECT_EQ("100/001", notificationHandler->GetLastPluggedInDeviceName());
        EXPECT_EQ("/dev/sda", notificationHandler->GetLastPluggedInDevicePath());
        
        USBDeviceImpl->Unregister(notificationHandler);
    }
    
    notificationHandler->Release();
}

/**
 * @brief Test OnDevicePluggedOut notification via Job::Dispatch mechanism  
 *        Using the public Job::Create and Job::Dispatch methods to trigger notifications naturally
 * @param[in] : None
 * @return : Notification should be triggered with correct device parameters
 */
TEST_F(USBDeviceTest, L1Notification_OnDevicePluggedOut_ViaJobDispatch_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    if (USBDeviceImpl.IsValid() && USBDeviceImpl.operator->() != nullptr)
    {
        // Register our notification handler with the USBDevice implementation
        USBDeviceImpl->Register(notificationHandler);
        
        // Create test USB device structure
        Exchange::IUSBDevice::USBDevice testDevice = {0};
        testDevice.deviceClass = 8;           // Mass storage class
        testDevice.deviceSubclass = 6;        // SCSI subclass  
        testDevice.deviceName = "101/002";
        testDevice.devicePath = "/dev/sdb";
        
        // Use the public Job::Create mechanism to create a job and dispatch it
        auto job = Plugin::USBDeviceImplementation::Job::Create(
            USBDeviceImpl.operator->(),
            Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
            testDevice
        );
        
        // Execute the job's Dispatch method - this is the natural flow
        job->Dispatch();
        
        // Wait for notification with timeout (since it goes through worker pool)
        EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_OnDevicePluggedOut));
        
        // Validate notification parameters
        EXPECT_EQ(8, notificationHandler->GetLastPluggedOutDeviceClass());
        EXPECT_EQ(6, notificationHandler->GetLastPluggedOutDeviceSubclass());
        EXPECT_EQ("101/002", notificationHandler->GetLastPluggedOutDeviceName());
        EXPECT_EQ("/dev/sdb", notificationHandler->GetLastPluggedOutDevicePath());
        
        USBDeviceImpl->Unregister(notificationHandler);
    }
    
    notificationHandler->Release();
}

/**
 * @brief Test multiple notifications in sequence via Job::Dispatch mechanism
 *        Testing notification system with multiple events using natural flow
 * @param[in] : None
 * @return : Both notifications should be received in correct order
 */
TEST_F(USBDeviceTest, L1Notification_MultipleEvents_ViaJobDispatch_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    if (USBDeviceImpl.IsValid() && USBDeviceImpl.operator->() != nullptr)
    {
        USBDeviceImpl->Register(notificationHandler);
        
        // Test device 1 - plug in
        Exchange::IUSBDevice::USBDevice testDevice1 = {0};
        testDevice1.deviceClass = 8;
        testDevice1.deviceSubclass = 6;
        testDevice1.deviceName = "100/001";
        testDevice1.devicePath = "/dev/sda";
        
        // Create and dispatch plug-in event
        auto plugInJob = Plugin::USBDeviceImplementation::Job::Create(
            USBDeviceImpl.operator->(),
            Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
            testDevice1
        );
        plugInJob->Dispatch();
        
        // Wait for first notification
        EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_OnDevicePluggedIn));
        EXPECT_EQ("100/001", notificationHandler->GetLastPluggedInDeviceName());
        
        // Reset event flag for second notification
        notificationHandler->ResetPluggedInEvent();
        
        // Test device 1 - plug out
        auto plugOutJob = Plugin::USBDeviceImplementation::Job::Create(
            USBDeviceImpl.operator->(),
            Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
            testDevice1
        );
        plugOutJob->Dispatch();
        
        // Wait for second notification
        EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_OnDevicePluggedOut));
        EXPECT_EQ("100/001", notificationHandler->GetLastPluggedOutDeviceName());
        
        USBDeviceImpl->Unregister(notificationHandler);
    }
    
    notificationHandler->Release();
}

/**
 * @brief Test notification parameters validation via Job::Dispatch mechanism
 *        Verifies all device parameters are correctly passed through notifications
 * @param[in] : None
 * @return : All device parameters should match expected values
 */
TEST_F(USBDeviceTest, L1Notification_ParametersValidation_ViaJobDispatch_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    if (USBDeviceImpl.IsValid() && USBDeviceImpl.operator->() != nullptr)
    {
        USBDeviceImpl->Register(notificationHandler);
        
        // Create comprehensive test device with all fields populated
        Exchange::IUSBDevice::USBDevice testDevice = {0};
        testDevice.deviceClass = 9;           // Hub class
        testDevice.deviceSubclass = 0;        // Hub subclass
        testDevice.deviceName = "002/005";    
        testDevice.devicePath = "/dev/sdc";
        
        // Trigger notification using natural Job mechanism
        auto job = Plugin::USBDeviceImplementation::Job::Create(
            USBDeviceImpl.operator->(),
            Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
            testDevice
        );
        job->Dispatch();
        
        // Wait for notification
        EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_OnDevicePluggedIn));
        
        // Comprehensive validation of all parameters
        EXPECT_TRUE(notificationHandler->IsPluggedInDeviceReceived());
        
        const Exchange::IUSBDevice::USBDevice& receivedDevice = notificationHandler->GetLastPluggedInDevice();
        EXPECT_EQ(testDevice.deviceClass, receivedDevice.deviceClass);
        EXPECT_EQ(testDevice.deviceSubclass, receivedDevice.deviceSubclass); 
        EXPECT_EQ(testDevice.deviceName, receivedDevice.deviceName);
        EXPECT_EQ(testDevice.devicePath, receivedDevice.devicePath);
        
        // Test individual getter methods
        EXPECT_EQ(9, notificationHandler->GetLastPluggedInDeviceClass());
        EXPECT_EQ(0, notificationHandler->GetLastPluggedInDeviceSubclass());
        EXPECT_EQ("002/005", notificationHandler->GetLastPluggedInDeviceName());
        EXPECT_EQ("/dev/sdc", notificationHandler->GetLastPluggedInDevicePath());
        
        USBDeviceImpl->Unregister(notificationHandler);
    }
    
    notificationHandler->Release();
}

/*Test cases for L1 Notifications using new test fixture*/

/**
 * @brief Test USB device plug-in notification using L1 test fixture
 *        Uses Job::Create mechanism to trigger natural notification flow
 * @param[in] : None
 * @return : Notification should be received with correct device parameters
 */
TEST_F(USBDeviceTest, OnDevicePluggedIn_ValidDevice_Success)
{
    // Reset handler state
    m_handler->ResetEvents();
    
    // Create test device with typical USB device parameters
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 8;              // Mass Storage class
    testDevice.deviceSubclass = 6;           // SCSI subclass
    testDevice.deviceName = "001/004";
    testDevice.devicePath = "/dev/sdb";
    
    // Use Job mechanism to trigger notification naturally
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        m_usbDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        testDevice
    );
    job->Dispatch();
    
    // Verify notification was received
    EXPECT_TRUE(m_handler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedIn));
    EXPECT_TRUE(m_handler->IsPluggedInDeviceReceived());
    
    // Validate device parameters
    const Exchange::IUSBDevice::USBDevice& receivedDevice = m_handler->GetLastPluggedInDevice();
    EXPECT_EQ(8, receivedDevice.deviceClass);
    EXPECT_EQ(6, receivedDevice.deviceSubclass);
    EXPECT_EQ("001/004", receivedDevice.deviceName);
    EXPECT_EQ("/dev/sdb", receivedDevice.devicePath);
}

/**
 * @brief Test USB device plug-out notification using L1 test fixture
 *        Uses Job::Create mechanism to trigger natural notification flow
 * @param[in] : None
 * @return : Notification should be received with correct device parameters
 */
TEST_F(USBDeviceTest, OnDevicePluggedOut_ValidDevice_Success)
{
    // Reset handler state
    m_handler->ResetEvents();
    
    // Create test device that will be "removed"
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 3;              // HID class
    testDevice.deviceSubclass = 1;           // Boot subclass
    testDevice.deviceName = "001/002";
    testDevice.devicePath = "/dev/input/event0";
    
    // Use Job mechanism to trigger notification naturally
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        m_usbDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
        testDevice
    );
    job->Dispatch();
    
    // Verify notification was received
    EXPECT_TRUE(m_handler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedOut));
    EXPECT_TRUE(m_handler->IsPluggedOutDeviceReceived());
    
    // Validate device parameters
    const Exchange::IUSBDevice::USBDevice& receivedDevice = m_handler->GetLastPluggedOutDevice();
    EXPECT_EQ(3, receivedDevice.deviceClass);
    EXPECT_EQ(1, receivedDevice.deviceSubclass);
    EXPECT_EQ("001/002", receivedDevice.deviceName);
    EXPECT_EQ("/dev/input/event0", receivedDevice.devicePath);
}

/**
 * @brief Test multiple sequential notifications using L1 test fixture
 *        Uses Job::Create mechanism for multiple natural notification events
 * @param[in] : None
 * @return : All notifications should be received in sequence
 */
TEST_F(USBDeviceTest, MultipleNotifications_Sequential_Success)
{
    // Reset handler state
    m_handler->ResetEvents();
    
    // First device plug-in
    Exchange::IUSBDevice::USBDevice device1;
    device1.deviceClass = 9;
    device1.deviceSubclass = 0;
    device1.deviceName = "001/001";
    device1.devicePath = "/dev/hub1";
    
    auto plugInJob = Plugin::USBDeviceImplementation::Job::Create(
        m_usbDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        device1
    );
    plugInJob->Dispatch();
    
    EXPECT_TRUE(m_handler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedIn));
    EXPECT_EQ("001/001", m_handler->GetLastPluggedInDeviceName());
    
    // Reset plug-in event, keep plug-out clear
    m_handler->ResetPluggedInEvent();
    
    // Second device plug-out
    Exchange::IUSBDevice::USBDevice device2;
    device2.deviceClass = 8;
    device2.deviceSubclass = 6;
    device2.deviceName = "002/003";
    device2.devicePath = "/dev/sdc";
    
    auto plugOutJob = Plugin::USBDeviceImplementation::Job::Create(
        m_usbDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
        device2
    );
    plugOutJob->Dispatch();
    
    EXPECT_TRUE(m_handler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedOut));
    EXPECT_EQ("002/003", m_handler->GetLastPluggedOutDeviceName());
    
    // Verify both events were processed
    EXPECT_TRUE(m_handler->IsPluggedOutDeviceReceived());
}

/**
 * @brief Test notification parameter validation with comprehensive device data
 *        Uses Job::Create mechanism to validate all device fields are correctly transmitted
 * @param[in] : None
 * @return : All device parameters should match expected values
 */
TEST_F(USBDeviceTest, NotificationParameters_ComprehensiveValidation_Success)
{
    m_handler->ResetEvents();
    
    // Create device with all possible parameter combinations
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 0xFF;           // Vendor specific
    testDevice.deviceSubclass = 0xFF;        // Vendor specific
    testDevice.deviceName = "003/007";
    testDevice.devicePath = "/dev/custom_device";
    
    // Test plug-in notification using Job mechanism
    auto plugInJob = Plugin::USBDeviceImplementation::Job::Create(
        m_usbDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        testDevice
    );
    plugInJob->Dispatch();
    
    EXPECT_TRUE(m_handler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedIn));
    
    // Validate using individual getter methods
    EXPECT_EQ(0xFF, m_handler->GetLastPluggedInDeviceClass());
    EXPECT_EQ(0xFF, m_handler->GetLastPluggedInDeviceSubclass());
    EXPECT_EQ("003/007", m_handler->GetLastPluggedInDeviceName());
    EXPECT_EQ("/dev/custom_device", m_handler->GetLastPluggedInDevicePath());
    
    // Reset and test plug-out with different device
    m_handler->ResetEvents();
    
    testDevice.deviceClass = 0x00;           // Undefined class
    testDevice.deviceSubclass = 0x00;        // Undefined subclass  
    testDevice.deviceName = "004/008";
    testDevice.devicePath = "/dev/undefined";
    
    auto plugOutJob = Plugin::USBDeviceImplementation::Job::Create(
        m_usbDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
        testDevice
    );
    plugOutJob->Dispatch();
    
    EXPECT_TRUE(m_handler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedOut));
    
    EXPECT_EQ(0x00, m_handler->GetLastPluggedOutDeviceClass());
    EXPECT_EQ(0x00, m_handler->GetLastPluggedOutDeviceSubclass());
    EXPECT_EQ("004/008", m_handler->GetLastPluggedOutDeviceName());
    EXPECT_EQ("/dev/undefined", m_handler->GetLastPluggedOutDevicePath());
}

/**
 * @brief Test notification timeout behavior
 *        Validates timeout handling when no notification is received
 * @param[in] : None  
 * @return : Timeout should occur when no notification is sent
 */
TEST_F(USBDeviceTest, NotificationTimeout_NoEvent_ExpectedTimeout)
{
    m_handler->ResetEvents();
    
    // Wait for notification that will never come
    EXPECT_FALSE(m_handler->WaitForRequestStatus(500, USBDevice_OnDevicePluggedIn));
    EXPECT_FALSE(m_handler->IsPluggedInDeviceReceived());
    
    // Verify no events were signaled
    EXPECT_EQ(USBDevice_StateInvalid, m_handler->GetSignalledEvents());
}

/*Test cases for L1 Notifications end here*/
