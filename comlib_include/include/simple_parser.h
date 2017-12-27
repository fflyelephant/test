#ifndef _SIMPLE_PARSER_H
#define _SIMPLE_PARSER_H

//keyword token section. When you modify this section,
//you should update token string table, too. The order 
//is important.
enum{TOKEN_CONFIG , TOKEN_PIPE , TOKEN_BW , TOKEN_ADD ,
TOKEN_FROM , TOKEN_TO , TOKEN_VIA , TOKEN_DELAY , TOKEN_NAT ,
TOKEN_DEL , TOKEN_FLUSH , TOKEN_RDR , TOKEN_PORT , TOKEN_RIGHT_ARROW ,
TOKEN_TCP , TOKEN_UDP , TOKEN_LIST , TOKEN_IPFW , TOKEN_ENABLE , TOKEN_DISABLE ,
TOKEN_ANY , TOKEN_IN , TOKEN_OUT , TOKEN_MAP , TOKEN_MAC , TOKEN_CREATE ,
TOKEN_KEEP , TOKEN_CHECK , TOKEN_LOG , TOKEN_ESTAB , TOKEN_COUNT , TOKEN_SIZE ,
TOKEN_TRIGGER,
//other token
TOKEN_SIGNED_VALUE , TOKEN_UNSIGNED_VALUE , TOKEN_IP_RANGE , 
TOKEN_PORT_RANGE , TOKEN_IP_STR , TOKEN_STRING , TOKEN_MAC_ADDRESS , TOKEN_QTY};

enum{RANGE_START , RANGE_END};

#define MAX_PORT_RANGE		5 
struct multiport{
	short num;
	short port[MAX_PORT_RANGE][2];
};

struct parser_info
{
	int argc;
	int cur_pos;
	char **argv;

	union
	{       
		unsigned short ports[2];	//for port range, host format
		unsigned long ips[2];		//for ip range, host format
		unsigned char mac[6];
		int signed_value;
		unsigned int unsigned_value;
		char *str;	//for TOKEN_STR
		char *ip_str;	//for TOKEN_IP_STR
	}val;
};

extern int match(struct parser_info *info , int token);
extern void parser_init(struct parser_info *info , int argc , char *argv[] , int parse_start);

#endif //_SIMPLE_PARSER_H
