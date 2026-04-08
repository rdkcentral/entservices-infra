//
//  NetFilterUtils.h
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#ifndef NETFILTERUTILS_H
#define NETFILTERUTILS_H

#include <stdbool.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*NfCommentMatcher)(const char*, void*);
int removeAllRulesMatchingComment(NfCommentMatcher matcher, void *userData);

int openExternalPort(in_port_t port, int protocol, const char *comment);

int addContainerHolePunch(const char *bridgeIface, int protocol,
                          in_port_t externalPort, in_addr_t containerIp,
                          in_port_t containerPort, const char *comment);

struct HolePunchEntry
{
    int protocol;
    in_port_t publicPort;    // the port mapped outside the container
    in_port_t containerPort; // the destination port inside the container
};

int getContainerHolePunchedPorts(const char *bridgeIface, in_addr_t containerIp,
                                 struct HolePunchEntry *entries, size_t *size);

#ifdef __cplusplus
}
#endif


#endif // NETFILTERUTILS_H
