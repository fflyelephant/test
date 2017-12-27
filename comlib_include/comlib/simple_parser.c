#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simple_parser.h"

#define diag_printf printf

#define DEBUG_VAR_CHECKER(name , var) \
diag_printf("[DEBUG][%s][%d] "name": %x\n" , __FUNCTION__ , __LINE__ , var)

//keyword string table
static char *token_str[] = {
"config" , "pipe" , "bw" , "add" , 
"from" , "to" , "via" , "delay" , "nat" , 
"del" , "flush" , "rdr" , "port" , "->" ,
"tcp" , "udp" , "list" , "ipfw" , "enable" , "disable" ,
"any" , "in" , "out" , "map" , "MAC" , "create" , 
"keep" , "check" , "log" , "estab" , "count" , "size", "trigger"};

void parser_init(struct parser_info *info , int argc , char *argv[] , int parse_start)
{
	memset(info , 0 , sizeof(*info));
	
	info->argc = argc;
	info->argv = argv;
	info->cur_pos = parse_start;
}

//return value: 0 not matched
//				1 matched
int match(struct parser_info *info , int token)
{
	if(info->cur_pos >= info->argc)
		return 0;

	if(token >= TOKEN_CONFIG && token < TOKEN_SIGNED_VALUE)
		if(strcmp(token_str[token] , info->argv[info->cur_pos]) != 0)
			return 0;

	if(token == TOKEN_UNSIGNED_VALUE)
		if(sscanf(info->argv[info->cur_pos] , "%u" , &info->val.unsigned_value) != 1)
			return 0;			

	if(token == TOKEN_SIGNED_VALUE)
		if(sscanf(info->argv[info->cur_pos] , "%d" , &info->val.signed_value) != 1)
			return 0;

	if(token == TOKEN_IP_STR)
	{
		unsigned int val[4];
		if(sscanf(info->argv[info->cur_pos] , "%u.%u.%u.%u" , &val[0] , &val[1] , &val[2] , &val[3]) != 4)
			return 0;

		info->val.ip_str = info->argv[info->cur_pos];
	}

	if(token == TOKEN_MAC_ADDRESS)
	{
		int index;
		unsigned int val[6];

		if(sscanf(info->argv[info->cur_pos] , "%x:%x:%x:%x:%x:%x" , 
		          &val[0] , &val[1] , &val[2] , &val[3] , &val[4] , &val[5]) != 6)
			return 0;

		for(index = 0 ; index < 6 ; ++index)
			info->val.mac[index] = val[index];
	}

	if(token == TOKEN_IP_RANGE)
	{
		unsigned int buf[8];
		if(sscanf(info->argv[info->cur_pos] , "%u.%u.%u.%u-%u.%u.%u.%u" , 
			&buf[0] , &buf[1] , &buf[2] , &buf[3] ,
			&buf[4] , &buf[5] , &buf[6] , &buf[7]) != 8)
			return 0;

		info->val.ips[RANGE_START] = (buf[0] << 24) + (buf[1] << 16) + 
			(buf[2] << 8) + buf[3];

		info->val.ips[RANGE_END] = (buf[4] << 24) + (buf[5] << 16) + 
			(buf[6] << 8) + buf[7];		
	}

	if(token == TOKEN_PORT_RANGE)
	{
		unsigned int ports[2];

		if(sscanf(info->argv[info->cur_pos] , "%u-%u" , &ports[0] , &ports[1]) != 2)
			return 0;

		info->val.ports[RANGE_START] = ports[RANGE_START];
		info->val.ports[RANGE_END] = ports[RANGE_END];
	}

	if(token == TOKEN_STRING)
		info->val.str = info->argv[info->cur_pos];

	++info->cur_pos;
	return 1;
}
