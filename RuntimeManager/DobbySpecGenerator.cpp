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
#include <fstream>

namespace WPEFramework
{
namespace Plugin
{

namespace 
{
    const std::string WESTEROS_SOCKET_MOUNT_POINT = "/tmp/westeros";
    const std::string APPS_PATH_MOUNT_POINT = "/package";
    const std::string RUNTIME_PATH_MOUNT_POINT = "/runtime";

    const int DEFAULT_MEM_LIMIT = 41943040;
    const int GPU_MEM_LIMIT = 367001600;
    const size_t CONTAINER_LOG_CAP = 65536;
}

DobbySpecGenerator::DobbySpecGenerator()
{
    LOGINFO("DobbySpecGenerator()");
}

DobbySpecGenerator::~DobbySpecGenerator()
{
    LOGINFO("~DobbySpecGenerator()");
}

bool DobbySpecGenerator::generate(const ApplicationConfiguration& config, string& resultSpec)
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
    spec["cwd"] = APPS_PATH_MOUNT_POINT;

    {
        Json::Value args(Json::arrayValue);
        for (const string& arg : config.mArgs)
            args.append(arg);
        spec["args"] = std::move(args);
    }
    
    {
        Json::Value consoleObj;
        consoleObj["limit"] = CONTAINER_LOG_CAP;
        consoleObj["path"] = "/tmp/container.log";
        spec["console"] = std::move(consoleObj);
    }

    {
        Json::Value user;
        user["uid"] = config.mUserId;
        user["gid"] = config.mGroupId;
        spec["user"] = std::move(user);
    }

    // if app uses graphics enable gpu and gpu mem limit
    if (shouldEnableGpu(config))
    {
        Json::Value gpuObj(Json::objectValue);
        gpuObj["enable"] = true;
        gpuObj["memLimit"] = GPU_MEM_LIMIT;
        spec["gpu"] = std::move(gpuObj);
    }

    spec["memLimit"] = getSysMemoryLimit(config);
    spec["env"] = createEnvVars(config);
    spec["mounts"] = createMounts(config);
    spec["rdkPlugins"] = creteRdkPlugins(config);

    // TODO: verify if we need EthanLog plugin, it seems it works in conjunction with AI 1.0
    // standard plugins are EthanLog and OCDM, OCDM is enabled if an app does not user rialto nad requires drm
    // TODO: not for Q1 vpu, dbus, seccomp

    // TODO: network field should not be needed, in dobby it just forces adding network plugin
    // verify, if we can safely remove it, as we always add network plugin
    spec["network"] = "nat";

    Json::FastWriter writer;
    resultSpec = writer.write(spec);
    LOGINFO("spec: '%s'\n", resultSpec.c_str());
    return true;
}

Json::Value DobbySpecGenerator::createEnvVars(const ApplicationConfiguration& config) const
{
    Json::Value env(Json::arrayValue);
    
    for (const std::string& str : config.mEnvVars)
    {
        env.append(str);
    }

    if (!config.mWesterosSocketPath.empty())
    {
		env.append("XDG_RUNTIME_DIR=/tmp");
		env.append("WAYLAND_DISPLAY=westeros");
    }

    return env;
}

Json::Value DobbySpecGenerator::createMounts(const ApplicationConfiguration& config) const
{
    Json::Value mounts(Json::arrayValue);

    if (!config.mAppPath.empty())
    {
        mounts.append(createBindMount(config.mAppPath, APPS_PATH_MOUNT_POINT, MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV));
    }

    if (!config.mRuntimePath.empty())
    {
        mounts.append(createBindMount(config.mRuntimePath, RUNTIME_PATH_MOUNT_POINT, MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV));
    }

    // this is not needed, Dobby adds this mount
    // if (!config.mWesterosSocketPath.empty())
    // {
    //     mounts.append(createBindMount(config.mWesterosSocketPath, WESTEROS_SOCKET_MOUNT_POINT, MS_BIND | MS_NOSUID | MS_NODEV | MS_NOEXEC));
    // }

    // TODO add extra mounts from config

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

ssize_t DobbySpecGenerator::getSysMemoryLimit(const ApplicationConfiguration& /*config*/) const
{
    // TODO mem limit should depend on application type
    return DEFAULT_MEM_LIMIT;
}

Json::Value DobbySpecGenerator::creteRdkPlugins(const ApplicationConfiguration& config) const
{
    Json::Value rdkPluginsObj(Json::objectValue);

    if (!config.mPorts.empty())
    {
        rdkPluginsObj["appservicesrdk"] = createAppServiceSDKPlugin(config);
    }

    rdkPluginsObj["minidump"] = createMinidumpPlugin(config);

    // TODO create ionplugin on xione

    rdkPluginsObj["networking"] = createNetworkPlugin(config);

    return rdkPluginsObj;
}

Json::Value DobbySpecGenerator::createMinidumpPlugin(const ApplicationConfiguration& config) const
{
    Json::Value pluginObj(Json::objectValue);

    pluginObj["required"] = false;
    pluginObj["data"]["destinationPath"] = "/opt/minidumps";

    return pluginObj;
}

Json::Value DobbySpecGenerator::createAppServiceSDKPlugin(const ApplicationConfiguration& config) const
{
    Json::Value pluginObj(Json::objectValue);

    pluginObj["required"] = false;

    Json::Value dependencies(Json::arrayValue);
    dependencies.append("networking");

    pluginObj["dependsOn"] = std::move(dependencies);

    Json::Value ports(Json::arrayValue);
    for (auto port : config.mPorts)
        ports.append(port);

    pluginObj["data"]["additionalPorts"] = std::move(ports);

    return pluginObj;
}

Json::Value DobbySpecGenerator::createNetworkPlugin(const ApplicationConfiguration& config) const
{
    Json::Value pluginObj(Json::objectValue);

    pluginObj["required"] = true;

    Json::Value dataObj(Json::objectValue);

    if (config.mWanLanAccess)
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
    dataObj["ipv6"] = false; // TODO need a switch for this

    pluginObj["data"] = std::move(dataObj);

    return pluginObj;
}

} // namespace Plugin
} // namespace WPEFramework
