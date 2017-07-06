#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "read_conf.h"
static struct config_t config;

void print_config(void)
{
	printf("NULL\n");
}

static int hex2dec(char c)
{
	int retval;
	switch(c){
		case '0':
			retval = 0;
			break;
		case '1':
			retval = 1;
			break;
		case '2':
			retval = 2;
			break;
		case '3':
			retval = 3;
			break;
		case '4':
			retval = 4;
			break;
		case '5':
			retval = 5;
			break;
		case '6':
			retval = 6;
			break;
		case '7':
			retval = 7;
			break;
		case '8':
			retval = 8;
			break;
		case '9':
			retval = 9;
			break;
		case 'a':
			retval = 10;
			break;
		case 'b':
			retval = 11;
			break;
		case 'c':
			retval = 12;
			break;
		case 'd':
			retval = 13;
			break;	
		case 'e':
			retval = 14;
			break;	
		case 'f':
			retval = 15;
			break;									
	}
	return retval;
}

static int read_yn(char *line, void *dest)
{
	int retval = 1;
	int *arg = dest;
	if(!strcasecmp(line, "yes"))
		*arg = 1;
	else if(!strcasecmp(line, "no"))
		*arg = 0;
	else retval = 0;
	return retval;
}

static int read_ip(char *line, void *dest)
{
	int retval = 1;
	struct in_addr *addr = dest;
	if(!inet_aton(line, addr))
	{
		retval = 0;
	}
	return retval;
}

static int read_str(char *line, void *dest)
{
	memcpy(dest, line , 16);
	return 1;
}

static int read_mac(char *line, void *dest)
{
	int i, j, tens, ones;
	unsigned char *arg = dest;

	for(i=0, j=0; j<11; j=j+2)
	{	
		tens = hex2dec(line[j]);
		ones = hex2dec(line[j+1]);
		arg[i] = tens*16 + ones;
	}
	return 1;
}

static struct config_keyword keywords[] = {
	{"yesno",    read_yn,    &(config.yesno),    "no"},
	{"ipaddr",   read_ip,    &(config.ipaddr),   "192.168.0.1"},
	{"name",     read_str,   &(config.yourname), "fflyelephant"},
	{"macaddr",  read_mac,   &(config.mac),      "000000000000"},
	{"",		 NULL,		 NULL,				 ""}
};

int read_config(FILE *fp)
{
	int i;
	char buffer[80], *token, *line;
	/* 先填写默认值 */
	for(i=0; keywords[i].var; i++)
	{
		keywords[i].handler(keywords[i].def, keywords[i].var);
	}

	/* 使用默认的配置值 */	
	if(fp == NULL) return 0;

	/* 读入一行(80字节保证了能读完一行) */
	while(fgets(buffer, 80, fp)) {
		/* 替换行尾换行符为'\0' */
		if(strchr(buffer, '\n'))  *(strchr(buffer, '\n')) = '\0';
		token = buffer + strspn(buffer, " ");/* 跳过行首的空格 */
		if(*token == '#') continue;/* #起始的行跳过 */

		line = token + strcspn(token, " ");
		if(*line == '\0') continue;/* 没有value的也跳过 */
		*line++ = '\0';

		line = line + strspn(line, " ");
		for(i=strlen(line); i>0 && isspace(line[i-1]); i--)
			line[i-1] = '\0';

		for(i=0; keywords[i].var; i++)
		{
			if(!strcasecmp(token, keywords[i].keyword))
			{
				if(!(keywords[i].handler(line, keywords[i].var)))
				{
					printf("token:%s value:%s up failed, will use defaule value:%s\n",token, line, keywords[i].def);
					keywords[i].handler(keywords[i].def, keywords[i].var);
				}
			}
		}
		printf("token:%s\n", token);
		printf("line:%s\n", line);
	};
	return 0;
}
