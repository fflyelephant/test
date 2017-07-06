#ifndef _READ_CONF_H
#define _READ_CONF_H

struct config_keyword {
	char keyword[14];
	int (*handler)(char *line, void *dest);
	void *var;
	char def[30];
};

struct config_t {
	unsigned int ipaddr;
	char yourname[16];
	unsigned char mac[6];
	int yesno;
};

int read_config(FILE *fp);
void print_config(void);
#endif
