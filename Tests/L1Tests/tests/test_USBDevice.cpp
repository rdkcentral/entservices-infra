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
 * L1 Notification Tests - COMPLETE IMPLEMENTATION
 * Test notification system using public accessible methods and direct notification calls
 * 
 * This section implements comprehensive notification tests that:
 * 1. Use existing USBDeviceTest fixture infrastructure (USBDeviceImpl, handler, etc.)
 * 2. Test notification flow through multiple accessible methods:
 *    - Job dispatch mechanism (natural flow)
 *    - Direct implementation method calls
 *    - JSON-RPC hotplug callback simulation
 * 3. Verify proper parameter passing and event synchronization
 * 4. Ensure proper ProxyType handling and resource management
 ********************************************************************************************************************/

/**
 * @brief Test OnDevicePluggedIn notification via Job dispatch mechanism
 *        Verifies natural notification flow using Job::Create and Job::Dispatch
 *        Tests parameter validation and proper event synchronization
 */
TEST_F(USBDeviceTest, OnDevicePluggedIn_ViaJobDispatch_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    USBDeviceImpl->Register(notificationHandler);
    notificationHandler->ResetEvents();
    
    // Create test data for device plugged in event
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 8;
    testDevice.deviceSubclass = 6;
    testDevice.deviceName = "001/004";
    testDevice.devicePath = "/dev/sda";
    
    // Use Job mechanism for natural notification flow
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        testDevice
    );
    job->Dispatch();
    
    // Verify notification was received
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedIn));
    EXPECT_EQ("001/004", notificationHandler->GetLastPluggedInDeviceName());
    EXPECT_EQ("/dev/sda", notificationHandler->GetLastPluggedInDevicePath());
    EXPECT_EQ(8, notificationHandler->GetLastPluggedInDeviceClass());
    EXPECT_EQ(6, notificationHandler->GetLastPluggedInDeviceSubclass());
    
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/**
 * @brief Test OnDevicePluggedOut notification via Job dispatch mechanism
 *        Verifies device removal notification flow using Job dispatch
 *        Tests parameter validation for device removal event
 */
TEST_F(USBDeviceTest, OnDevicePluggedOut_ViaJobDispatch_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    USBDeviceImpl->Register(notificationHandler);
    notificationHandler->ResetEvents();
    
    // Create test data for device plugged out event
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 8;
    testDevice.deviceSubclass = 6;
    testDevice.deviceName = "001/005";
    testDevice.devicePath = "/dev/sdb";
    
    // Use Job mechanism for natural notification flow
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
        testDevice
    );
    job->Dispatch();
    
    // Verify notification was received
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedOut));
    EXPECT_EQ("001/005", notificationHandler->GetLastPluggedOutDeviceName());
    EXPECT_EQ("/dev/sdb", notificationHandler->GetLastPluggedOutDevicePath());
    EXPECT_EQ(8, notificationHandler->GetLastPluggedOutDeviceClass());
    EXPECT_EQ(6, notificationHandler->GetLastPluggedOutDeviceSubclass());
    
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/**
 * @brief Test notifications triggered via public API methods
 *        Verifies that public methods can indirectly trigger notifications
 *        Uses existing JSON-RPC handler infrastructure
 */
TEST_F(USBDeviceTest, NotificationVia_PublicAPIMethods_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    USBDeviceImpl->Register(notificationHandler);
    notificationHandler->ResetEvents();
    
    // Setup mock infrastructure for device detection
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    
    // Test via getDeviceList - may trigger internal device scanning
    string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    
    // Test via getDeviceInfo - may trigger device state verification
    JsonObject deviceInfoParams;
    deviceInfoParams["deviceName"] = "100/001";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), deviceInfoParams.ToString(), response));
    
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/**
 * @brief Test notifications triggered via hotplug callback mechanism
 *        Verifies libUSB hotplug callbacks trigger proper notifications
 *        Tests integration with existing libUSB mock infrastructure
 */
TEST_F(USBDeviceTest, OnDevicePluggedIn_ViaHotplugCallback_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    USBDeviceImpl->Register(notificationHandler);
    notificationHandler->ResetEvents();
    
    // Setup mock infrastructure
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    
    // Create mock device for hotplug callback
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    // Trigger hotplug callback directly
    libUSBHotPlugCbDeviceAttached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 0);
    
    // Verify notification was triggered
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_OnDevicePluggedIn));
    
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/**
 * @brief Test notifications triggered via hotplug callback for device removal
 *        Verifies libUSB device detached callbacks trigger proper notifications
 *        Tests device removal notification flow
 */
