//
//  NetFilterUtils.c
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#include "NetFilterUtils.h"

#include <xtables.h>
#include <libiptc.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_tcpudp.h>
#include <linux/netfilter/xt_multiport.h>
#include <linux/netfilter/xt_comment.h>
#include <linux/netfilter/xt_state.h>
#include <linux/netfilter/xt_conntrack.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/nf_nat.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>


extern void nfDebug(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void nfInfo(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void nfError(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void nfSysError(int error, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));


struct xt_dnat_info
{
    struct xt_entry_target target;
    struct nf_nat_ipv4_multi_range_compat range;
};


// -----------------------------------------------------------------------------
/*!
    \internal

    Returns the comment string for the given iptables \a entry.

    If the entry doesn't have a comment then returns a null QByteArray object.

 */
static bool getEntryComment(const struct ipt_entry *entry, char *buf)
{
    // check there are any matches
    if (entry->target_offset <= 0)
        return false;

    // the size of a comment struct
    const size_t minCommentSize = sizeof(struct xt_entry_match) +
                                  XT_ALIGN(sizeof(struct xt_comment_info));

    // loop through all the matches
    const uint8_t *ptr = (const uint8_t*)(entry) + sizeof(struct ipt_entry);
    const uint8_t * const end = (const uint8_t*)(entry) + entry->target_offset;
    while (ptr < end)
    {
        const struct xt_entry_match *match = (const struct xt_entry_match*)ptr;
        ptr += match->u.match_size;

        const char *matchName = match->u.user.name;
        if (!matchName || (matchName[0] == '\0'))
            continue;

        // ignore non-comment matches
        if (strcmp(matchName, "comment") != 0)
            continue;

        // sanity check the size of the structure
        if (match->u.user.match_size < minCommentSize)
        {
            nfError("match structure for '%s' is too small (size: %hu, expected: %zu)",
                    matchName, match->u.user.match_size, minCommentSize);
            continue;
        }

        const struct xt_comment_info *info = (const struct xt_comment_info*)match->data;

        // just to ensure it's null terminated (needed?)
        memcpy(buf, info->comment, XT_MAX_COMMENT_LEN);
        buf[XT_MAX_COMMENT_LEN] = '\0';

        return true;
    }

    // didn't find any comment match fields
    return false;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Returns the input interface filter values from the rule entry.

 */
static bool getEntryInterface(const struct ipt_entry *entry, bool in,
                              char *ifaceName, bool *inverted)
{
    if (in)
    {
        if (ifaceName)
        {
            strncpy(ifaceName, entry->ip.iniface, IF_NAMESIZE);
            ifaceName[IF_NAMESIZE] = '\0';
        }

        if (inverted)
            *inverted = (entry->ip.invflags & IPT_INV_VIA_IN) ? true : false;
    }
    else
    {
        if (ifaceName)
        {
            strncpy(ifaceName, entry->ip.outiface, IF_NAMESIZE);
            ifaceName[IF_NAMESIZE] = '\0';
        }

        if (inverted)
            *inverted = (entry->ip.invflags & IPT_INV_VIA_OUT) ? true : false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Returns the ip address and port of the DNAT target rule.  If the rule
    doesn't have a DNA target then \c false is returned.

 */
static bool getEntryDNATDestination(const struct ipt_entry *entry,
                                    in_addr_t *destAddr, in_port_t *destPort)
{
    const ssize_t minTargetSize = sizeof(struct xt_entry_target) +
                                  XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));

    const ssize_t targetSize = (ssize_t)entry->next_offset - (ssize_t)entry->target_offset;
    if (targetSize < minTargetSize)
    {
        return false;
    }

    const uint8_t *ptr = (const uint8_t*)(entry) + entry->target_offset;

    struct xt_entry_target *targetEntry = (struct xt_entry_target*)(ptr);
    if (strcmp(targetEntry->u.user.name, "DNAT") != 0)
    {
        return false;
    }

    const struct nf_nat_ipv4_multi_range_compat *info = (const struct nf_nat_ipv4_multi_range_compat*)targetEntry->data;
    if (info->rangesize != 1)
    {
        return false;
    }

    const struct nf_nat_ipv4_range *range = &(info->range[0]);
    if ((range->min_ip != range->max_ip) || !(range->flags & NF_NAT_RANGE_MAP_IPS))
    {
        return false;
    }

    if (destAddr)
        *destAddr = be32toh(range->min_ip);
    if (destPort)
        *destPort = be16toh(range->min.tcp.port);

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Iterates through the matches on an entry to find either a TCP or UDP
    match and then returns the source and / or destination port numbers.

    \warning This currently ignores range or inverted matches.

 */
bool getEntryTcpUdpMatchPort(const struct ipt_entry *entry, int *protocol,
                             in_port_t *sourcePort, in_port_t *destPort)
{
    // check there are any matches
    if (entry->target_offset <= 0)
        return false;

    // loop through all the matches
    const uint8_t *ptr = (const uint8_t*)(entry) + sizeof(struct ipt_entry);
    const uint8_t * const end = (const uint8_t*)(entry) + entry->target_offset;
    while (ptr < end)
    {
        const struct xt_entry_match *match = (const struct xt_entry_match*)ptr;
        ptr += match->u.match_size;

        const char *matchName = match->u.user.name;
        if (!matchName || (matchName[0] == '\0'))
            continue;

        if (strcmp(matchName, "tcp") == 0)
        {
            if (protocol)
                *protocol = IPPROTO_TCP;

            const struct xt_tcp *info = (const struct xt_tcp*)match->data;
            if (sourcePort)
                *sourcePort = info->spts[0];
            if (destPort)
                *destPort = info->dpts[0];

            return true;
        }
        else if (strcmp(matchName, "udp") == 0)
        {
            if (protocol)
                *protocol = IPPROTO_UDP;

            const struct xt_udp *info = (const struct xt_udp*)match->data;
            if (sourcePort)
                *sourcePort = info->spts[0];
            if (destPort)
                *destPort = info->dpts[0];

            return true;
        }
    }

    // didn't find any tcp or udp match fields
    return false;
}

// -----------------------------------------------------------------------------
/*!
    Removes all rules in iptables that have matching comments files with
    \a commentMatch.

 */
int removeAllRulesMatchingComment(NfCommentMatcher matcher, void *userData)
{
    static const char *tableNames[] = { "filter", "nat", NULL };
    for (const char **tableName = tableNames; *tableName != NULL; tableName++)
    {
        nfDebug("processing table '%s'", *tableName);

        // open the table object
        struct xtc_handle *table = iptc_init(*tableName);
        if (!table)
        {
            nfError("failed to open table '%s'", *tableName);
            continue;
        }

        // counts the number of changes make in the table
        unsigned int changes = 0;

        // loop through all chains
        const char *chainName = iptc_first_chain(table);
        while (chainName)
        {
            nfDebug("processing chain '%s'", chainName);

            // process all the rules in the chain
            unsigned int index = 0;
            const struct ipt_entry *entry = iptc_first_rule(chainName, table);
            while (entry)
            {
                // check if the entry matches one in the list we're supposed to
                // be deleting
                char comment[XT_MAX_COMMENT_LEN + 1] = { '\0' };
                getEntryComment(entry, comment);

                // move to the next entry so if the rule is deleted the iterator
                // is still valid
                entry = iptc_next_rule(entry, table);

                // skip over if doesn't match
                if ((strlen(comment) == 0) || (matcher(comment, userData) != 0))
                {
                    index++;
                    continue;
                }

                nfInfo("deleting rule at index %d 'from %s:%s'",
                            index, *tableName, chainName);

                // it is safe to delete the entry we're currently
                // iterating on
                if (iptc_delete_num_entry(chainName, index, table))
                {
                    changes++;
                }
                else
                {
                    nfSysError(errno, "failed to delete rule at index %u in '%s:%s'",
                                    index, *tableName, chainName);
                    index++;
                }
             }

            // move to next chain
            chainName = iptc_next_chain(table);
        }

        // commit the changes (if any)
        if (changes && !iptc_commit(table))
        {
            nfSysError(errno, "failed to commit changes to table '%s'",
                            *tableName);
        }

        // free the table
        iptc_free(table);
    }

    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Adds a UDP or TCP match rule with given port ranges.  Equivalent of the
    following on iptables cmdline:

    \code
        iptables ... -m <tcp|udp> --sport <srcPortMin>:<srcPortMax> --dport <dstPortMin>:<dstPortMax>
    \endcode

 */
static ssize_t appendTcpUdpMatch(struct xt_entry_match *entry, size_t space,
                                 int protocol,
                                 in_port_t srcPortMin, in_port_t srcPortMax,
                                 in_port_t dstPortMin, in_port_t dstPortMax)
{
    if (protocol == IPPROTO_TCP)
    {
        const ssize_t size = sizeof(struct xt_entry_match) + XT_ALIGN(sizeof(struct xt_tcp));
        if (size > space)
        {
            nfError("not enough space to write tcp match entry");
            return -1;
        }

        memset(entry, 0x00, size);
        strcpy(entry->u.user.name, "tcp");
        entry->u.user.match_size = size;

        struct xt_tcp *matchTcp = (struct xt_tcp *) entry->data;
        matchTcp->spts[0] = srcPortMin;
        matchTcp->spts[1] = srcPortMax;
        matchTcp->dpts[0] = dstPortMin;
        matchTcp->dpts[1] = dstPortMax;

        return size;
    }
    else if (protocol == IPPROTO_UDP)
    {
        const ssize_t size = sizeof(struct xt_entry_match) + XT_ALIGN(sizeof(struct xt_udp));
        if (size > space)
        {
            nfError("not enough space to write udp match entry");
            return -1;
        }

        memset(entry, 0x00, size);
        strcpy(entry->u.user.name, "udp");
        entry->u.user.match_size = size;

        struct xt_udp *matchUdp = (struct xt_udp *) entry->data;
        matchUdp->spts[0] = srcPortMin;
        matchUdp->spts[1] = srcPortMax;
        matchUdp->dpts[0] = dstPortMin;
        matchUdp->dpts[1] = dstPortMax;

        return size;
    }
    else
    {
        nfError("invalid protocol value");
        return -1;
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Adds a conntrack state match rule.  Equivalent of the following on iptables
    cmdline:

    \code
        iptables ... -m state --state <states>
    \endcode

 */
static ssize_t appendStateMatch(struct xt_entry_match *entry, size_t space,
                                uint16_t states)
{
    const ssize_t size = sizeof(struct xt_entry_match) + XT_ALIGN(sizeof(struct xt_conntrack_mtinfo3));
    if (size > space)
    {
        nfError("not enough space to write conntrack match entry");
        return -1;
    }

    memset(entry, 0x00, size);
    strcpy(entry->u.user.name, "conntrack");
    entry->u.user.match_size = size;
    entry->u.user.revision = 3;

    struct xt_conntrack_mtinfo3 *info = (struct xt_conntrack_mtinfo3 *) entry->data;
    info->match_flags = XT_CONNTRACK_STATE | XT_CONNTRACK_STATE_ALIAS;
    info->state_mask = states;

    return size;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Adds a comment match rule.  Equivalent of the following on iptables cmdline:

    \code
        iptables ... -m comment --comment "<comment>"
    \endcode

 */
static ssize_t appendCommentMatch(struct xt_entry_match *entry, size_t space,
                                  const char *comment)
{
    const ssize_t size = sizeof(struct xt_entry_match) + XT_ALIGN(sizeof(struct xt_comment_info));
    if (size > space)
    {
        nfError("not enough space to write comment match entry");
        return -1;
    }

    memset(entry, 0x00, size);
    strcpy(entry->u.user.name, "comment");
    entry->u.user.match_size = size;

    struct xt_comment_info *info = (struct xt_comment_info *) entry->data;
    strncpy(info->comment, comment, XT_MAX_COMMENT_LEN - 1);

    return size;
}

// -----------------------------------------------------------------------------
/*!

    \code

        iptables -I INPUT  -p <protocol> \
                 -m <protocol> --dport <port> \
                 -m state --state NEW,ESTABLISHED \
                 -m comment --comment "<comment>" \
                 -j ACCEPT

    \endcode

 */
static struct ipt_entry *createInputFilterRule(in_port_t port, int protocol, const char *comment)
{
    const size_t ruleBufSize = 1024;
    uint8_t *ruleBuf = calloc(1, ruleBufSize);

    uint8_t *const start = ruleBuf;
    uint8_t *const end = start + ruleBufSize;
    uint8_t *ptr = start;

    // populate the standard fields first
    struct ipt_entry *header = (struct ipt_entry*)ptr;
    {
        // protocol
        header->ip.proto = protocol;
    }

    ptr += sizeof(struct ipt_entry);

    // populate the matches
    {
        ssize_t matchSize;

        // -m protocol
        matchSize = appendTcpUdpMatch((struct xt_entry_match*)ptr, (end - ptr),
                                      protocol, 0, UINT16_MAX, port, port);
        if (matchSize < 0)
        {
            free(ruleBuf);
            return NULL;
        }
        ptr += matchSize;


        // -m state
        matchSize = appendStateMatch((struct xt_entry_match*)ptr, (end - ptr),
                                     XT_CONNTRACK_STATE_BIT(IP_CT_NEW) |
                                     XT_CONNTRACK_STATE_BIT(IP_CT_ESTABLISHED));
        if (matchSize < 0)
        {
            free(ruleBuf);
            return NULL;
        }
        ptr += matchSize;


        // -m comment
        if (comment)
        {
            matchSize = appendCommentMatch((struct xt_entry_match *)ptr,
                                           (end - ptr), comment);
            if (matchSize < 0)
            {
                free(ruleBuf);
                return NULL;
            }
            ptr += matchSize;
        }
    }

    // set the size of the matches by setting the target offset
    header->target_offset = ptr - start;

    // write the target entry
    {
        struct xt_entry_target *targetEntry = (struct xt_entry_target*)ptr;
        const size_t targetSize = XT_ALIGN(sizeof(struct xt_standard_target));
        if (targetSize > (end - ptr))
        {
            nfError("no space in entry to write target");
            free(ruleBuf);
            return NULL;
        }

        targetEntry->u.user.target_size = targetSize;
        strcpy(targetEntry->u.user.name, IPTC_LABEL_ACCEPT);

        // update the pointer to the end of the target
        ptr += targetSize;
        // nfDebug("ptr=%p  end=%p  (space %zu)", ptr, end, (end - ptr));
    }

    // set the size of the matches + target by setting the next offset
    header->next_offset = ptr - start;

    return header;
}

// -----------------------------------------------------------------------------
/*!

    \code

         iptables -I OUTPUT -p <protocol> \
                 -m <protocol> --sport <port> \
                 -m state --state ESTABLISHED \
                 -m comment --comment "<comment>" \
                 -j ACCEPT

    \endcode

 */
static struct ipt_entry *createOutputFilterRule(in_port_t port, int protocol, const char *comment)
{
    const size_t ruleBufSize = 1024;
    uint8_t *ruleBuf = calloc(1, ruleBufSize);

    uint8_t *const start = ruleBuf;
    uint8_t *const end = start + ruleBufSize;
    uint8_t *ptr = start;

    // populate the standard fields first
    struct ipt_entry *header = (struct ipt_entry*)ptr;
    {
        // protocol
        header->ip.proto = protocol;

        ptr += sizeof(struct ipt_entry);
    }

    // populate the matches
    {
        ssize_t matchSize;

        // -m protocol
        matchSize = appendTcpUdpMatch((struct xt_entry_match*)ptr, (end - ptr),
                                      protocol, port, port, 0, UINT16_MAX);
        if (matchSize < 0)
        {
            free(ruleBuf);
            return NULL;
        }
        ptr += matchSize;


        // -m state
        matchSize = appendStateMatch((struct xt_entry_match*)ptr, (end - ptr),
                                     XT_CONNTRACK_STATE_BIT(IP_CT_ESTABLISHED));
        if (matchSize < 0)
        {
            free(ruleBuf);
            return NULL;
        }
        ptr += matchSize;


        // -m comment
        if (comment)
        {
            matchSize = appendCommentMatch((struct xt_entry_match *)ptr,
                                           (end - ptr), comment);
            if (matchSize < 0)
            {
                free(ruleBuf);
                return NULL;
            }
            ptr += matchSize;
        }
    }

    // set the size of the matches by setting the target offset
    header->target_offset = ptr - start;

    // write the target entry
    {
        struct xt_entry_target *targetEntry = (struct xt_entry_target*)ptr;
        const size_t targetSize = XT_ALIGN(sizeof(struct xt_standard_target));
        if (targetSize > (end - ptr))
        {
            nfError("no space in entry to write target");
            free(ruleBuf);
            return NULL;
        }

        targetEntry->u.user.target_size = targetSize;
        strcpy(targetEntry->u.user.name, IPTC_LABEL_ACCEPT);

        // update the pointer to the end of the target
        ptr += targetSize;
    }

    // set the size of the matches + target by setting the target offset
    header->next_offset = ptr - start;

    return header;
}

// -----------------------------------------------------------------------------
/*!
    Adds a rule to the filter INPUT and OUTPUT table to allow incoming
    connections.  This is equivalent of running the following iptables commands:

    \code

        iptables -I INPUT  -p <PROTOCOL> -m <PROTOCOL> --dport <PORT> \
                 -m state --state NEW,ESTABLISHED \
                 -m comment --comment "<COMMENT>" -j ACCEPT

        iptables -I OUTPUT -p <PROTOCOL> -m <PROTOCOL> --sport <PORT> \
                 -m state --state ESTABLISHED \
                 -m comment --comment "<COMMENT>" -j ACCEPT

    \endcode

 */
int openExternalPort(in_port_t port, int protocol, const char *comment)
{
    static const xt_chainlabel INPUT = "INPUT";
    static const xt_chainlabel OUTPUT = "OUTPUT";

    // open the filter table object
    struct xtc_handle *table = iptc_init("filter");
    if (!table)
    {
        nfError("failed to open table 'filter'");
        return -1;
    }


    struct ipt_entry *inputEntry = createInputFilterRule(port, protocol, comment);
    if (!inputEntry)
    {
        nfError("failed to create input filter rule'");
        iptc_free(table);
        return -1;
    }
    if (!iptc_insert_entry(INPUT, inputEntry, 1, table))
    {
        nfSysError(errno, "failed to insert rule into table 'filter'");
        free(inputEntry);
        iptc_free(table);
        return -1;
    }

    free(inputEntry);


    struct ipt_entry *outputEntry = createOutputFilterRule(port, protocol, comment);
    if (!outputEntry)
    {
        nfError("failed to create input filter rule'");
        iptc_free(table);
        return -1;
    }
    if (!iptc_insert_entry(OUTPUT, outputEntry, 1, table))
    {
        nfSysError(errno, "failed to insert rule into table 'filter'");
        free(outputEntry);
        iptc_free(table);
        return -1;
    }

    free(outputEntry);


    // commit the changes (if any)
    if (!iptc_commit(table))
    {
        nfSysError(errno, "failed to commit changes to table 'filter'");
        iptc_free(table);
        return -1;
    }

    // free the table
    iptc_free(table);
    return 0;
}

// -----------------------------------------------------------------------------
/*!

    \code

        iptables -t nat -A PREROUTING \
            ! -i dobby0 --source 0.0.0.0/0 --destination 0.0.0.0/0 -p <PROTOCOL> \
            -m <PROTOCOL> --dport <EXTERNAL_PORT> \
            -m comment --comment "<COMMENT>" \
            -j DNAT --to <CONTAINER_IP>:<CONTAINER_PORT>

    \endcode

 */
static struct ipt_entry *createDNATRule(const char *bridgeIface, int protocol,
                                        in_port_t externalPort, in_addr_t containerIp,
                                        in_port_t containerPort, const char *comment)
{
    const size_t ruleBufSize = 1024;
    uint8_t *ruleBuf = calloc(1, ruleBufSize);

    uint8_t *const start = ruleBuf;
    uint8_t *const end = start + ruleBufSize;
    uint8_t *ptr = start;

    // populate the standard fields first
    struct ipt_entry *header = (struct ipt_entry*)ptr;
    {
        // protocol
        header->ip.proto = protocol;

        // in interface
        size_t len = strnlen(bridgeIface, IFNAMSIZ - 1);
        memcpy(header->ip.iniface, bridgeIface, len);
        memset(header->ip.iniface_mask, 0xff, len);
        header->ip.invflags |= IPT_INV_VIA_IN;

        ptr += sizeof(struct ipt_entry);
    }

    // populate the matches
    {
        ssize_t matchSize;

        // -m protocol
        matchSize = appendTcpUdpMatch((struct xt_entry_match*)ptr, (end - ptr),
                                      protocol, 0, UINT16_MAX, externalPort, externalPort);
        if (matchSize < 0)
        {
            free(ruleBuf);
            return NULL;
        }
        ptr += matchSize;


        // -m comment
        if (comment)
        {
            matchSize = appendCommentMatch((struct xt_entry_match *)ptr,
                                           (end - ptr), comment);
            if (matchSize < 0)
            {
                free(ruleBuf);
                return NULL;
            }
            ptr += matchSize;
        }
    }

    // set the size of the matches by setting the target offset
    header->target_offset = ptr - start;

    // write the DNAT target entry
    {
        struct xt_entry_target *targetEntry = (struct xt_entry_target*)ptr;
        const size_t targetSize = sizeof(struct xt_entry_target) +
                                  XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));
        if (targetSize > (end - ptr))
        {
            nfError("no space in entry to write target");
            free(ruleBuf);
            return NULL;
        }

        memset(targetEntry, 0x00, targetSize);
        targetEntry->u.user.target_size = targetSize;
        strncpy(targetEntry->u.user.name, "DNAT", XT_EXTENSION_MAXNAMELEN - 1);

        struct nf_nat_ipv4_multi_range_compat *info = (struct nf_nat_ipv4_multi_range_compat *)targetEntry->data;

        info->rangesize = 1;
        struct nf_nat_ipv4_range *range = &(info->range[0]);

        range->min_ip = range->max_ip = htobe32(containerIp);
        range->flags |= NF_NAT_RANGE_MAP_IPS;

        if (protocol == IPPROTO_TCP)
            range->min.tcp.port = range->max.tcp.port = htobe16(containerPort);
        else
            range->min.udp.port = range->max.udp.port = htobe16(containerPort);
        range->flags |= NF_NAT_RANGE_PROTO_SPECIFIED;

        // update the pointer to the end of the target
        ptr += targetSize;
    }

    // set the size of the matches + target by setting the target offset
    header->next_offset = ptr - start;

    return header;
}

// -----------------------------------------------------------------------------
/*!

    \code

        iptables -t filter -I FORWARD 1 \
            ! -i <BRIDGE_IFACE> -o <BRIDGE_IFACE> --source 0.0.0.0/0 --destination <CONTAINER_IP> -p <PROTOCOL> \
            -m <PROTOCOL> --dport <CONTAINER_PORT> \
            -m comment --comment "<COMMENT>" \
            -j ACCEPT

    \endcode

 */
static struct ipt_entry *createForwardingRule(const char *bridgeIface, int protocol,
                                              in_addr_t containerIp, in_port_t containerPort,
                                              const char *comment)
{
    const size_t ruleBufSize = 1024;
    uint8_t *ruleBuf = calloc(1, ruleBufSize);

    uint8_t *const start = ruleBuf;
    uint8_t *const end = start + ruleBufSize;
    uint8_t *ptr = start;

    // populate the standard fields first
    struct ipt_entry *header = (struct ipt_entry*)ptr;
    {
        // protocol
        header->ip.proto = protocol;

        // destination IP
        header->ip.dst.s_addr = htonl(containerIp);
        header->ip.dmsk.s_addr = 0xffffffff;

        // in interface
        size_t len = strnlen(bridgeIface, IFNAMSIZ - 1);
        memcpy(header->ip.iniface, bridgeIface, len);
        memset(header->ip.iniface_mask, 0xff, len);
        header->ip.invflags |= IPT_INV_VIA_IN;

        // out interface
        memcpy(header->ip.outiface, bridgeIface, len);
        memset(header->ip.outiface_mask, 0xff, len);

        ptr += sizeof(struct ipt_entry);
    }

    // populate the matches
    {
        ssize_t matchSize;

        // -m protocol
        matchSize = appendTcpUdpMatch((struct xt_entry_match*)ptr, (end - ptr),
                                      protocol, 0, UINT16_MAX, containerPort, containerPort);
        if (matchSize < 0)
        {
            free(ruleBuf);
            return NULL;
        }
        ptr += matchSize;


        // -m comment
        if (comment)
        {
            matchSize = appendCommentMatch((struct xt_entry_match *)ptr,
                                           (end - ptr), comment);
            if (matchSize < 0)
            {
                free(ruleBuf);
                return NULL;
            }
            ptr += matchSize;
        }
    }

    // set the size of the matches by setting the target offset
    header->target_offset = ptr - start;

    // write the target entry
    {
        struct xt_entry_target *targetEntry = (struct xt_entry_target*)ptr;
        const size_t targetSize = XT_ALIGN(sizeof(struct xt_standard_target));
        if (targetSize > (end - ptr))
        {
            nfError("no space in entry to write target");
            free(ruleBuf);
            return NULL;
        }

        targetEntry->u.user.target_size = targetSize;
        strcpy(targetEntry->u.user.name, IPTC_LABEL_ACCEPT);

        // update the pointer to the end of the target
        ptr += targetSize;
    }

    // set the size of the matches + target by setting the target offset
    header->next_offset = ptr - start;

    return header;
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
int addContainerHolePunch(const char *bridgeIface, int protocol,
                          in_port_t externalPort, in_addr_t containerIp,
                          in_port_t containerPort, const char *comment)
{
    static const xt_chainlabel PREROUTING = "PREROUTING";
    static const xt_chainlabel FORWARD = "FORWARD";

    {
        // open the nat table object
        struct xtc_handle *natTable = iptc_init("nat");
        if (!natTable)
        {
            nfError("failed to open table 'nat'");
            return -1;
        }

        struct ipt_entry *natEntry = createDNATRule(bridgeIface, protocol,
                                                    externalPort,
                                                    containerIp, containerPort,
                                                    comment);
        if (!natEntry)
        {
            nfError("failed to create PREROUTING nat rule'");
            iptc_free(natTable);
            return -1;
        }
        if (!iptc_insert_entry(PREROUTING, natEntry, 1, natTable))
        {
            nfSysError(errno, "failed to insert rule into table 'nat'");
            free(natEntry);
            iptc_free(natTable);
            return -1;
        }

        free(natEntry);

        // commit the changes (if any)
        if (!iptc_commit(natTable))
        {
            nfSysError(errno, "failed to commit changes to table 'nat'");
            iptc_free(natTable);
            return -1;
        }

        // free the table
        iptc_free(natTable);
    }

    {
        // open the filter table object
        struct xtc_handle *filterTable = iptc_init("filter");
        if (!filterTable)
        {
            nfError("failed to open table 'filter'");
            return -1;
        }

        struct ipt_entry *forwardEntry = createForwardingRule(bridgeIface, protocol,
                                                              containerIp, containerPort,
                                                              comment);
        if (!forwardEntry)
        {
            nfError("failed to create FORWARD filter rule'");
            iptc_free(filterTable);
            return -1;
        }
        if (!iptc_insert_entry(FORWARD, forwardEntry, 1, filterTable))
        {
            nfSysError(errno, "failed to insert FORWARD rule into table 'filter'");
            free(forwardEntry);
            iptc_free(filterTable);
            return -1;
        }

        free(forwardEntry);

        // commit the changes (if any)
        if (!iptc_commit(filterTable))
        {
            nfSysError(errno, "failed to commit changes to table 'filter'");
            iptc_free(filterTable);
            return -1;
        }

        // free the table
        iptc_free(filterTable);
    }

    return 0;
}

// -----------------------------------------------------------------------------
/*!
    Gets the set of currently hole punched port for a container with a given
    IP address.  Specifically this looks for a NAT PREROUTING rule, which has
    no source address filter, but forwards onto the containers IP address.

    \a containerIp can be 0.0.0.0, in which case it gets the entries for all
    containers.

 */
int getContainerHolePunchedPorts(const char *bridgeIface, in_addr_t containerIp,
                                 struct HolePunchEntry *entries, size_t *size)
{
    const size_t maxEntries = *size;
    size_t nEntries = 0;
    *size = nEntries;

    // open the table object
    struct xtc_handle *table = iptc_init("nat");
    if (!table)
    {
        nfError("failed to open table 'nat'");
        return -1;
    }

    // loop through all chains
    const char *chainName = iptc_first_chain(table);
    while (chainName)
    {
        nfDebug("processing chain '%s' in NAT table", chainName);

        if (strcmp(chainName, "PREROUTING") == 0)
        {
            // process all the rules in the chain
            const struct ipt_entry *entry = iptc_first_rule(chainName, table);
            for (; entry; entry = iptc_next_rule(entry, table))
            {
                // get the in interface and check it matches !<bridgeIface>
                char ifaceName[IF_NAMESIZE + 1] = {'\0'};
                bool inverted = false;
                if (!getEntryInterface(entry, 1, ifaceName, &inverted) ||
                    (strcmp(ifaceName, bridgeIface) != 0) || !inverted)
                {
                    continue;
                }

                // get the DNAT destination
                in_addr_t destAddr;
                in_port_t destPort;
                if (!getEntryDNATDestination(entry, &destAddr, &destPort) ||
                    ((containerIp != 0) && (destAddr != containerIp)))
                {
                    continue;
                }

                // get the TCP or UDP match filter port number
                int matchProtocol;
                in_port_t matchPort;
                if (!getEntryTcpUdpMatchPort(entry, &matchProtocol, NULL, &matchPort) ||
                    ((matchProtocol != IPPROTO_TCP) && (matchProtocol != IPPROTO_UDP)))
                {
                    continue;
                }

                // store the match
                if (nEntries < maxEntries)
                {
                    entries[nEntries].protocol = matchProtocol;
                    entries[nEntries].publicPort = matchPort;
                    entries[nEntries].containerPort = destPort;
                    nEntries++;
                }

            }
        }

        // move to next chain
        chainName = iptc_next_chain(table);
    }

    // free the table
    iptc_free(table);

    // set the number of entries found
    *size = nEntries;

    return 0;

}
