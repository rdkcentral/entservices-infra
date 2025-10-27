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

TEST_F(USBDeviceTest, libUSBInit_Success)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_has_capability(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->libUSBInit());
}

TEST_F(USBDeviceTest, libUSBInit_AlreadyInitialized)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_has_capability(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->libUSBInit());
    EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->libUSBInit());
}

TEST_F(USBDeviceTest, libUSBInit_ThreadCreationSuccess)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_has_capability(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, USBDeviceImpl->libUSBInit());
}

TEST_F(USBDeviceTest, libUSBInit_ContextInitFailure)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_MEM));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_strerror(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return("Insufficient memory"));

    EXPECT_EQ(Core::ERROR_GENERAL, USBDeviceImpl->libUSBInit());
}

TEST_F(USBDeviceTest, libUSBInit_NoHotplugCapability)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_has_capability(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_exit(::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, USBDeviceImpl->libUSBInit());
}

TEST_F(USBDeviceTest, libUSBInit_DeviceArrivedCallbackRegistrationFailure)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_has_capability(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_MEM));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_strerror(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return("Insufficient memory"));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_exit(::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, USBDeviceImpl->libUSBInit());
}

TEST_F(USBDeviceTest, libUSBInit_DeviceLeftCallbackRegistrationFailure)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_has_capability(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_SUCCESS));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NOT_SUPPORTED));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_strerror(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return("Operation not supported"));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_exit(::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, USBDeviceImpl->libUSBInit());
}

TEST_F(USBDeviceTest, libUSBInit_ContextInitBoundaryError)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_INVALID_PARAM));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_strerror(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return("Invalid parameter"));

    EXPECT_EQ(Core::ERROR_GENERAL, USBDeviceImpl->libUSBInit());
}

TEST_F(USBDeviceTest, libUSBInit_MultipleErrorCodes)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_OTHER));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_strerror(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return("Other error"));

    EXPECT_EQ(Core::ERROR_GENERAL, USBDeviceImpl->libUSBInit());
}

TEST_F(USBDeviceTest, libUSBInit_ContextInitNotFoundError)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_init_context(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NOT_FOUND));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_strerror(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return("Entity not found"));

    EXPECT_EQ(Core::ERROR_GENERAL, USBDeviceImpl->libUSBInit());
}

// ...existing code...

TEST_F(USBDeviceTest, libUSBHotPlugCallbackDeviceAttached_NullDevice)
{
    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(0);

    int result = USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(nullptr, nullptr, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

    EXPECT_EQ(0, result);
}

TEST_F(USBDeviceTest, libUSBHotPlugCallbackDeviceAttached_GetDeviceDescriptorFailure)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(0);

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    int result = USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

    EXPECT_EQ(0, result);
}

TEST_F(USBDeviceTest, libUSBHotPlugCallbackDeviceAttached_InvalidDeviceClass)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceSubClass = 0xFF;
                desc->bDeviceClass = 0xFF;
                return LIBUSB_SUCCESS;
            }));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_ADDRESS_1));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                return Core::ERROR_NONE;
            }));

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    int result = USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

    EXPECT_EQ(0, result);
}

TEST_F(USBDeviceTest, libUSBHotPlugCallbackDeviceAttached_GetBusNumberFailure)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                return LIBUSB_SUCCESS;
            }));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_ADDRESS_1));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(0);

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    int result = USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

    EXPECT_EQ(0, result);
}

TEST_F(USBDeviceTest, libUSBHotPlugCallbackDeviceAttached_GetDeviceAddressFailure)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                return LIBUSB_SUCCESS;
            }));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(0);

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    int result = USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

    EXPECT_EQ(0, result);
}

TEST_F(USBDeviceTest, libUSBHotPlugCallbackDeviceAttached_GetPortNumbersFailure)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                return LIBUSB_SUCCESS;
            }));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_ADDRESS_1));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(0);

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    int result = USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

    EXPECT_EQ(0, result);
}