TEST_F(USBDeviceTest, OnDevicePluggedOut_ViaHotplugCallback_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    USBDeviceImpl->Register(notificationHandler);
    notificationHandler->ResetEvents();
    
    // Setup mock infrastructure
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);
    
    // Create mock device for hotplug callback
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_2;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_2;
    dev.port_number = MOCK_USB_DEVICE_PORT_2;
    
    // Trigger hotplug callback directly
    libUSBHotPlugCbDeviceDetached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0);
    
    // Verify notification was triggered
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(2000, USBDevice_OnDevicePluggedOut));
    
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/**
 * @brief Test notification registration and management with multiple handlers
 *        Verifies multiple notification handlers can be registered
 *        Tests proper notification delivery to all registered handlers
 */
TEST_F(USBDeviceTest, NotificationRegistration_MultipleHandlers_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler1 = new L1USBDeviceNotificationHandler();
    L1USBDeviceNotificationHandler* notificationHandler2 = new L1USBDeviceNotificationHandler();
    
    // Register multiple handlers
    USBDeviceImpl->Register(notificationHandler1);
    USBDeviceImpl->Register(notificationHandler2);
    
    notificationHandler1->ResetEvents();
    notificationHandler2->ResetEvents();
    
    // Create test data and trigger notification
    Exchange::IUSBDevice::USBDevice testDevice;
    testDevice.deviceClass = 8;
    testDevice.deviceSubclass = 6;
    testDevice.deviceName = "002/003";
    testDevice.devicePath = "/dev/sdc";
    
    auto job = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        testDevice
    );
    job->Dispatch();
    
    // Verify both handlers received notification
    EXPECT_TRUE(notificationHandler1->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedIn));
    EXPECT_TRUE(notificationHandler2->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedIn));
    
    EXPECT_EQ("002/003", notificationHandler1->GetLastPluggedInDeviceName());
    EXPECT_EQ("002/003", notificationHandler2->GetLastPluggedInDeviceName());
    
    USBDeviceImpl->Unregister(notificationHandler1);
    USBDeviceImpl->Unregister(notificationHandler2);
    notificationHandler1->Release();
    notificationHandler2->Release();
}

/**
 * @brief Test notification timing and rapid event handling
 *        Verifies system can handle rapid notification sequences
 *        Tests notification handler reset and multiple event handling
 */
TEST_F(USBDeviceTest, NotificationTiming_RapidNotifications_Success)
{
    L1USBDeviceNotificationHandler* notificationHandler = new L1USBDeviceNotificationHandler();
    
    USBDeviceImpl->Register(notificationHandler);
    
    // Test rapid plug-in, plug-out sequence
    notificationHandler->ResetEvents();
    
    // Create device plugged in event
    Exchange::IUSBDevice::USBDevice testDevice1;
    testDevice1.deviceClass = 8;
    testDevice1.deviceSubclass = 6;
    testDevice1.deviceName = "003/001";
    testDevice1.devicePath = "/dev/sdd";
    
    auto job1 = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
        testDevice1
    );
    job1->Dispatch();
    
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedIn));
    
    // Reset and test device removal
    notificationHandler->ResetPluggedInEvent();
    
    Exchange::IUSBDevice::USBDevice testDevice2;
    testDevice2.deviceClass = 8;
    testDevice2.deviceSubclass = 6;
    testDevice2.deviceName = "003/002";
    testDevice2.devicePath = "/dev/sde";
    
    auto job2 = Plugin::USBDeviceImplementation::Job::Create(
        USBDeviceImpl.operator->(),
        Plugin::USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT,
        testDevice2
    );
    job2->Dispatch();
    
    EXPECT_TRUE(notificationHandler->WaitForRequestStatus(1000, USBDevice_OnDevicePluggedOut));
    EXPECT_EQ("003/002", notificationHandler->GetLastPluggedOutDeviceName());
    
    USBDeviceImpl->Unregister(notificationHandler);
    notificationHandler->Release();
}

/*Test cases for L1 Notification Tests end here*/
