#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
/*
	用socket获得当前interface的信息
	1:ip地址
	2:mac地址
	3:interface index
注:创建的是原始套接字，编译及执行都需要root权限
*/
int read_interface(char* interface)
{
	int fd;
	int ret = 0;
	struct ifreq ifr;
	struct sockaddr_in *ip_addr = NULL;
	char mac[6] = {0};
	memset(&ifr, 0, sizeof(struct ifreq));
	fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if(fd >= 0)
	{
		ifr.ifr_addr.sa_family = AF_INET;
		strcpy(ifr.ifr_name, interface);

		//get interface ip
		if(ioctl(fd, SIOCGIFADDR, &ifr) == 0)
		{
			ip_addr = (struct sockaddr_in *)&ifr.ifr_addr;
			printf("The %s ip addr is %s\n",ifr.ifr_name,inet_ntoa(ip_addr->sin_addr));
		}
		else
		{
			printf("SIOCGIFADDR failed, error:%s\n", strerror(errno));
			ret = -1;
			goto out;
		}

		//get interface mac
		if(ioctl(fd, SIOCGIFHWADDR, &ifr) == 0)
		{
			memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
			printf("interface mac: %02X:%02X:%02X:%02X:%02X:%02X\n",
				mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);			
		}
		else
		{
			printf("SIOCGIFHWADDR failed, error:%s\n", strerror(errno));
			ret = -1;
			goto out;
		}

		//get interface index
		if (ioctl(fd, SIOCGIFINDEX, &ifr) == 0) 
		{
			printf("The interface index:%d\n",ifr.ifr_ifindex);
		} 
		else 
		{
			printf("SIOCGIFINDEX failed, error:%s\n", strerror(errno));
			ret = -1;
			goto out;
		}

	}
	else
	{
		printf("%s\n",strerror(errno));
		ret = -1;
	}
out:
	close(fd);
	return ret;
}

int main(int agrc,char** agrv)
{
	int ret;
	char *interface = "eth0";
	ret = read_interface(interface);
	printf("ret:%d\n",ret);
	return 0;
}