TEST_F(USBDeviceTest, libUSBHotPlugCallbackDeviceAttached_EmptyDevicePath)
{
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_NE(text.find("\"devicePath\":\"\""), std::string::npos);
                return Core::ERROR_NONE;
            }));

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    int result = USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, nullptr);

    EXPECT_EQ(0, result);
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_NullDevice)
{
    string serialNumber;
    
    USBDeviceImpl->getDeviceSerialNumber(nullptr, serialNumber);
    
    EXPECT_EQ(serialNumber, string(""));
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_GetBusNumberReturnsZero)
{
    string serialNumber;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));
    
    libusb_device dev = {0};
    dev.bus_number = 0;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string(""));
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_GetPortNumbersReturnsZero)
{
    string serialNumber;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string(""));
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_GetPortNumbersNegativeReturn)
{
    string serialNumber;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(-1));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string(""));
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_SysfsPathDoesNotExist)
{
    string serialNumber;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(255));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = 255;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = 255;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = 255;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string(""));
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_SerialFileDoesNotExist)
{
    string serialNumber;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string(""));
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_EmptySerialFile)
{
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile.close();
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string(""));
    
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_MultiplePortNumbers)
{
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123-1/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << "TESTSERIAL123" << std::endl;
    serialNumOutFile.close();
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = 123;
                    port_numbers[1] = 1;
                    return 2;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string("TESTSERIAL123"));
    
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_MaxPortNumbers)
{
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-1-2-3-4-5-6-7/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << "MAXPORTSERIAL" << std::endl;
    serialNumOutFile.close();
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    for(int i = 0; i < 7; i++)
                    {
                        port_numbers[i] = i + 1;
                    }
                    return 7;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string("MAXPORTSERIAL"));
    
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_SerialFileReadFailure)
{
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << "VALIDSERIAL" << std::endl;
    serialNumOutFile.close();
    
    chmod(serialNumFileName.c_str(), 0000);
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string(""));
    
    chmod(serialNumFileName.c_str(), 0644);
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_WhitespaceInSerial)
{
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << "  SERIAL_WITH_SPACES  " << std::endl;
    serialNumOutFile.close();
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string("  SERIAL_WITH_SPACES  "));
    
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_SpecialCharactersInSerial)
{
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << "SERIAL@#$%^&*()" << std::endl;
    serialNumOutFile.close();
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, string("SERIAL@#$%^&*()"));
    
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDeviceSerialNumber_VeryLongSerial)
{
    string serialNumber;
    std::string longSerial(1024, 'A');
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << longSerial << std::endl;
    serialNumOutFile.close();
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDeviceSerialNumber(&dev, serialNumber);
    
    EXPECT_EQ(serialNumber, longSerial);
    
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDevicePathFromDevice_NullDevice)
{
    string devicePath;
    string serialNumber;
    
    USBDeviceImpl->getDevicePathFromDevice(nullptr, devicePath, serialNumber);
    
    EXPECT_EQ(devicePath, string(""));
    EXPECT_EQ(serialNumber, string(""));
}

TEST_F(USBDeviceTest, getDevicePathFromDevice_EmptySerialNumber)
{
    string devicePath;
    string serialNumber;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDevicePathFromDevice(&dev, devicePath, serialNumber);
    
    EXPECT_EQ(devicePath, string(""));
    EXPECT_EQ(serialNumber, string(""));
}

TEST_F(USBDeviceTest, getDevicePathFromDevice_DirectoryOpenFailure)
{
    string devicePath;
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << "TESTSERIAL" << std::endl;
    serialNumOutFile.close();
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    system("rm -rf /tmp/block");
    
    USBDeviceImpl->getDevicePathFromDevice(&dev, devicePath, serialNumber);
    
    EXPECT_EQ(devicePath, string(""));
    
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDevicePathFromDevice_NoMatchingDevice)
{
    string devicePath;
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << "TESTSERIAL" << std::endl;
    serialNumOutFile.close();
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    system("mkdir -p /tmp/block");
    
    USBDeviceImpl->getDevicePathFromDevice(&dev, devicePath, serialNumber);
    
    EXPECT_EQ(devicePath, string(""));
    
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDevicePathFromDevice_VendorFileReadFailure)
{
    string devicePath;
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << "TESTSERIAL" << std::endl;
    serialNumOutFile.close();
    
    std::string vendorFileName = "/tmp/block/sda/device/vendor";
    system("mkdir -p /tmp/block/sda/device");
    std::ofstream outVendorStream(vendorFileName);
    outVendorStream.close();
    chmod(vendorFileName.c_str(), 0000);
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDevicePathFromDevice(&dev, devicePath, serialNumber);
    
    EXPECT_EQ(devicePath, string(""));
    
    chmod(vendorFileName.c_str(), 0644);
    std::remove(vendorFileName.c_str());
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDevicePathFromDevice_ModelFileReadFailure)
{
    string devicePath;
    string serialNumber;
    std::string serialNumFileName = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile(serialNumFileName);
    serialNumOutFile << "TESTSERIAL" << std::endl;
    serialNumOutFile.close();
    
    std::string vendorFileName = "/tmp/block/sda/device/vendor";
    system("mkdir -p /tmp/block/sda/device");
    std::ofstream outVendorStream(vendorFileName);
    outVendorStream << "TestVendor" << std::endl;
    outVendorStream.close();
    
    std::string modelFileName = "/tmp/block/sda/device/model";
    std::ofstream outModelStream(modelFileName);
    outModelStream.close();
    chmod(modelFileName.c_str(), 0000);
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(MOCK_USB_DEVICE_BUS_NUMBER_1));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers))
                {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }));
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDevicePathFromDevice(&dev, devicePath, serialNumber);
    
    EXPECT_EQ(devicePath, string(""));
    
    chmod(modelFileName.c_str(), 0644);
    std::remove(modelFileName.c_str());
    std::remove(vendorFileName.c_str());
    std::remove(serialNumFileName.c_str());
}

