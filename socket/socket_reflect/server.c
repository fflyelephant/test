#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <assert.h>
#include <netdb.h>

#define PORT 4950

void set_nonblock(int fd) {
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    assert(flags != -1);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char** argv) {
    char clieninfo[255]  = {0};
    int rcount;
    int status, fd, adrlen, connected_fd;
    int retval, n = 1;
    fd_set rfds; 
    struct sockaddr_in serveraddr;
    struct sockaddr_in clientaddr;
    struct timeval timeout;         

    printf("server pid:%d\n", getpid());
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("server create socket failure\n");
        exit(1);
    }

    memset(&serveraddr, 0, sizeof serveraddr);
    memset(&clientaddr, 0, sizeof clientaddr);

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port   = htons(PORT);
    /* INADDR_ANY = '0.0.0.0', 表示本主机下所有到此PORT的报文都copy一份给此套接字 */
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    adrlen = sizeof clientaddr;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&n, sizeof n) == -1) {
        perror("setsockopt failure");
        exit(1);
    }

    if(bind(fd, (struct sockaddr *)&serveraddr, sizeof serveraddr) < 0) {
        perror("server bind failure\n");
        exit(1);
    }

    if(listen(fd, 5) < 0) {
        perror("server listen failure\n");
        exit(1);
    }

    connected_fd = accept(fd, (struct sockaddr *)&clientaddr, &adrlen);
    if(connected_fd < 0) {
        perror("server accept failure\n");
        exit(1);
    }

    set_nonblock(connected_fd);
    printf("Successful Connection!\n");

    while(1) {
        memset(clieninfo, 0, sizeof clieninfo);
        timeout.tv_sec  = 10;
        timeout.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(connected_fd, &rfds);
        printf("before:%ld\n", time(0));
        retval = select(connected_fd+1, &rfds, NULL, NULL, &timeout);
        printf(" after:%ld\n", time(0));

        if(retval == 0){
        //timeout connected_fd don't ready to be read
            continue;
        } else if (retval > 0){
        //connected_fd ready to be read
            if(FD_ISSET(connected_fd, &rfds)){
               if((rcount = read(connected_fd, clieninfo, sizeof clieninfo)) < 0){
                    if(errno == EINTR)
                        continue;
                    else {
                        perror("server read error");
                        break;
                    }
               } else if (rcount == 0){
                    printf("client send a FIN, server will shutdown,clieninfo:%s, %d\n", clieninfo, rcount);
                    shutdown(connected_fd, SHUT_RDWR);
                    break;
               } else {
                    printf("Client:%s\n", clieninfo);
                    write(connected_fd, clieninfo, rcount);
               }     
            }
        }
        else if (errno == EINTR){
        //select interrupt by a signal
            printf("select interrupt by a signal");
            continue;
        } else {
        //some error happend
            perror("select error\n");
            break;
           // continue;
        }
    }
    close(fd);
    return 0;
}
/*验证了两个问题
1:selec函数中有检测非阻塞的描述符, 这时select仍旧最多等待timeout的时延才返回, 并不会因为监听的是非阻塞描述符
而select立即返回, 提前返回的条件还是准备好读写(相当于非阻塞的描述符在select中执行了同阻塞描述符类似的动作)

2:对于TCP的套接字来说, 如果一方read返回的是0, 表示对端已经发送了FIN, 本端应用层获得这个FIN的机制就是
  A:此套接字变为可读
  B:read的结果却是0

记录:
A:close(fd)只是将fd的引用计数减一, 引用计数为0的时候(注意多进程中的引用计数)才会执行结束的四次握手动作
B:shutdown(fd, howto), 是专门为网络编程提供的结束接口, 不过fd的引用计数是多少, 直接执行关闭套接字的动作
*/