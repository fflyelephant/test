#ifndef _LIBSTRING_H_
#define _LIBSTRING_H_

#ifdef __cplusplus
extern "C" {
#endif

int getopt(int argc, char **argv, const char *optstring,char **optarg,int *optind);
unsigned char *get_mac_by_name(char *name);
int set_mac_by_name(char* name, char *mac);
void trim(char *str, char c);
void dump(char *msg, void *data, int len);
unsigned long uptime(void);
#ifndef HAVE_STRCASESTR
/*
 * only glibc2 has this.
 */
char *strcasestr(const char *haystack, const char *needle);
#endif

#if defined(__ECOS__)//add by aaron
struct param_arg {
	int argc;
	char *argv[10];
};
void clean_args(struct param_arg *param);
int copy_args(struct param_arg *param,int argc,char **argv);
#define SAFE_FREE(x) do{if(x) free(x);x = NULL;}while(0)
#define BZERO_VALUE(x) do{memset(&x,0,sizeof(x));}while(0)
#endif
int hwaddr_aton(const char *txt, unsigned char *addr);
void parse_string(char *str, int *p_argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif

