//
//  WebInspector.h
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#ifndef WEBINSPECTOR_H
#define WEBINSPECTOR_H

#include "Debugger.h"
#include <string>
#include <memory>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdio>

class WebInspector final : public Debugger
{
public:
    static std::shared_ptr<WebInspector> attach(const std::string &appId,
                                               const in_addr_t &appIpAddr,
                                               int debugPort);

    ~WebInspector() final;

    Type type() const override;
    bool isAttached() const override;
    int debugPort() const override;


private:
    WebInspector(const std::string &appId, int debugPort, const std::string &nfRuleComment);

private:
    const std::string mAppId;
    const int mDebugPort;
    const std::string mNetFilterCommentMatcher;
};

#endif // WEBINSPECTOR_H
