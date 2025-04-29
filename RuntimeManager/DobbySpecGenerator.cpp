/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#include "DobbySpecGenerator.h"
#include "ApplicationConfiguration.h"
#include "UtilsLogging.h"
#include <sys/mount.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <bitset>
#include <set>
#include <sys/sysinfo.h>

namespace WPEFramework
{
namespace Plugin
{

namespace 
{
    const std::string WESTEROS_SOCKET_MOUNT_POINT = "/tmp/westeros";
    const std::string RUNTIME_PATH_MOUNT_POINT = "/runtime";

    const int NONHOMEAPP_MEM_LIMIT = 524288000;
    const int NONHOMEAPP_GPUMEM_LIMIT = 367001600;
    const int ION_DEFAULT_HEAP_QUOTA_LIMIT = 268435456;

    const size_t CONTAINER_LOG_CAP = 65536;
}

DobbySpecGenerator::DobbySpecGenerator(): mIonMemoryPluginData(Json::objectValue), mPackageMountPoint("/package"), mRuntimeMountPoint("/runtime")
{
    LOGINFO("DobbySpecGenerator()");
    initialiseIonHeapsJson();
}

DobbySpecGenerator::~DobbySpecGenerator()
{
    LOGINFO("~DobbySpecGenerator()");
}

bool DobbySpecGenerator::generate(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig, string& resultSpec)
{
    LOGINFO("DobbySpecGenerator::generate()");
    resultSpec = "";

    std::ifstream inFile("/tmp/specchange");
    if (inFile.good())
    {
        inFile.open("/tmp/specchange"); //open the input file
        std::stringstream strStream;
        strStream << inFile.rdbuf(); //read the file
        resultSpec = strStream.str(); //str holds the content of the file
        std::cout << resultSpec << "\n"; //you can do anything with the string!!!
        inFile.close();
        JsonObject parameters;
        parameters.FromString(resultSpec.c_str());
        return true;
    }
   
    Json::Value spec;
    spec["version"] = "1.1";
    spec["memLimit"] = getSysMemoryLimit(config, runtimeConfig);

    Json::Value args(Json::arrayValue);
    args.append(runtimeConfig.command);
    //TODO : What if more args?
    /*
    for (const string& arg : config.mArgs)
        args.append(arg);
    */
    spec["args"] = std::move(args);
    spec["cwd"] = mPackageMountPoint;
    // if app uses graphics enable gpu and gpu mem limit
    if (shouldEnableGpu(config))
    {
        Json::Value gpuObj(Json::objectValue);
        gpuObj["enable"] = true;
        gpuObj["memLimit"] = getGPUMemoryLimit(config, runtimeConfig);
        spec["gpu"] = std::move(gpuObj);
    }
    spec["restartOnCrash"] = false;

    Json::Value vpuObj;
    vpuObj["enable"] = getVpuEnabled(config);
    spec["vpu"] = std::move(vpuObj);
    
    Json::Value cpuObj;
    cpuObj["cores"] = getCpuCores();
    spec["cpu"] = std::move(cpuObj);

    Json::Value consoleObj;
    //TODO: limit, path properties are retrieved from app package and need to be populated here
    consoleObj["limit"] = CONTAINER_LOG_CAP;
    consoleObj["path"] = "/tmp/container.log";
    spec["console"] = std::move(consoleObj);

    Json::Value etcObj;
    Json::Value hostsArray(Json::arrayValue);
    Json::Value servicesArray(Json::arrayValue);
    // always add a local host entry
    hostsArray.append("127.0.0.1\tlocalhost");

    // add some common services for the app
    servicesArray.append("ftp\t\t21/tcp");
    servicesArray.append("domain\t\t53/tcp");
    servicesArray.append("domain\t\t53/udp");
    servicesArray.append("http\t\t80/tcp\t\twww");
    servicesArray.append("http\t\t80/udp");
    servicesArray.append("ntp\t\t123/udp");
    servicesArray.append("https\t\t443/tcp");
    servicesArray.append("https\t\t443/udp");
    //TODO Read for mapi capability from package and populate below for every mapi ports
    //servicesArray.append("mapi\t\t" + std::to_string(port) + "/tcp");

    //TODO: Based on soc and if broadcom, populate wayland-client and wayland-egl
    Json::Value preloadsArray(Json::arrayValue);
    etcObj["hosts"] = hostsArray;
    etcObj["services"] = servicesArray;
    etcObj["ld-preload"] = preloadsArray;
    spec["etc"] = std::move(etcObj);

    // TODO: verify if we need EthanLog plugin, it seems it works in conjunction with AI 1.0
    // standard plugins are EthanLog and OCDM, OCDM is enabled if an app does not user rialto nad requires drm
    // TODO: not for Q1 vpu, dbus, seccomp

    // TODO: network field should not be needed, in dobby it just forces adding network plugin
    // verify, if we can safely remove it, as we always add network plugin
    if (runtimeConfig.wanLanAccess)
    {
        spec["network"] = "nat";
    }
    else
    {
        spec["network"] = "private";
    }
    Json::Value userObj;
    userObj["uid"] = config.mUserId;
    userObj["gid"] = config.mGroupId;
    spec["user"] = std::move(userObj);

    //spec["plugins"] = populateClassicPlugins(config, runtimeConfig);
    populateClassicPlugins(config, runtimeConfig, spec);
    spec["rdkPlugins"] = createRdkPlugins(config, runtimeConfig);
    spec["mounts"] = createMounts(config, runtimeConfig);
    spec["env"] = createEnvVars(config, runtimeConfig);

    Json::FastWriter writer;
    resultSpec = writer.write(spec);
    LOGINFO("spec: '%s'\n", resultSpec.c_str());

    std::ofstream generatedSpecFile;
    generatedSpecFile.open("/tmp/generatedSpec.json");
    generatedSpecFile << resultSpec.c_str();
    generatedSpecFile.close();

    return true;
}

Json::Value DobbySpecGenerator::createEnvVars(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig) const
{
    Json::Value env(Json::arrayValue);
    env.append(std::string("APPLICATION_NAME=") + config.mAppId);
    
//"APPLICATION_LAUNCH_PARAMETERS=eyJHV19JUCI6IiIsImFyZ3MiOnt9LCJjb250ZXh0Ijp7InNvdXJjZSI6ImFwcHMtcmFpbC1sYXVuY2gifSwib3JpZ2luIjoiRVBHIn0=",
//"APPLICATION_LAUNCH_METHOD=EPG",
//"APPLICATION_TOKEN=1ae0cc31-7d6e-4d2a-bf15-5522126c1b2e",
//"DEVICE_FRIENDLYNAME=Living Room",
//"DEVICE_MODEL_NUM=ELTE11MWR",
//"PARTNER_ID=xglobal",
//"REGION=USA",
//"LANG=en_US",
//"ADDITIONAL_DATA_URL=http%3A%2F%2F127.0.0.1%3A8009%2Fe6486224-8058-49c2-936b-4f5a87c45bb2%2FYouTube%2Fdial_data",

    for (const std::string& str : runtimeConfig.envVariables)
    {
        env.append(str);
    }

    if (!config.mWesterosSocketPath.empty())
    {
        env.append("XDG_RUNTIME_DIR=/tmp");
        env.append("WAYLAND_DISPLAY=westeros");
        env.append("WESTEROS_SINK_VIRTUAL_WIDTH=1920");
        env.append("WESTEROS_SINK_VIRTUAL_HEIGHT=1080");
        env.append("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION=wayland-egl");
        env.append("QT_WAYLAND_SHELL_INTEGRATION=wl-simple-shell");

        //TODO Find the place where it is populated from appsservice
        env.append("WESTEROS_SINK_AMLOGIC_USE_DMABUF=1");
        env.append("WESTEROS_GL_USE_AMLOGIC_AVSYNC=1");
        env.append("WESTEROS_SINK_USE_FREERUN=1");
        env.append("WESTEROS_GL_MODE=3840x2160x60");
        env.append("WESTEROS_GL_GRAPHICS_MAX_SIZE=1920x1080");
        env.append("WESTEROS_GL_USE_REFRESH_LOCK=1");
    }
    //TODO: If broadcom soc
    //WESTEROS_VPC_BRIDGE=westeros
    //TODO: This is true by default
    if (runtimeConfig.resourceManagerClientEnabled)
    {
        env.append("ESSRMGR_APPID=" + config.mAppId);
        env.append("CLIENT_IDENTIFIER=" + config.mAppId);
        if (!config.mWesterosSocketPath.empty())
        {
            env.append("WESTEROS_SINK_USE_ESSRMGR=1");
        }
    }	     

    if (runtimeConfig.dial)
    {
        std::string dialId = runtimeConfig.dialId;
        if (dialId.empty())
            dialId = config.mAppId;

        env.append(std::string("APPLICATION_DIAL_NAME=") + dialId);
        //TODO: Need dial config get from somewhere
        //std::ostringstream dataUrlStream;
        //dataUrlStream << "http://127.0.0.1:"
        //              << mDialConfig->getDialServerPort() << '/'
        //              << mDialConfig->getDialServerPathPrefix() << '/'
        //              << dialId << '/'
        //              << "dial_data";

        //const std::string dataUrl = AICommon::encodeURL(dataUrlStream.str());
        //env.append(std::string("ADDITIONAL_DATA_URL=") + dataUrl);

        //env.append(std::string("DIAL_USN=") + mDialConfig->getDialUsn());
	const std::string dataUrl("");
	const std::string dialUsn("");
        env.append(std::string("ADDITIONAL_DATA_URL=") + dataUrl);
        env.append(std::string("DIAL_USN=") + dialUsn);
    }
    return env;
}

Json::Value DobbySpecGenerator::createMounts(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig) const
{
    Json::Value mounts(Json::arrayValue);

    if (!runtimeConfig.appPath.empty())
    {
        mounts.append(createBindMount(runtimeConfig.appPath, mPackageMountPoint, MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV));
    }

    if (!runtimeConfig.runtimePath.empty())
    {
        mounts.append(createBindMount(runtimeConfig.runtimePath, mRuntimeMountPoint, MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV));
    }

    // TODO add extra mounts from config
    mounts.append(createBindMount("/etc/ssl/certs", "/etc/ssl/certs",
                               (MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV)));

    mounts.append(createPrivateDataMount(runtimeConfig));
    createFkpsMounts(config, runtimeConfig, mounts);
    return mounts;
}

Json::Value DobbySpecGenerator::createBindMount(const std::string& source,
                                                const std::string& destination,
                                                unsigned long mountFlags) const
{
    Json::Value mount(Json::objectValue);

    mount["source"] = source;
    mount["destination"] = destination;
    mount["type"] = "bind";

    Json::Value mountOptions(Json::arrayValue);;

    static const std::vector<std::pair<unsigned long, std::string>> mountFlagsNames =
    {
        {   MS_BIND | MS_REC,   "rbind"         },
        {   MS_BIND,            "bind"          },
        {   MS_SILENT,          "silent"        },
        {   MS_RDONLY,          "ro"            },
        {   MS_SYNCHRONOUS,     "sync"          },
        {   MS_NOSUID,          "nosuid"        },
        {   MS_DIRSYNC,         "dirsync"       },
        {   MS_NODIRATIME,      "nodiratime"    },
        {   MS_RELATIME,        "relatime"      },
        {   MS_NOEXEC,          "noexec"        },
        {   MS_NODEV,           "nodev"         },
        {   MS_NOATIME,         "noatime"       },
        {   MS_STRICTATIME,     "strictatime"   },
    };

    // convert the mount flags to their string equivalents
    for (const auto &entry : mountFlagsNames)
    {
        const unsigned long mountFlag = entry.first;
        if ((mountFlag & mountFlags) == mountFlag)
        {
            mountOptions.append(entry.second);

            // clear the mount flags bit such that we can display a warning
            // if the caller supplies a flag we don't support
            mountFlags &= ~mountFlag;
        }
    }

    // if there was a mount flag we didn't support display a warning
    if (mountFlags != 0)
    {
        LOGWARN("unsupported mount flag(s) 0x%04lx", mountFlags);
    }

    mount["options"] = std::move(mountOptions);
    
    return mount;
}

bool DobbySpecGenerator::shouldEnableGpu(const ApplicationConfiguration& config) const
{
    return !config.mWesterosSocketPath.empty();
}

ssize_t DobbySpecGenerator::getSysMemoryLimit(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig) const
{
    ssize_t memoryLimit = runtimeConfig.systemMemoryLimit;
    if (memoryLimit <= 0)
    {
        if (runtimeConfig.appType == ApplicationType::INTERACTIVE)
	{
            memoryLimit = NONHOMEAPP_MEM_LIMIT;  
        }
        //TODO: Add other application types
    }
    return memoryLimit;
}

ssize_t DobbySpecGenerator::getGPUMemoryLimit(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig) const
{
    ssize_t gpuMemoryLimit = runtimeConfig.gpuMemoryLimit;
    if (gpuMemoryLimit <= 0)
    {
        if (runtimeConfig.appType == ApplicationType::INTERACTIVE)
	{
            gpuMemoryLimit = NONHOMEAPP_GPUMEM_LIMIT;
        }
        //TODO: Add other application types
    }
    return gpuMemoryLimit;
}

bool DobbySpecGenerator::getVpuEnabled(const ApplicationConfiguration& config) const
{
    //TODO: To return true, if below conditions are met:
    // check if app is not system app
    // rialto is not enabled
    // vpus blacklist is not having app
    
    return true;
}

std::string DobbySpecGenerator::getCpuCores()
{
    //TODO: Populate cpu bitmask from config (aisettings.json)
    
    //std::bitset<32> cpuSetBitmask = mConfig->getAppsCpuSet();
    //cpuSetBitmask &= ((0x1U << nCores) - 1);

    // check the bitmask so that we have at least one core enabled
    //if (cpuSetBitmask.none())
    //{
    //    cpuSetBitmask = ((0x1U << nCores) - 1);
    //}

    const int nCores = std::min(32, get_nprocs());
    std::bitset<32> cpuSetBitmask = ((0x1U << nCores) - 1);
    // create the json string for the cores to enable
    std::ostringstream coresStream;
    for (int core = 0; core < nCores; core++)
    {
        if (cpuSetBitmask.test(core))
        {
            coresStream << core << ",";
        }
    }

    // get the string and remove trailing ','
    std::string coresStr = coresStream.str();
    if (!coresStr.empty())
        coresStr.pop_back();

    return coresStr;
}

Json::Value DobbySpecGenerator::createRdkPlugins(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig) const
{
    Json::Value rdkPluginsObj(Json::objectValue);
    //TODO: Appsservice plugin is not needed for sure?
    if (!config.mPorts.empty())
    {
        rdkPluginsObj["appservicesrdk"] = createAppServiceSDKPlugin(config, runtimeConfig);
    }
    // TODO create ionplugin on xione
    rdkPluginsObj["ionmemory"] = createIonMemoryPlugin();

    rdkPluginsObj["minidump"] = createMinidumpPlugin();


    rdkPluginsObj["networking"] = createNetworkPlugin(config, runtimeConfig);
    if (runtimeConfig.thunder)
    {
        rdkPluginsObj["thunder"] = createThunderPlugin(config);
    }
    //TODO: Nat holepuncher, multicast socket plugin, credentialmanager plugin, httpproxy
    return rdkPluginsObj;
}

Json::Value DobbySpecGenerator::createMinidumpPlugin() const
{
    Json::Value pluginObj(Json::objectValue);
    static const Json::StaticString minidumpPath("/opt/minidumps");
    static const Json::StaticString minidumpSecurePath("/opt/secure/minidumps");

    pluginObj["required"] = false;
    if (access("/tmp/.SecureDumpDisable", R_OK) == 0)
    {
        pluginObj["data"]["destinationPath"] = minidumpPath;
    }
    else
    {
        pluginObj["data"]["destinationPath"] = minidumpSecurePath;
    }
    return pluginObj;
}

Json::Value DobbySpecGenerator::createAppServiceSDKPlugin(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig) const
{
    //TODO: May need to check for any local services, dial port
    Json::Value pluginObj(Json::objectValue);

    pluginObj["required"] = false;

    Json::Value dependencies(Json::arrayValue);
    dependencies.append("networking");

    pluginObj["dependsOn"] = std::move(dependencies);

    if (runtimeConfig.dial)
    {
        //TODO: Need to populate dial ports	    
    }
    Json::Value ports(Json::arrayValue);
    for (auto port : config.mPorts)
        ports.append(port);

    pluginObj["data"]["additionalPorts"] = std::move(ports);

    return pluginObj;
}

Json::Value DobbySpecGenerator::createNetworkPlugin(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig) const
{
    Json::Value pluginObj(Json::objectValue);

    pluginObj["required"] = true;

    Json::Value dataObj(Json::objectValue);

    if (runtimeConfig.wanLanAccess)
    {
        dataObj["type"] = "nat";
        dataObj["dnsmasq"] = true;
    }
    else
    {
        dataObj["type"] = "none";
        dataObj["dnsmasq"] = false;
    }

    dataObj["ipv4"] = true;
    //TODO: Need to read config to enable ipv6 or not
    dataObj["ipv6"] = false;

    //TODO May or may not need these parameters
    /*
    // add NAT holepunch support if requested (only for non-html apps)
    if (appPackage->hasCapability(IPackage::Capability::NatHolePunch) &&
        isAllowedHolePunch(appPackage))
    {
        data["portForwarding"]["hostToContainer"] = createHolePunchArray(appPackage);
    }

    // add multicast forwarding - primarily used for netflix MDX
    if (appPackage->hasCapability(IPackage::Capability::MulticastForward))
    {
        data["multicastForwarding"] = createMulticastGroupsArray(appPackage);
    }

    // add inter-container communication
    if (appPackage->hasCapability(IPackage::Capability::LocalSocketServer) ||
        appPackage->hasCapability(IPackage::Capability::LocalSocketClient))
    {
        Json::Value interContainerArray = createInterContainerArray(appPackage);
        if (!interContainerArray.empty())
        {
            data[interContainer] = std::move(interContainerArray);
        }
    }
    */
    pluginObj["data"] = std::move(dataObj);

    return pluginObj;
}

void DobbySpecGenerator::populateClassicPlugins(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig, Json::Value& spec)
{
    Json::Value pluginsArray(Json::arrayValue);
    // enable the logging plugin
    pluginsArray.append(createEthanLogPlugin(config, runtimeConfig));
    
    //TODO: if webapp or not needed rialto. how to get these info?
    pluginsArray.append(createOpenCDMPlugin(config, runtimeConfig));

    spec["plugins"] = std::move(pluginsArray);
}

Json::Value DobbySpecGenerator::createEthanLogPlugin(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig) const
{
    Json::Value plugin(Json::objectValue);
    static const Json::StaticString name("name");
    static const Json::StaticString data("data");
    static const Json::StaticString pluginName("EthanLog");

    static const Json::StaticString loglevels("loglevels");
    static const Json::StaticString fatal("fatal");
    static const Json::StaticString error("error");
    static const Json::StaticString warning("warning");
    static const Json::StaticString milestone("milestone");
    static const Json::StaticString info("info");
    static const Json::StaticString debug("debug");

    Json::Value levels(Json::arrayValue);
    //TODO: Read logging mask from package
    /*
    unsigned logMask = package->loggingMask();
    if (logMask & unsigned(IPackage::LogLevel::Default))
        logMask |= mDefaultLoggingMask;

    if (logMask & unsigned(IPackage::LogLevel::Fatal))
        levels.append(fatal);
    if (logMask & unsigned(IPackage::LogLevel::Error))
        levels.append(error);
    if (logMask & unsigned(IPackage::LogLevel::Warning))
        levels.append(warning);
    if (logMask & unsigned(IPackage::LogLevel::Info))
        levels.append(info);
    if (logMask & unsigned(IPackage::LogLevel::Debug))
        levels.append(debug);
    if (logMask & unsigned(IPackage::LogLevel::Milestone))
        levels.append(milestone);
    */
    levels.append(fatal);
    levels.append(error);
    levels.append(warning);
    levels.append(info);
    levels.append(debug);
    levels.append(milestone);

    plugin[name] = pluginName;
    plugin[data][loglevels] = levels;

    return plugin;
}

Json::Value DobbySpecGenerator::createIonMemoryPlugin() const
{
    Json::Value plugin(Json::objectValue);
    static const Json::StaticString data("data");
    plugin[data] = mIonMemoryPluginData;
    return plugin;
}

Json::Value DobbySpecGenerator::createThunderPlugin(const ApplicationConfiguration& config) const
{
    static const Json::StaticString dependsOn("dependsOn");
    static const Json::StaticString bearerUrl("bearerUrl");
    static const Json::StaticString data("data");
    static const Json::StaticString localServices2("");
    static const Json::StaticString localServices5("");

    Json::Value plugin(Json::objectValue);
    Json::Value dependencies(Json::arrayValue);
    dependencies.append("networking");
    plugin[dependsOn] = std::move(dependencies);

    //TODO: Check for localservices1,localservices2,localservices3,localservices4
    //plugin[data][bearerUrl] = localServices5;
    plugin[data][bearerUrl] = localServices2;

    return plugin;
}

Json::Value DobbySpecGenerator::createOpenCDMPlugin(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig) const
{
    static const Json::StaticString name("name");
    static const Json::StaticString data("data");
    static const Json::StaticString pluginName("OpenCDM");

    Json::Value plugin(Json::objectValue);
    plugin[name] = pluginName;
    plugin[data] = Json::Value::null;

    return plugin;
}

void DobbySpecGenerator::initialiseIonHeapsJson()
{
    Json::Value heapsArray(Json::arrayValue);
    //TODO: Get ion heap quotas and default heap quota
    /*
    for (const auto &heapQuota : mConfig->getIonHeapQuotas())
    {
        Json::Value heapObject(Json::objectValue);
        heapObject["name"] = heapQuota.first;
        heapObject["limit"] = heapQuota.second;

        heapsArray.append(std::move(heapObject));
    }

    mIonMemoryPluginData["defaultLimit"] = mConfig->getIonHeapDefaultQuota();
    */
    mIonMemoryPluginData["defaultLimit"] = ION_DEFAULT_HEAP_QUOTA_LIMIT;
    mIonMemoryPluginData["heaps"] = std::move(heapsArray);
}

Json::Value DobbySpecGenerator::createPrivateDataMount(RuntimeConfig& runtimeConfig) const
{
    static const Json::StaticString source("source");
    static const Json::StaticString destination("destination");
    static const Json::StaticString type("type");
    static const Json::StaticString loop("loop");
    static const Json::StaticString fstype("fstype");
    static const Json::StaticString ext4("ext4");
    static const Json::StaticString options("options");

    static const Json::StaticString nosuid("nosuid");
    static const Json::StaticString nodev("nodev");
    static const Json::StaticString noexec("noexec");

    Json::Value mount(Json::objectValue);
    //TODO: Read data image from package manager from here
    /*
    const std::string sourcePath = package->privateDataImagePath();
    */
    const std::string sourcePath("");
    if (sourcePath.empty())
    {
        return Json::nullValue;
    }

    mount[source] = sourcePath;
    mount[destination] = "/home/private";
    mount[type] = loop;
    mount[fstype] = ext4;

    Json::Value optionsArray(Json::arrayValue);
    optionsArray.append(nosuid);
    optionsArray.append(nodev);
    optionsArray.append(noexec);
    mount[options] = optionsArray;

    return mount;
}

void DobbySpecGenerator::createFkpsMounts(const ApplicationConfiguration& config, RuntimeConfig& runtimeConfig, Json::Value& spec) const
{

    // TODO: get the list of fkps files from app config from package manager
    // get the list of FKPS files to map, this comes from the apps config.xml
    /*
    std::optional<std::set<std::string>> fkpsFiles =
        package->capabilityValueSet(packagemanager::IPackage::Capability::FkpsAccess);
    */
    std::set<std::string> fkpsFiles = {
        "ffffffff00000001.key",
        "ffffffff00000001.sha",
        "ffffffff00000001.keyinfo",
        "ffffffff00000002.bin",
        "ffffffff00000002.sha",
        "ffffffff00000004.bin",
        "ffffffff00000004.sha",
        "ffffffff00000006.bin",
        "ffffffff00000006.sha",
        "ffffffff00000007.bin",
        "ffffffff00000008.bin",
        "ffffffff00000009.key",
        "ffffffff00000009.sha",
        "ffffffff00000009.keyinfo",
        "ffffffff0000000a.sha",
        "ffffffff0000000a.bin",
        "0381000003810001.key",
        "0381000003810001.keyinfo",
        "0381000003810001.sha",
        "0381000003810002.key",
        "0381000003810002.keyinfo",
        "0381000003810003.key",
        "0381000003810003.keyinfo",
        "0681000006810001.bin"
    };
//TODO: Check size is 0

    if (fkpsFiles.empty())
        return;

    // iterate through the files and make sure they're accessible
    const std::string fkpsPathPrefix("/opt/drm/");
    for (std::set<std::string>::iterator it=fkpsFiles.begin(); it!=fkpsFiles.end(); ++it)
    //for (const std::string &fkpsFile : fkpsFiles.value())
    {
	std::string fkpsFile = *it;      
        const std::string fkpsFilePath = fkpsPathPrefix + fkpsFile;

        // check if the file exists
        struct stat details;
        if (stat(fkpsFilePath.c_str(), &details) != 0)
        {
            if (errno == ENOENT)
            {
                printf("missing FKPS file '%s', won't map into container",
                            fkpsFilePath.c_str());
            }
            else
            {
                printf("failed to set FKPS file '%s' %d",
                                 fkpsFilePath.c_str(), errno);
            }

            continue;
        }

        // check the group owner matches the app
        if ((details.st_gid != config.mGroupId) &&
            (chown(fkpsFilePath.c_str(), -1, config.mGroupId) != 0))
        {
            printf("failed to change group owner of '%s' %d",
                             fkpsFilePath.c_str(), errno);
        }

        // and that the group perms are set to r--
        if (((details.st_mode & S_IRGRP) != S_IRGRP) &&
            (chmod(fkpsFilePath.c_str(), ((details.st_mode | S_IRGRP) & ALLPERMS)) != 0))
        {
            printf("failed to set file permissions to 0%03o for '%s' %d",
                             ((details.st_mode & ~S_IRWXG) | S_IRGRP), fkpsFilePath.c_str(), errno);
        }

        // finally add a bind mount for them
        spec.append(createBindMount(fkpsFilePath, fkpsFilePath,
                                          (MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC)));
    }

    // additional tmpfs mount apparently required for YT certification
    spec.append(createTmpfsMount("/opt/drm/vault",
                                       (MS_NOSUID | MS_NODEV | MS_NOEXEC)));

}

Json::Value DobbySpecGenerator::createTmpfsMount(const std::string &mntDestination,
                                                 unsigned long mntOptions) const
{
    static const Json::StaticString source("source");
    static const Json::StaticString destination("destination");
    static const Json::StaticString type("type");
    static const Json::StaticString tmpfs("tmpfs");
    static const Json::StaticString options("options");

    static const Json::StaticString nosuid("nosuid");
    static const Json::StaticString nodev("nodev");
    static const Json::StaticString noexec("noexec");
    static const Json::StaticString noatime("noatime");
    static const Json::StaticString relatime("relatime");
    static const Json::StaticString strictatime("strictatime");
    static const Json::StaticString sync("sync");
    static const Json::StaticString nodiratime("nodiratime");
    static const Json::StaticString size("size=65536k");
    static const Json::StaticString indoes("nr_inodes=8k");
    Json::Value mount(Json::objectValue);

    mount[type] = tmpfs;
    mount[source] = tmpfs;
    mount[destination] = mntDestination;

    Json::Value optionsArray(Json::arrayValue);

    if (mntOptions & MS_NOSUID)
        optionsArray.append(nosuid);
    if (mntOptions & MS_NODEV)
        optionsArray.append(nodev);
    if (mntOptions & MS_NOEXEC)
        optionsArray.append(noexec);

    if (mntOptions & MS_SYNCHRONOUS)
        optionsArray.append(sync);

    if (mntOptions & MS_NODIRATIME)
        optionsArray.append(nodiratime);

    if (mntOptions & MS_NOATIME)
        optionsArray.append(noatime);
    if (mntOptions & MS_RELATIME)
        optionsArray.append(relatime);
    if (mntOptions & MS_STRICTATIME)
        optionsArray.append(strictatime);

    optionsArray.append(size);
    optionsArray.append(indoes);

    mount[options] = std::move(optionsArray);

    return mount;
}
} // namespace Plugin
} // namespace WPEFramework
