//
//  NetFilter.cpp
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#include "NetFilter.h"
#include "NetFilterUtils.h"
#include <list>
#include <stdarg.h>

extern "C" const char *iptc_strerror(int err);


/// Global lock, used to control access to the iptables ruleset
NetFilterLock NetFilter::mLock;


// -----------------------------------------------------------------------------
/*!
    \internal

    Exported logging function so can be used by C code in NetFilterUtils.c

 */
extern "C" void nfDebug(const char *format, ...)
{
    char message[256];
    va_list ap;

    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);
    LOGINFO("%s", message);
}

extern "C" void nfInfo(const char *format, ...)
{
    char message[256];
    va_list ap;

    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);
    LOGINFO("%s", message);
}

extern "C" void nfError(const char *format, ...)
{
    char message[256];
    va_list ap;

    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);
    LOGWARN("%s", message);
}

extern "C" void nfSysError(int error, const char *format, ...)
{
    char message[256];
    va_list ap;

    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);
    LOGWARN("%s (%d - %s)", message, error, iptc_strerror(error));
}


// -----------------------------------------------------------------------------
/*!
    \static

    Removes all rules in iptables that have matching comments files with
    \a commentMatch.

 */
void NetFilter::removeAllRulesMatchingComment(const std::string &commentMatch)
{
	/*
    qCInfo(netFilter) << "removing all iptables rules who's comments match" << commentMatch;

    std::lock_guard<NetFilterLock> locker(mLock);

    ::removeAllRulesMatchingComment(regexMatcher,
                                    const_cast<QRegExp*>(&commentMatch));
				    */
}

// -----------------------------------------------------------------------------
/*!
    \static

    Adds a rule to the filter INPUT and OUTPUT table to allow incoming
    connections.  This is equivalent of running the following iptables commands:

    \code

        iptables -I INPUT  -p <protocol> -m <protocol> --dport <port> \
                -m conntrack --ctstate NEW,ESTABLISHED \
                -m comment --comment "<comment>" -j ACCEPT

        iptables -I OUTPUT -p <protocol> -m <protocol> --sport <port> \
                -m conntrack --ctstate ESTABLISHED \
                -m comment --comment "<comment>" -j ACCEPT

    \endcode

 */
bool NetFilter::openExternalPort(in_port_t port, Protocol protocol,
                                 const std::string &comment)
{
    LOGINFO("opening netfilter hole for %s port %hu",
           (protocol == Protocol::Tcp) ? "tcp" :
           (protocol == Protocol::Udp) ? "udp" : "???",
           port);
    std::lock_guard<NetFilterLock> locker(mLock);

    return (::openExternalPort(port, static_cast<int>(protocol), comment.c_str()) == 0);
}

// -----------------------------------------------------------------------------
/*!
    Adds two rules for routing incoming requests on a given port to a container.

    The first rule sets up pre-routing so the incoming packets have their ip
    address and port number changed to match the container:

    \code
        iptables -t nat -A PREROUTING \
            ! -i dobby0 --source 0.0.0.0/0 --destination 0.0.0.0/0 -p <PROTOCOL> \
            -m <PROTOCOL> --dport <EXTERNAL_PORT> \
            -m comment --comment "<COMMENT>" \
            -j DNAT --to <CONTAINER_IP>:<CONTAINER_PORT>
    \endcode

    And the second rule enables forwarding from to the dobby0 bridge and then
    on into the container:

    \code
        iptables -t filter -I FORWARD 1 \
            ! -i dobby0 -o dobby0 --source 0.0.0.0/0 --destination <CONTAINER_IP> -p <PROTOCOL> \
            -m <PROTOCOL> --dport <CONTAINER_PORT> \
            -m comment --comment "<COMMENT>" \
            -j ACCEPT
    \endcode

 */
bool NetFilter::addContainerPortForwarding(const std::string &bridgeIface,
                                           Protocol protocol,
                                           uint16_t externalPort,
                                           const in_addr_t &containerIp,
                                           uint16_t containerPort,
                                           const std::string &comment)
{

    std::lock_guard<NetFilterLock> locker(mLock);

    return (::addContainerHolePunch(bridgeIface.c_str(),
                                    static_cast<int>(protocol),
                                    externalPort,
                                    containerIp, containerPort,
                                    comment.c_str()) == 0);
}

// -----------------------------------------------------------------------------
/*!
    \static

    Gets the set of currently hole punched port for a container with a given
    IP address.  Specifically this looks for a NAT PREROUTING rule, which has
    no source address filter, but forwards onto the containers IP address.

 */
std::list<NetFilter::PortForward> NetFilter::getContainerPortForwardList(const std::string &bridgeIface,
                                                                     const in_addr_t &containerIp)
{
    LOGINFO("finding NAT PREROUTING rules that forward to a container ip address %u", containerIp);
    std::unique_lock<NetFilterLock> locker(mLock);

    HolePunchEntry entries[32];
    size_t count = 32;

    ::getContainerHolePunchedPorts(bridgeIface.c_str(), containerIp, entries, &count);

    locker.unlock();

    std::list<PortForward> values;
    for (size_t i = 0; i < count; i++)
    {
        switch (entries[i].protocol)
        {
            case IPPROTO_TCP:
                values.push_back(PortForward{ Protocol::Tcp,
                                           entries[i].publicPort,
                                           entries[i].containerPort });
                break;
            case IPPROTO_UDP:
                values.push_back(PortForward{ Protocol::Udp,
                                           entries[i].publicPort,
                                           entries[i].containerPort });
                break;

            default:
                LOGWARN("unknown hole punch protocol (%d)", entries[i].protocol);
        }
    }

    return values;
}
