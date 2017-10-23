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

#define SERVER_PORT 4950


int main(int argc, char** argv) {
    char clieninfo[255]  = {0};
    char servreply[255]  = {0};
    int fd, end = 0, rcount;
    struct sockaddr_in serveraddr;        
    if(argc < 2){
        printf("usage: clien <ipaddr>");
        exit(1);
    }

    printf("client pid:%d\n", getpid());
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("client create socket failure\n");
        exit(1);
    }
    memset(&serveraddr, 0, sizeof serveraddr);

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port   = htons(SERVER_PORT);
    inet_pton(AF_INET, argv[1], &serveraddr.sin_addr);

    if(connect(fd, (struct sockaddr *)&serveraddr, sizeof serveraddr) < 0){
        perror("connect error");
        goto out;
    }

    while(!end){
        memset(clieninfo, 0, sizeof clieninfo);
        memset(servreply, 0, sizeof servreply);

        read(STDIN_FILENO, clieninfo, sizeof clieninfo);
        if(clieninfo[0] == 'q' || clieninfo[0] == 'Q'){ 
            shutdown(fd, SHUT_RDWR);
            printf("close fd:%d\n", fd);
            end = 1;
        } else {
            printf("write to server:%s\n", clieninfo);
            write(fd, clieninfo, sizeof clieninfo);
        }

        if((rcount = read(fd, servreply, sizeof servreply)) < 0){
            if(errno == EINTR)
                continue;
            else {
                perror("read error");
                break;
            }
        } else if(rcount == 0) {
            printf("server send a FIN\n");
            break;
        } else {
            printf("Server reply:%s\n", servreply);
        }
    }
out:
    if(fd) close(fd);
    return 0;
}