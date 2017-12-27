#include <network.h>
#include <cyg/kernel/kapi.h> //this is must when you call any cyg_xxx function.
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>

#if defined(__ECOS__)
#include <libstring.h>
#endif
//joel add the utility
int getopt(int argc, char **argv, const char *optstring,char **optarg,int *optind)
{
	int id;
	int i,opt;
	char *pos;
	if (!optind || !optarg)
	{
		fprintf(stderr,"gotopt point error\n");
		return 0;
	}
	/* optind is started from 1. */
	if (*optind==0) *optind=1;
	id = *optind;

	if (id > argc || id < 0) return -1;

	for (i=id; i<argc; i++)
	{
		if (argv[i][0] == '-')
		{
			opt = argv[i][1];
			pos = strchr(optstring,(char)opt);
			if (!pos)
			{
				fprintf(stderr,"unknow option -%c\n",opt);
				return -1;
			}
			if (*(pos+1)==':')/*if last one +1 will the '\0' ,it is safe*/
			{
				if (argv[i][2])
				{
					*optarg = &(argv[i][2]);
					*optind = i+1 <= argc ? i+1 : -1;
				}
				else if (i+1 <= argc)
				{
					*optarg = argv[i+1];
					*optind = i+2 <= argc ? i+2: -1;
				}
				else
				{
					fprintf(stderr, "%s: opt - %c needs argument !\n",__func__,opt);
					return -1;
				}
			}
			else
			{
				*optarg = NULL;
				*optind = i+1;
			}
			return opt;
		}
	}
	//*optind = i+2 <= argc ? i+2: -1;
	return -1;
}
//end

//get mac by interface name.
unsigned char *get_mac_by_name(char *name)
{
	int fd;
	static struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	if(!name)
		goto get_mac_by_name_out;

	if ( strncmp(name, "lo", 2) == 0 )
		goto get_mac_by_name_out;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( fd < 0)
	{
		diag_printf("get_mac_by_name fail open socket\n");
		goto get_mac_by_name_out;
	}
	else
	{
		ifr.ifr_addr.sa_family = AF_INET;
		strncpy(ifr.ifr_name, name, IFNAMSIZ-1);

		if(ioctl(fd, SIOCGIFHWADDR, &ifr)<0) {
			diag_printf("get_mac_by_name: SIOCGIHWADDR Error\n");
		}
		close(fd);
	}

get_mac_by_name_out:

	return (unsigned char *)ifr.ifr_hwaddr.sa_data;
}

int set_mac_by_name(char *name, char *mac)
{
	int ret = 0;
	int fd;
	struct ifreq ifr;

	if(!name)
		return -1;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( fd < 0)
	{
		diag_printf("set_mac_by_name fail open socket\n");
		return -1;
	}
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, name, IFNAMSIZ-1);
	memcpy(ifr.ifr_hwaddr.sa_data, mac, 6);
	if(ioctl(fd, SIOCSIFHWADDR, &ifr)<0) {
		perror("SIOCGIHWADDR Error:");
		ret = -1;
	}

	close(fd);

	return ret;


}

//find the char c from the end of str and trim string to there.
void trim(char *str, char c)
{
int len;

	if (str == NULL) return;

	len = strlen(str);
	while(len)
	{
		if (str[len-1] == c)
		{
			str[len-1] = 0;
			break;
		}
		len--;
	}

	if (c == 0x0a)
		trim(str, 0x0d);

}

void dump(char *msg, void *data, int len)
{
int i;
unsigned char *buf = (unsigned char*) data;

	fprintf(stderr, "%s\n", msg);
	for(i=0; i < len; i++)
	{
		fprintf(stderr, "%02x ", (unsigned int)buf[i]);
		if ((i+1)%16 == 0)
			fprintf(stderr, "\n");
	}

	fprintf(stderr, "--------dump end-------\n");
}

unsigned long uptime(void)
{
	return (long)cyg_current_time()/100;
}


#ifndef HAVE_STRCASESTR
/*
 * only glibc2 has this.
 */
char *strcasestr(const char *haystack, const char *needle)
{
    const char *cp1=haystack, *cp2=needle;
    const char *cx;
    int tstch1, tstch2;

    /* printf("looking for '%s' in '%s'\n", needle, haystack); */
    if (cp1 && cp2 && *cp1 && *cp2)
        for (cp1=haystack, cp2=needle; *cp1; ) {
            cx = cp1; cp2 = needle;
            do {
                /* printf("T'%c' ", *cp1); */
                if (! *cp2) { /* found the needle */
                    /* printf("\nfound '%s' in '%s'\n", needle, cx); */
                    return (char *)cx;
                }
                if (! *cp1)
                    break;

                tstch1 = toupper(*cp1);
                tstch2 = toupper(*cp2);
                if (tstch1 != tstch2)
                    break;
                /* printf("M'%c' ", *cp1); */
                cp1++; cp2++;
            }
            while (1);
            if (*cp1)
                cp1++;
        }
    /* printf("\n"); */
    if (cp1 && *cp1)
        return (char *)cp1;

    return NULL;
}
#endif

static int hex2num(char c)
{
  	if (c >= '0' && c <= '9')
        return c - '0';
	if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
	return -1;
}
/**
 ** hwaddr_aton - Convert ASCII string to MAC address+ ** @txt: MAC address as a string (e.g., "00:11:22:33:44:55")
 ** @addr: Buffer for the MAC address (ETH_ALEN = 6 bytes)
 ** Returns: 0 on success, -1 on failure (e.g., string not a MAC address)
 **/
int hwaddr_aton(const char *txt, unsigned char *addr)
{
	int i;
	for (i = 0; i < 6; i++)
	{
	        int a, b;
	        a = hex2num(*txt++);
	        if (a < 0)
	        return -1;
	        b = hex2num(*txt++);
	        if (b < 0)
	        return -1;
	        *addr++ = (a << 4) | b;
	        if (i < 5 && *txt++ != ':')
	        return -1;
    	}
    	return 0;
}

#if defined(__ECOS__)
void clean_args(struct param_arg *param)
{
	int i;
	if(!param)
	{
		return;
	}
	for(i=0;i<param->argc && i< sizeof(param->argv)/sizeof(param->argv[0]);i++)
	{
		if(param->argv[i])
		{
			free(param->argv[i]);
			param->argv[i]= NULL;
		}
	}
	param->argc = 0;
}
int copy_args(struct param_arg *param,int argc,char **argv)
{
	int i;
	if(!param)
	{
		return 0;
	}
	clean_args(param);//clean old param

	param->argc = argc;
	for(i=0;i<param->argc && i< sizeof(param->argv)/sizeof(param->argv[0]);i++)
	{
		param->argv[i] = strdup(argv[i]);
		if(!param->argv[i])
		{
			clean_args(param);
			return 0;
		}
	}
	return 1;
}

void parse_string(char *str, int *p_argc, char **argv)
{
	int i;
	int argc=0;

	for (i=0; str[i] ; ) {
		if (str[i] && str[i] != ' ') {
			argv[argc++] = &str[i];
			if ( str[i] == '"' ) {	//let "ls 123" as one argv.
				char *ptr = strchr(&str[i+1], '"');
				if (ptr) {
					argv[argc-1] = &str[i+1];
					*ptr = '\0';
					i = ptr - str +1;
					continue;
				}
			}
			i++;
			while (str[i]!= 0 && str[i] != ' ')
				i++;
			if (str[i] == 0)
				goto out;
			else
				str[i] = 0;
			i++;
		} else
			i++;
	}
out:
	*p_argc = argc;
}

#endif
