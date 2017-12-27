#include <asm/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>
#include <netinet/in.h>
 
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
 
typedef int (*msg_handlers)(struct sockaddr_nl *nl, struct nlmsghdr *msg);
#define IFLA_WIRELESS    11
#define NL_MAX_MSG_LEN   4096

int open_netlink()
{
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    struct sockaddr_nl addr;

    memset((void *)&addr, 0, sizeof(addr));

    if (sock<0)
        return sock;
    addr.nl_family = PF_NETLINK;
//    addr.nl_pid = getpid();
    addr.nl_pid = 0;
    addr.nl_groups = RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR;
//    addr.nl_groups = RTMGRP_LINK;
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        return -1;
    return sock;
}
 
/* recv a netlink msg and show the nlmsg_flags fields of  struct nlmsghdr*/
void show_flags(int flag)
{
    switch(flag){
        case NLM_F_REQUEST:
            printf("nlmsg_flags is NLM_F_REQUEST\n");
            break;

        case NLM_F_ACK:
            printf("nlmsg_flags is NLM_F_ACK\n");
            break;

        case NLM_F_ECHO:
            printf("nlmsg_flags is NLM_F_ECHO\n");
            break;

        case NLM_F_MULTI:
            printf("nlmsg_flags is NLM_F_MULTI\n");
            break;

        case NLM_F_ROOT:
            printf("nlmsg_flags is NLM_F_ROOT\n");
            break;

        case NLM_F_MATCH:
            printf("nlmsg_flags is NLM_F_MATCH\n");
            break;

        case NLM_F_ATOMIC:
            printf("nlmsg_flags is NLM_F_ATOMIC\n");
            break;

        case NLM_F_DUMP:
            printf("nlmsg_flags is NLM_F_DUMP\n");
            break;
/*
        case NLM_F_REPLACE:
            printf("nlmsg_flags is NLM_F_REPLACE\n");
            break;

        case NLM_F_EXCL:
            printf("nlmsg_flags is NLM_F_EXCL\n");
            break;

        case NLM_F_CREATE:
            printf("nlmsg_flags is NLM_F_CREATE\n");
            break;
*/
        case NLM_F_APPEND:
            printf("nlmsg_flags is NLM_F_APPEND\n");
            break;

        default:
            printf("invalid nlmsg_flags\n");
    }
}

int read_event(int sockfd, msg_handlers msg_handler)
{
    int status, ret = 0;
    struct nlmsghdr *nlhdr = (struct nlmsghdr *)calloc(1, NL_MAX_MSG_LEN);
    struct sockaddr_nl snl;
    struct iovec iov;
    memset(&iov, 0, sizeof iov);
    iov.iov_base = nlhdr;
    iov.iov_len  = NL_MAX_MSG_LEN;
    struct msghdr msg;
    memset(&msg, 0, sizeof msg);
    msg.msg_name    = (void *)&snl;
    msg.msg_namelen = sizeof snl;
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;
    
    status = recvmsg(sockfd, &msg, MSG_DONTWAIT);
    if(status < 0)
    {
        /* Socket non-blocking so bail out once we have read everything */
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return ret;

        /* Anything else is an error */
        printf("read_netlink: Error recvmsg: %d\n", status);
        perror("read_netlink: Error: ");
        return status;
    }
        
    if(status == 0)
    {
        printf("read_netlink: EOF\n");
    }
    printf("recvmsg return\n");
    if(nlhdr->nlmsg_flags == NLM_F_MULTI)
        printf("Got a multi message\n");

    /* We need to handle more than one message per 'recvmsg' */
    for( ; NLMSG_OK(nlhdr, (unsigned int)status); nlhdr = NLMSG_NEXT(nlhdr, status))
    {
        printf("got1\n");

        /* Finish reading */
        if (nlhdr->nlmsg_type == NLMSG_DONE)
            return ret;

        /* Message is some kind of error */
        if (nlhdr->nlmsg_type == NLMSG_ERROR)
        {
            printf("read_netlink: Message is an error - decode TBD\n");
            return -1; // Error
        }

        show_flags(nlhdr->nlmsg_flags);

        /* Call message handler */
        if(msg_handler)
        {
            ret = (*msg_handler)(&snl, nlhdr);
            if(ret < 0)
            {
                printf("read_netlink: Message hander error %d\n", ret);
                return ret;
            }
        }
        else
        {
            printf("read_netlink: Error NULL message handler\n");
            return -1;
        }
    }

    return ret;
}
/*

                
----------------------------------------------------------
|           |           |          |        | another  | 
| nlmsghdr  | ifinfomsg |  rtattr  | paload | nlmsghdr | .....
----------------------------------------------------------
*msg        *ifi        *attr

*/
static int netlink_link_state(struct sockaddr_nl *nl, struct nlmsghdr *msg)
{
    int ifimsg_len, nlmsg_len, attrlen, rta_len;
    struct ifinfomsg *ifi;
    struct rtattr * attr;

    ifi = (struct ifinfomsg *)NLMSG_DATA(msg);
    ifimsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));
    nlmsg_len  = NLMSG_ALIGN(sizeof(struct nlmsghdr));
    attrlen = msg->nlmsg_len - nlmsg_len - ifimsg_len;
    if (attrlen < 0)
        return -1;

    attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);
    rta_len = RTA_ALIGN(sizeof(struct rtattr));

    printf("netlink_link_state: Link %s\n", 
           (ifi->ifi_flags & IFF_RUNNING)?"Up":"Down");

    while(RTA_OK(attr, attrlen)) {
        if(attr->rta_type == IFLA_WIRELESS)
            printf("Got IFLA_WIRELESS!!\n");
        if(attr->rta_type == IFLA_IFNAME)
            printf("interface name\n");

        attr = RTA_NEXT(attr, attrlen);
    }
