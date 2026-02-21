//
//  NetFilter.h
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#ifndef NETFILTER_H
#define NETFILTER_H

#include "NetFilterLock.h"

#include <string>
#include <netdb.h>
#include <sys/socket.h>
#include <list>

class NetFilter
{
public:
    enum class Protocol
    {
        Tcp = IPPROTO_TCP,
        Udp = IPPROTO_UDP
    };

    static void removeAllRulesMatchingComment(const std::string &commentMatch);

    static bool openExternalPort(in_port_t port, Protocol protocol,
                                 const std::string &comment);

    static bool addContainerPortForwarding(const std::string &bridgeIface,
                                           Protocol protocol,
                                           uint16_t externalPort,
                                           const in_addr_t &containerIp,
                                           uint16_t containerPort,
                                           const std::string &comment);

    struct PortForward
    {
        Protocol protocol;
        uint16_t externalPort;
        uint16_t containerPort;
    };

    static std::list<NetFilter::PortForward> getContainerPortForwardList(const std::string &bridgeIface,
                                                          const in_addr_t &containerIp = 0);

private:
    static NetFilterLock mLock;
};

#endif // NETFILTER_H
