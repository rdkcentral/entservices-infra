//
//  ContainerUtils.h
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#ifndef CONTAINERUTILS_H
#define CONTAINERUTILS_H

#include <functional>
#include "tracing/Logging.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework
{
namespace Plugin
{

class ContainerUtils
{

public:
    enum Namespace {
        NetworkNamespace = 0x01,
        MountNamespace = 0x02,
        IpcNamespace = 0x04,
        PidNamespace = 0x08,
        UserNamespace = 0x10,
        UtsNamespace = 0x20
    };

    static uint32_t getContainerIpAddress(const std::string &containerId);

private:
    static bool nsEnterImpl(const std::string &containerId, std::string type,
                            const std::function<void()> &func);
};

}
}
#endif // CONTAINERUTILS_H
