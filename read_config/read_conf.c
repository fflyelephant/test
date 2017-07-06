#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

int main(int agrc,char** agrv)
{
	char buffer[80], *token, *line;
	FILE* in = NULL;
	if(!(in = fopen("./config.conf","r")))
	{
		printf("open file error!\n");
		return -1;
	}

	while(fgets(buffer, 80, in)) {

		if(strchr(buffer, '\n'))  *(strchr(buffer, '\n')) = '\0';
		
		if(strchr(buffer, '#'))  *(strchr(buffer, '#')) = '\0';

		token = buffer + strspn(buffer, " \t");
		if(*token == '\0') continue;

		line = token + strcspn(token, " \t=");
		if(*line == '\0') continue;
		*line++ = '\0';
		printf("token:%s\n",token);
		
		line = line + strspn(line, " \t=");
		printf("line:%s\n",line);
	};
	fclose(in);
	return 0;
}
