//
//  WebInspector.cpp
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#include "WebInspector.h"

#include "UtilsLogging.h"
#include "tracing/Logging.h"

#include "NetFilter.h"

#include <unistd.h>

// -----------------------------------------------------------------------------
/*!
    \class WebInspector

    Object that represents a WebInspector attachment to an app.  This is just
    a firewall forwarding rule to allow external apps to connect to the
    webinspector port inside a container.

    On destruction the firewall forwarding rule is removed.

 */


std::shared_ptr<WebInspector> WebInspector::attach(const std::string &appId,
                                                  const in_addr_t &appIpAddr,
                                                  int debugPort)
{
    // setup port forward from external port to container port 22222
    const std::string comment = "appsservice-gatewayd:" + appId + ":webinspector";
    in_addr_t ip = appIpAddr;
    if (!NetFilter::addContainerPortForwarding("dobby0",
                                               NetFilter::Protocol::Tcp,
                                               debugPort, ip, 22222,
                                               comment))
    {
        LOGWARN("failed to setup port forwarding for webinspector");
        return nullptr;
    }

    return std::shared_ptr<WebInspector>(new WebInspector(appId, debugPort, comment));
}

WebInspector::WebInspector(const std::string &appId, int debugPort, const std::string &nfRuleComment)
    : mAppId(appId)
    , mDebugPort(debugPort)
    , mNetFilterCommentMatcher(nfRuleComment)
{
}

WebInspector::~WebInspector()
{
    LOGINFO("detaching webinspector from %s", mAppId.c_str());

    NetFilter::removeAllRulesMatchingComment(mNetFilterCommentMatcher);
}

Debugger::Type WebInspector::type() const
{
    return Type::WebInspector;
}

bool WebInspector::isAttached() const
{
    return true;
}

int WebInspector::debugPort() const
{
    return mDebugPort;
}

