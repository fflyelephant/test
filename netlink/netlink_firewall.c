#include <asm/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>
#include<linux/netfilter_ipv4/ip_queue.h>
#include <netinet/in.h>
 
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

/* 此定义值和内核保持一致 */
enum _nf_action{
	NF_DROP,
 	NF_ACCEPT
/*	
	NF_STOLEN,
	NF_QUEUE,
	NF_REPEAT
*/
}nf_action;

/*发送的IPQM_MODE消息内存图
--------------------------------------
|				 |					 |
|struct nlmsghdr |struct ipq_mode_msg|
--------------------------------------
*/
int send_mode(int sockfd)
{
	//int seq = 0;
	struct sockaddr_nl addr, local;
	struct nlmsghdr *nl_header = NULL;
	struct ipq_mode_msg *mode_data = NULL;
	char buf[128] = {0};
	socklen_t addrlen;
	/* 目的地址结构(内核) */
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = 0; /* 发向内核设置为0 */
	addr.nl_groups = 0; /* NETLINK_FIREWALL 不支持多播 */
	addrlen = sizeof(addr);

	/* 绑定本地地址结构 */
	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_pid = getpid(); 
	local.nl_groups = 0; /* NETLINK_FIREWALL 不支持多播 */
	if(bind(sockfd, (struct sockaddr*)&local, sizeof(local)) < 0){
		perror("bind error");
		return -1;
	}

	nl_header = (struct nlmsghdr*)buf;
	nl_header->nlmsg_len  = NLMSG_LENGTH(sizeof(struct ipq_mode_msg));
	nl_header->nlmsg_type = IPQM_MODE;
	nl_header->nlmsg_flags= NLM_F_REQUEST;
	//nl_header->nlmsg_seq  = seq++;
	nl_header->nlmsg_pid  = getpid();

	mode_data = NLMSG_DATA(nl_header);
	mode_data->value = IPQ_COPY_META; /* 告诉内核我只想获取元数据部分 */
	mode_data->range = 0; /* 当value=IPQ_COPY_DATA时才有效 */
	/* 向内核发送请求 */
	if(sendto(sockfd, (void *)nl_header, nl_header->nlmsg_len, 0, (struct sockaddr *)&addr, addrlen) < 0){
		perror("sendto error\n");
		return -1;
	}
	return 0;
}	


int packet_handle(int sockfd)
{
	//int seq = 0;
	socklen_t addrlen;
	char recv_buf[128] = {0};
	char send_buf[128] = {0};
	struct sockaddr_nl addr;
	struct nlmsghdr *nl_header = NULL;
	struct ipq_verdict_msg *verdict_data = NULL;
	struct ipq_packet_msg *packet_data = NULL;
	addrlen = sizeof(addr);

	/*接受的IPQM_PACKET消息内存图
	----------------------------------------
	|				 |					   |
	|struct nlmsghdr |struct ipq_packet_msg|
	----------------------------------------
	*/
	if(recvfrom(sockfd, (void *)recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&addr, &addrlen) < 0){
		perror("recvfrom kernel packet error\n");
		return -1;
	}

	nl_header = (struct nlmsghdr*)recv_buf;
	/* 根据packet_data指向的struct ipq_packet_msg结构, 分析报文该采取何种处理方式 */
	packet_data = NLMSG_DATA(nl_header);
	
	/* 详细的分析代码 */
	/* .............. */
	/* 详细的分析代码 */

	/* 我默认最终将丢弃所有匹配到的报文 */
	nl_header = (struct nlmsghdr*)send_buf;
	nl_header->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ipq_verdict_msg));
	nl_header->nlmsg_type  = IPQM_VERDICT;
	nl_header->nlmsg_flags = NLM_F_REQUEST; 
	//nl_header->nlmsg_seq   = seq++;
	nl_header->nlmsg_pid   = getpid();
	verdict_data = NLMSG_DATA(nl_header);
	verdict_data->value = 1;
	verdict_data->id    = packet_data->packet_id;/* 重要:唯一指定了NF_DROP动作在哪个报文生效 */

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = 0; /* 发向内核设置为0 */
	addr.nl_groups = 0; /* NETLINK_FIREWALL 不支持多播 */

	/*发送的IPQM_VERDICT消息内存图
	-----------------------------------------
	|				 |					    |
	|struct nlmsghdr |struct ipq_verdict_msg|
	-----------------------------------------
	*/
	sleep(1);
	if(sendto(sockfd, (void *)nl_header, nl_header->nlmsg_len, 0, (struct sockaddr *)&addr, addrlen) < 0)
	{
		perror("sendto NF_DROP error\n");
		exit(1);
	}
	//printf("Got a match packet, and send NF_DROP to it\n");
	return 0;
}

int open_netlink()
{
	int netlink_socket;
	netlink_socket = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_FIREWALL);
	if(netlink_socket < 0){
		perror("creat netlink_socket failed\n");
		return -1;
	}
	return netlink_socket;
}

int main(int argc, char const *argv[])
{
	int sockfd, ret, c;
	while((c = getopt(argc, argv, "ad")) != -1){
		switch(c){
			case 'a':
				nf_action = NF_ACCEPT;
				break;
			case 'd':
				nf_action = NF_DROP;
				break;
			default:
				printf("UNKNOW parameter\n");
				exit(1);
		}
	}
	printf("nf_action:%s\n", nf_action ? "NF_ACCEPT" : "NF_DROP");
	sockfd = open_netlink();
	if(sockfd < 0)
		return -1;

	/* 先发送IPQM_MODE消息给内核, 表示此套接字要接收流过QUEUE的报文 */ 
	ret = send_mode(sockfd);
	if(ret < 0)
		return -1;
		
	/* 收到kernel中QUEUE匹配到的报文, 并回复verdict报文指定处理方式后方可接收下一个QUEUE报文 */	
	for( ; ; ){
		packet_handle(sockfd);
		//usleep(1000);
	}
		
	return 0;
}