TEST_F(USBDeviceTest, getDevicePathFromDevice_Success)
{
    string devicePath;
    string serialNumber;
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    
    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;
    
    USBDeviceImpl->getDevicePathFromDevice(&dev, devicePath, serialNumber);
    
    EXPECT_EQ(devicePath, string("/dev/sda"));
    EXPECT_EQ(serialNumber, string("B32FD507"));
}

TEST_F(USBDeviceTest, getUSBDescriptorValue_InvalidDescriptorIndex)
{
    libusb_device_handle handle = {0};
    string stringDescriptor;
    
    uint32_t result = USBDeviceImpl->getUSBDescriptorValue(&handle, 0x0409, 0, stringDescriptor);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(stringDescriptor, string(""));
}

TEST_F(USBDeviceTest, getUSBDescriptorValue_GetStringDescriptorFailure)
{
    libusb_device_handle handle = {0};
    string stringDescriptor;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor_ascii(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));
    
    uint32_t result = USBDeviceImpl->getUSBDescriptorValue(&handle, 0x0409, 1, stringDescriptor);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

TEST_F(USBDeviceTest, getUSBDescriptorValue_InvalidDescriptorType)
{
    libusb_device_handle handle = {0};
    string stringDescriptor;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 4;
                data[1] = 0xFF;
                return 4;
            }));
    
    uint32_t result = USBDeviceImpl->getUSBDescriptorValue(&handle, 0x0409, 1, stringDescriptor);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

TEST_F(USBDeviceTest, getUSBDescriptorValue_IncompleteDescriptor)
{
    libusb_device_handle handle = {0};
    string stringDescriptor;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 10;
                data[1] = LIBUSB_DT_STRING;
                return 4;
            }));
    
    uint32_t result = USBDeviceImpl->getUSBDescriptorValue(&handle, 0x0409, 1, stringDescriptor);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

TEST_F(USBDeviceTest, getUSBDescriptorValue_AsciiSuccess)
{
    libusb_device_handle handle = {0};
    string stringDescriptor;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor_ascii(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, unsigned char *data, int length) {
                strcpy((char*)data, "TestString");
                return 10;
            }));
    
    uint32_t result = USBDeviceImpl->getUSBDescriptorValue(&handle, 0x0409, 1, stringDescriptor);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(stringDescriptor, string("TestString"));
}

