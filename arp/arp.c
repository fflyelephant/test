#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#define MAC_BROADCAST_ADDR (uint8_t *)"/xff/xff/xff/xff/xff/xff"
#define ARP_REQUEST 1
#define ARP_REPLY 2
struct arpMsg {
	struct ethhdr ethhdr;	 	/* 链路层头部 */
	u_short htype;				/* 硬件类型 (ARPHRD_ETHER=1 表示以太网) */
	u_short ptype;				/* 协议类型 (ETH_P_IP=0x0800 表示IP协议类型) */
	u_char  hlen;				/* 硬件地址长度 (6) */
	u_char  plen;				/* 协议地址长度 (4) */
	u_short operation;			/* ARP 操作类型 (1:arp请求 2:arp应答)*/
	u_char  sHaddr[6];			/* 源MAC地址 */
	u_char  sInaddr[4];			/* 源IP地址 */
	u_char  tHaddr[6];			/* 目的MAC地址 */
	u_char  tInaddr[4];			/* 目的IP地址 */
	u_char  pad[18];			/* 填充字节,因为爱以太网中传输的帧最小是64字节 */
};

int arpping(const uint8_t* sour_mac, const uint32_t sour_ip, const uint32_t dest_ip, const uint8_t* interface)
{
	int s; /* raw socket */
	int send_times = 3;
	int optval = 1;
	struct arpMsg	arp;
	struct sockaddr addr;
	/* create a raw socket */
	if ((s = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP))) == -1) {
		perror("arpping open raw socket failed");
		return -1;
	}

	/* 允许发送广播包,如果没有设置此option,即使目的MAC是FF:FF:FF:FF:FF:FF此报文也不被发送 */
	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) == -1) {
		perror("setsockoptetopt on raw socket failed");
		close(s);
		return -1;
	}

	/* 封装 Ethernet 报文 */
	memset(&arp, 0, sizeof(arp));
	memcpy(arp.ethhdr.h_dest, MAC_BROADCAST_ADDR, 6);
	memcpy(arp.ethhdr.h_source, sour_mac, 6);

	/* 封装 arp 报文 */
	arp.htype = htons(ARPHRD_ETHER);
	arp.ptype = htons(ETH_P_IP);
	arp.hlen = 6;
	arp.plen = 4;
	arp.operation = htons(ARP_REQUEST);
	memcpy(arp.sHaddr, sour_mac, 6);
	memcpy(arp.sInaddr, &sour_ip, sizeof(sour_ip));
	memcpy(arp.tInaddr, &dest_ip, sizeof(dest_ip));

	/* 发送arp request 广播包*/
	memset(&addr, 0, sizeof(addr));
	strncpy(addr.sa_data, interface, sizeof(addr.sa_data));
	
	if (sendto(s, &arp, sizeof(arp), 0, &addr, sizeof(addr)) < 0){
		perror("arpping sendto error");
		return -1;
	}

	/* 获取返回数据(阻塞) */
	if (recv(s, &arp, sizeof(arp), 0) < 0)
	{
		printf("no response\n");
		return -1;
	}
	printf("interface dest_mac: %02X:%02X:%02X:%02X:%02X:%02X\n",\
            arp.sHaddr[0],arp.sHaddr[1],arp.sHaddr[2],\
            arp.sHaddr[3],arp.sHaddr[4],arp.sHaddr[5]);         

	return 0;
}

/*
    用socket获得当前interface的信息
    1:ip地址
    2:mac地址
    3:interface index
注:创建的是原始套接字，编译及执行都需要root权限
*/
int read_interface(const char* interface, uint32_t *sour_ip, uint8_t *sour_mac)
{
    int fd;
    int ret = 0;
    struct ifreq ifr;
    struct sockaddr_in *ip_addr = NULL;
    memset(&ifr, 0, sizeof(struct ifreq));
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (fd > 0){
        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name, interface, sizeof(ifr.ifr_name));
        //get interface ip
        if (sour_ip) {
	        if (ioctl(fd, SIOCGIFADDR, &ifr) == 0){
	            ip_addr = (struct sockaddr_in *) &ifr.ifr_addr;
	            printf("The sour_ip addr is:%s\n", inet_ntoa(ip_addr->sin_addr));
	            *sour_ip = ip_addr->sin_addr.s_addr;
	        } else {
	            perror("SIOCGIFADDR failed");
	            ret = -1;
	            goto out;
	        }
 		}

        //get interface mac
 		if (sour_mac) {
	        if(ioctl(fd, SIOCGIFHWADDR, &ifr) == 0){
	            memcpy(sour_mac, ifr.ifr_hwaddr.sa_data, 6);
	            printf("interface sour_mac: %02X:%02X:%02X:%02X:%02X:%02X\n",
	                sour_mac[0],sour_mac[1],sour_mac[2],sour_mac[3],sour_mac[4],sour_mac[5]);         
	        } else {
	            perror("SIOCGIFHWADDR failed");
	            ret = -1;
	            goto out;
	        }
		}
    } else {
        perror("read_interface socket failed");
        ret = -1;
    }
out:
    close(fd);
    return ret;
}

int main(int argc,char** argv)
{
    int ret;
    if(argc < 2){
    	printf("need a dest ip\n");
    	return -1;
    }
    uint32_t dest_ip = inet_addr(argv[1]);
    char *interface = "eth0";
    uint32_t sour_ip;
    uint8_t sour_mac[6] = {0};
    /* 获取本地IP,MAC信息 */
    read_interface(interface, &sour_ip, sour_mac);
    
    /* 调用arpping广播arp报文获取MAC地址 */
    arpping(sour_mac, sour_ip, dest_ip, interface);

    return 0;
}
