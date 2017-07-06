#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "read_conf.h"

int main(int argc, char** argv)
{
	FILE* conf_fp = NULL;
	if(argc < 2)
		read_config(NULL);
	else{
		conf_fp = fopen(argv[1], "r");
		if(NULL == conf_fp)
		{
			perror("open config file errorn\n");
			exit(1);
		}
		read_config(conf_fp);
	}
	if(conf_fp != NULL) fclose(conf_fp);
	print_config();
	return 0;
}