TEST_F(USBDeviceTest, getUSBDescriptorValue_Success)
{
    libusb_device_handle handle = {0};
    string stringDescriptor;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 8;
                data[1] = LIBUSB_DT_STRING;
                data[2] = 'T';
                data[3] = 0;
                data[4] = 'e';
                data[5] = 0;
                data[6] = 's';
                data[7] = 0;
                return 8;
            }));
    
    uint32_t result = USBDeviceImpl->getUSBDescriptorValue(&handle, 0x0409, 1, stringDescriptor);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(stringDescriptor, string("Tes"));
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_NullDevice)
{
    libusb_device_descriptor desc = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(nullptr, &desc, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_NullDescriptor)
{
    libusb_device dev = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, nullptr, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_NullDeviceInfo)
{
    libusb_device dev = {0};
    libusb_device_descriptor desc = {0};
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, &desc, nullptr);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_OpenFailure)
{
    libusb_device dev = {0};
    libusb_device_descriptor desc = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, &desc, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_GetLanguageDescriptorFailure)
{
    libusb_device dev = {0};
    libusb_device_descriptor desc = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    desc.iSerialNumber = 1;
    desc.iManufacturer = 2;
    desc.iProduct = 3;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<1>(nullptr),
            ::testing::Return(LIBUSB_SUCCESS)));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, &desc, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_NoLanguageIds)
{
    libusb_device dev = {0};
    libusb_device_descriptor desc = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    desc.iSerialNumber = 1;
    desc.iManufacturer = 2;
    desc.iProduct = 3;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<1>(nullptr),
            ::testing::Return(LIBUSB_SUCCESS)));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 2;
                return 2;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor_ascii(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(3)
        .WillRepeatedly(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, unsigned char *data, int length) {
                strcpy((char*)data, "Test");
                return 4;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, &desc, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_SingleLanguageSuccess)
{
    libusb_device dev = {0};
    libusb_device_descriptor desc = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    desc.iSerialNumber = 1;
    desc.iManufacturer = 2;
    desc.iProduct = 3;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<1>(nullptr),
            ::testing::Return(LIBUSB_SUCCESS)));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(4)
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 4;
                data[2] = 0x09;
                data[3] = 0x04;
                return 4;
            }))
        .WillRepeatedly(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 8;
                data[1] = LIBUSB_DT_STRING;
                data[2] = 'T';
                data[3] = 0;
                data[4] = 'e';
                data[5] = 0;
                data[6] = 's';
                data[7] = 0;
                return 8;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, &desc, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(deviceInfo.numLanguageIds, 1);
    EXPECT_EQ(deviceInfo.productInfo1.languageId, 0x0409);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_MultipleLanguagesSuccess)
{
    libusb_device dev = {0};
    libusb_device_descriptor desc = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    desc.iSerialNumber = 1;
    desc.iManufacturer = 2;
    desc.iProduct = 3;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<1>(nullptr),
            ::testing::Return(LIBUSB_SUCCESS)));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(10)
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 10;
                data[2] = 0x09;
                data[3] = 0x04;
                data[4] = 0x07;
                data[5] = 0x04;
                data[6] = 0x0C;
                data[7] = 0x04;
                data[8] = 0x16;
                data[9] = 0x04;
                return 10;
            }))
        .WillRepeatedly(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 8;
                data[1] = LIBUSB_DT_STRING;
                data[2] = 'T';
                data[3] = 0;
                data[4] = 'e';
                data[5] = 0;
                data[6] = 's';
                data[7] = 0;
                return 8;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, &desc, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(deviceInfo.numLanguageIds, 4);
    EXPECT_EQ(deviceInfo.productInfo1.languageId, 0x0409);
    EXPECT_EQ(deviceInfo.productInfo2.languageId, 0x0407);
    EXPECT_EQ(deviceInfo.productInfo3.languageId, 0x040C);
    EXPECT_EQ(deviceInfo.productInfo4.languageId, 0x0416);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_MoreThanFourLanguages)
{
    libusb_device dev = {0};
    libusb_device_descriptor desc = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    desc.iSerialNumber = 1;
    desc.iManufacturer = 2;
    desc.iProduct = 3;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<1>(nullptr),
            ::testing::Return(LIBUSB_SUCCESS)));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(10))
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 12;
                data[2] = 0x09;
                data[3] = 0x04;
                data[4] = 0x07;
                data[5] = 0x04;
                data[6] = 0x0C;
                data[7] = 0x04;
                data[8] = 0x16;
                data[9] = 0x04;
                data[10] = 0x19;
                data[11] = 0x04;
                return 12;
            }))
        .WillRepeatedly(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 8;
                data[1] = LIBUSB_DT_STRING;
                data[2] = 'T';
                data[3] = 0;
                data[4] = 'e';
                data[5] = 0;
                data[6] = 's';
                data[7] = 0;
                return 8;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, &desc, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(deviceInfo.numLanguageIds, 5);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_ZeroDescriptorIndices)
{
    libusb_device dev = {0};
    libusb_device_descriptor desc = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    desc.iSerialNumber = 0;
    desc.iManufacturer = 0;
    desc.iProduct = 0;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<1>(nullptr),
            ::testing::Return(LIBUSB_SUCCESS)));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 4;
                data[2] = 0x09;
                data[3] = 0x04;
                return 4;
            }));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, &desc, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(deviceInfo.numLanguageIds, 1);
}

TEST_F(USBDeviceTest, getUSBExtInfoStructFromDeviceDescriptor_DescriptorValueFailure)
{
    libusb_device dev = {0};
    libusb_device_descriptor desc = {0};
    Exchange::IUSBDevice::USBDeviceInfo deviceInfo = {0};
    
    desc.iSerialNumber = 1;
    desc.iManufacturer = 2;
    desc.iProduct = 3;
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<1>(nullptr),
            ::testing::Return(LIBUSB_SUCCESS)));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[0] = 4;
                data[2] = 0x09;
                data[3] = 0x04;
                return 4;
            }))
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor_ascii(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));
    
    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);
    
    uint32_t result = USBDeviceImpl->getUSBExtInfoStructFromDeviceDescriptor(&dev, &desc, &deviceInfo);
    
    EXPECT_EQ(result, Core::ERROR_GENERAL);
}

// ...existing code...
