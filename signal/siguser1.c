#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

static void signal_handler(int sig)
{
	printf("Got SIGUSR1\n");
	printf("Processing SIGUSR1...\n");
	sleep(5);
	printf("sleep over\n");
} 

int main(int argc, char** argv)
{
	char buf[20];
	int n = 0;
	printf("PID:%d\n", getpid());
	struct sigaction act, oldact;
	act.sa_handler = signal_handler;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGINT);
	act.sa_flags = SA_RESTART;
	if(sigaction(SIGUSR1, &act, &oldact) < 0)
	{
		perror("sigaction error");
		return -1;
	}
	
	memset(buf, 0, 20);	
	if((n = read(STDIN_FILENO, buf, 20)) < 0){//行缓冲阻塞
		perror("read:");
	}
	else{
		buf[n] = '\0';
		printf("%d type got, string:%s\n", n, buf);
	}
	perror("read");
	return 0;
}
