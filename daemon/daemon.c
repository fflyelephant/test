#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/resource.h>

#define ERROR_EXIT(m)\
do\
{\
	perror(m);\
	exit(EXIT_FAILURE);\
}\
while(0);


void mydaemon(int nochdir, int noclose)
{	
	pid_t pid;
	int fd, i, nfiles;
	struct rlimit rl;

	pid = fork();
	if(pid < 0)
		ERROR_EXIT("First fork failed!");

	if(pid > 0)
		exit(EXIT_SUCCESS);// father exit

	if(setsid() == -1)
		ERROR_EXIT("setsid failed!");

	pid = fork();
	if(pid < 0)
		ERROR_EXIT("Second fork failed!");

	if(pid > 0)// father exit
		exit(EXIT_SUCCESS);

	/* 关闭从父进程继承来的文件描述符 */
	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		ERROR_EXIT("getrlimit failed!");
	nfiles = rl.rlim_cur = rl.rlim_max;
	setrlimit(RLIMIT_NOFILE, &rl);
	for(i=3; i<nfiles; i++)
		close(i);

	/* 重定向标准的3个文件描述符 */
	if(!noclose)
	{
		if(fd = open("/dev/null", O_RDWR) < 0)
			ERROR_EXIT("open /dev/null failed!");
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);	
		if(fd > 2)
	    	close(fd);

	}

	/* 改变工作目录和文件掩码常量 */
	if(!nochdir)
		chdir("/");
	umask(0);
}


int main(int argc, char **argv)
{
	time_t t;
	int fd, i;
	mydaemon(0, 0);
	//daemon(0, 0);
	fd = open("./mydaemon.log", O_RDWR|O_CREAT, 0644);
	if(fd < 0)
		ERROR_EXIT("open mydaemon.log failed!");
	for(i=0; i<3; i++)
	{
		t = time(0);
		char *buf = asctime(localtime(&t));
		write(fd, buf, strlen(buf));
		printf("write\n");
		sleep(10);
	}
	close(fd);
	return 0;
}