/*    
    printf("netlink_link_state: family: %d\n", ifi->ifi_family);
    if(ifi->ifi_family == AF_INET)
    {
        printf("Link index: %d Flags: (0x%x) ", 
                ifi->ifi_index, ifi->ifi_flags);
         if(ifi->ifi_flags & IFF_UP)
             printf("IFF_UP ");
         if(ifi->ifi_flags & IFF_BROADCAST)
             printf("IFF_BROADCAST ");
         if(ifi->ifi_flags & IFF_DEBUG)
             printf("IFF_DEBUG ");
         if(ifi->ifi_flags & IFF_LOOPBACK)
             printf("IFF_LOOPBACK ");
         if(ifi->ifi_flags & IFF_POINTOPOINT)
             printf("IFF_POINTOPOINT ");
         if(ifi->ifi_flags & IFF_NOTRAILERS)
             printf("IFF_NOTRAILERS ");
         if(ifi->ifi_flags & IFF_RUNNING)
             printf("IFF_RUNNING ");
         if(ifi->ifi_flags & IFF_NOARP)
             printf("IFF_NOARP ");
         if(ifi->ifi_flags & IFF_PROMISC)
             printf("IFF_PROMISC ");
         if(ifi->ifi_flags & IFF_ALLMULTI)
             printf("IFF_ALLMULTI ");
         if(ifi->ifi_flags & IFF_MASTER)
             printf("IFF_MASTER ");
         if(ifi->ifi_flags & IFF_SLAVE)
             printf("IFF_SLAVE ");
         if(ifi->ifi_flags & IFF_MULTICAST)
             printf("IFF_MULTICAST ");
         if(ifi->ifi_flags & IFF_PORTSEL)
             printf("IFF_PORTSEL ");
         if(ifi->ifi_flags & IFF_AUTOMEDIA)
             printf("IFF_AUTOMEDIA ");
         printf("\n");        
    }
*/    
    return 0;
}

static int msg_handler(struct sockaddr_nl *nl, struct nlmsghdr *msg)
{
    switch (msg->nlmsg_type)
    {
        case RTM_NEWADDR:
            printf("msg_handler: RTM_NEWADDR\n");
            break;
        case RTM_DELADDR:
            printf("msg_handler: RTM_DELADDR\n");
            break;
        case RTM_NEWROUTE:
            printf("msg_handler: RTM_NEWROUTE\n");
            break;
        case RTM_DELROUTE:
            printf("msg_handler: RTM_DELROUTE\n");
            break;
        case RTM_NEWLINK:
            printf("msg_handler: RTM_NEWLINK\n");
            netlink_link_state(nl, msg);
            break;
        case RTM_DELLINK:
            printf("msg_handler: RTM_DELLINK\n");
            break;
        default:
            printf("msg_handler: Unknown netlink nlmsg_type %d\n",
                   msg->nlmsg_type);
            break;
    }
    return 0;
}


int main(int argc, char *argv[])
{
    printf("%d, %d\n", (int) NLMSG_ALIGN(sizeof(struct nlmsghdr)), sizeof(struct nlmsghdr));
    int nls = open_netlink();

    printf("Started watching:\n");
    if (nls < 0) {
        printf("Open Error!");
    }

    while (1)
    {
        read_event(nls, msg_handler);
        sleep(1);
    }
        
    return 0;
}

 