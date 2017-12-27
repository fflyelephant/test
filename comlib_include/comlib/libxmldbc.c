/* vi: set sw=4 ts=4: */
/*
 *	libxmldbc.c
 *
 *	common library for xmldb client.
 *	Created by David Hsieh <david_hsieh@alphanetworks.com>
 *	Copyright (C) 2004-2009 by Alpha Networks, Inc.
 *
 *	This file is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation; either'
 *	version 2.1 of the License, or (at your option) any later version.
 *
 *	The GNU C Library is distributed in the hope that it will be useful,'
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *	Lesser General Public License for more details.
 *
 *	You should have received a copy of the GNU Lesser General Public
 *	License along with the GNU C Library; if not, write to the Free
 *	Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *	02111-1307 USA.
 */
#if defined(__ECOS__)
#include <network.h>
#endif
#include <stdio.h>
#ifdef MEMWATCH
#include "memwatch.h"
#else
#include <stdlib.h>
#endif
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#if !defined(__ECOS__)
#include <sys/un.h>
#endif
#include <errno.h>

#include <dtrace.h>
#if defined(__ECOS__)
#include <susock.h>
#endif
#include <xmldb.h>
#include <libxmldbc.h>

#if defined(__ECOS__)
#if defined(MSG_NOSIGNAL)
#undef MSG_NOSIGNAL
#endif
#define MSG_NOSIGNAL 0
#endif

#ifdef DEBUG_LIBXMLDBC
#define XMLDBCDBG(x)	x
#else
#define XMLDBCDBG(x)
#endif

#if !defined(__ECOS__)
int lxmldbc_run_shell(char * buf, int size, const char * format, ...)
{
	FILE * fp;
	int i, c;
	char cmd[MAX_CMD_LEN];
	va_list marker;

	va_start(marker, format);
	vsnprintf(cmd, sizeof(cmd), format, marker);
	va_end(marker);

	fp = popen(cmd, "r");
	if (fp)
	{
		for (i=0; i<size-1; i++)
		{
			c = fgetc(fp);
			if (c == EOF) break;
			buf[i] = (char)c;
		}
		buf[i] = '\0';
		pclose(fp);

		/* remove the last '\n' */
		i = strlen(buf);
		if (buf[i-1] == '\n') buf[i-1] = 0;
		return 0;
	}
	buf[0] = 0;
	return -1;
}
#endif

/* call system() in printf() format. */
int lxmldbc_system(const char * format, ...)
{
	char cmd[MAX_CMD_LEN];
	va_list marker;

	va_start(marker, format);
	vsnprintf(cmd, sizeof(cmd), format, marker);
	va_end(marker);
#if !defined(__ECOS__)
	return system(cmd);
#else
	return process_cmd(cmd);	//edit by spirit 0728
#endif
}

#define IS_WHITE(x)	((x) == ' ' || (x)=='\t' || (x) == '\n' || (x) == '\r')

char * lxmldbc_eatwhite(char * string)
{
	if (string==NULL) return NULL;
	while (*string)
	{
		if (!IS_WHITE(*string)) break;
		string++;
	}
	return string;
}

char * lxmldbc_reatwhite(char * ptr)
{
	int i;

	if (ptr==NULL) return NULL;
	i = strlen(ptr)-1;
	while (i >= 0 && IS_WHITE(ptr[i])) ptr[i--] = '\0';
	return ptr;
}

/**************************************************************************/
#if !defined(__ECOS__)
static int __open_socket(const char * sockname)
{
	struct sockaddr_un where;
	int fd;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		d_error("%s: Counld not create unix domain socket: %s.\n",__FUNCTION__, strerror(errno));
		return -1;
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC);

	where.sun_family = AF_UNIX;
	if (sockname == NULL) sockname = XMLDB_DEFAULT_UNIXSOCK;
	snprintf(where.sun_path, sizeof(where.sun_path), "%s", sockname);

	if (connect(fd, (struct sockaddr *)&where, sizeof(where)) < 0)
	{
		d_error("%s: Cound not connect to unix socket: %s.\n",__FUNCTION__, sockname);
		close(fd);
		return -1;
	}
	return fd;
}
#else
susock_handle __open_socket(sock_t name)
{
	if (name == NULL)
		name = XMLDB_DEFAULT_UNIXSOCK;

	return susock_open(name);
}
#endif

static int send_xmldb_cmd(int fd, action_t action, unsigned long flags, const char * data, unsigned short length)
{
	rgdb_ipc_t ipc;
	ssize_t size;

	ipc.action = action;
	ipc.flags = flags;
	ipc.length = length;
	size = send(fd, &ipc, sizeof(ipc), MSG_NOSIGNAL);
	if (size <= 0) return -1;
	size = send(fd, data, length, MSG_NOSIGNAL);
	if (size <= 0) return -1;
	return 0;
}

#if defined(__ECOS__)
//add by siyou to ouput http reply header.
//cgi may return some headers, we can't overwrite it.
//HTTP header be generated:
//Content-Type: text/xml
static char *deal_http_header(char *buf, ssize_t *psize,FILE *out)
{
char *ptr;
char tmp[500];
int n;

	ptr = strstr(buf,"\n\n");

	if (ptr)
	{
	char *p=buf;

		n=0;
		//output header first.
		while ( p < ptr )
		{
			if ( *p == '\n')
				tmp[n++] = '\r';

			tmp[n++] = *p;
			p++;
		}

		//we got the http header section.
		if ( strstr(buf, "Content-Type:") == NULL )
			n += sprintf(&tmp[n],"\r\nContent-Type: text/html");

		if ( strstr(buf, "Server:") == NULL )
			n += sprintf(&tmp[n],"\r\nServer: siyou server");

		n += sprintf(&tmp[n], "\r\nTransfer-Encoding: chunked");

		tmp[n++] = '\r';tmp[n++] = '\n';
		tmp[n++] = '\r';tmp[n++] = '\n';

		fwrite(tmp,n,1,out);

		ptr += 2; //skip \n\n.
		*psize -= ptr-buf;
	}
	else
		ptr = buf;

	return ptr;
}
#endif

static void redirect_output(int fd, FILE * out)
{
	fd_set read_set;
	ssize_t size;
	char buff[1024];
#if defined(__ECOS__)
	char *ptr;
	int need_check=1;
#endif

	for (;;)
	{
		FD_ZERO(&read_set);
		FD_SET(fd, &read_set);
		if (select(fd+1, &read_set, NULL, NULL, NULL) < 0) continue;
		if (FD_ISSET(fd, &read_set))
		{
			size = read(fd, buff, sizeof(buff));
			if (size <= 0) break;
#if !defined(__ECOS__)
			if (buff[size-1] == '\0')
			{
				fwrite(buff, 1, strlen(buff), out);
				break;
			}
			else
			{
				fwrite(buff, 1, size, out);
			}
#else
			//here stdout is mean http connection.
			if ( out != stdout )
			{
				if (buff[size-1] == '\0')
         		{
             		fwrite(buff, 1, strlen(buff), out);
             		break;
         		}
         		else
         		{
             		fwrite(buff, 1, size, out);
         		}
			}
			else
			{
				//now let process http reply header,
				//and output complete http header.
				ptr = buff;
				if ( need_check )
				{
					ptr = deal_http_header(buff, &size, out);
					if ( ptr != buff )
						need_check = 0;
				}

				if (ptr[size-1] == '\0')
				{
					//fix: we only rx null char and no other string data,
					//take care the NULL char is the last char but it is in another read.
					if ( (size -1) == 0)
					{
						//diag_printf("redirect_output: why send out empty data??\n");
						if ( need_check == 0 )
							fprintf(out,"0\r\n\r\n");

						break;
					}
					//print chunk bytes.
					if ( need_check == 0 )
						fprintf(out,"%lx\r\n", size-1);

					fwrite(ptr, size-1, 1, out);

					//print end and last end..
					if ( need_check == 0 )
						fprintf(out,"\r\n0\r\n\r\n");

					break;
				}
				else
				{
					//print chunk bytes.
					if ( need_check == 0 )
						fprintf(out,"%lx\r\n", size);

					fwrite(ptr, size, 1, out);

					if ( need_check == 0 )
						fprintf(out,"\r\n");
				}
			}
#endif
		}
	}
	//joel add for bcm platform.the web server will lost some data without flush.
	fflush(out);
}

static size_t redirect_to_buffer(int fd, char * buff, size_t buff_size)
{
	fd_set read_set;
	ssize_t size;
	size_t written = 0;

	dassert(buff && buff_size);

	for (;;)
	{
		FD_ZERO(&read_set);
		FD_SET(fd, &read_set);
		if (select(fd+1, &read_set, NULL, NULL, NULL) < 0) continue;
		if (FD_ISSET(fd, &read_set))
		{
			size = read(fd, buff+written, buff_size - written);
			if (size <= 0) break;
			written += size;
			if (buff[written - 1] == '\0') break;
			if (buff_size >= written)
			{
				d_error("%s: no more buffer space for read !!\n", __FUNCTION__);
				break;
			}
		}
	}
	return written;
}

/* command with output */
static int _cmd_w_out(sock_t sn, action_t a, flag_t f, const void * param, size_t size, FILE * out)
{
#if defined(__ECOS__)
	susock_handle sk;
#endif
	int sock, ret = -1;
#if !defined(__ECOS__)
	if ((sock = __open_socket(sn)) >= 0)
#else
	if ((sk = __open_socket(sn)) != NULL)
#endif
	{
#if defined(__ECOS__)
		sock = susock_fd(sk);
#endif
		if (send_xmldb_cmd(sock, a, f, param, size) >= 0)
		{
			redirect_output(sock, out ? out : stdout);
			ret = 0;
		}
#if !defined(__ECOS__)
		close(sock);
#else
		susock_close(sk);
#endif
	}
	return ret;
}

/* command without output */
static int _cmd_wo_out(sock_t sn, action_t a, flag_t f, const void * param, size_t size)
{
#if defined(__ECOS__)
	susock_handle sk;
#endif
	rgdb_ipc_t ipc;
	ssize_t rsize;
	int sock;
	int ret = -1;

#if !defined(__ECOS__)
	if ((sock = __open_socket(sn)) >= 0)
#else
	if ((sk = __open_socket(sn)) != NULL)
#endif
	{
#if defined(__ECOS__)
		sock = susock_fd(sk);
#endif
		if (send_xmldb_cmd(sock, a, f, param, size) >= 0)
		{
			rsize = read(sock, &ipc, sizeof(ipc));
			ret = ipc.retcode;
		}
#if !defined(__ECOS__)
		close(sock);
#else
		susock_close(sk);
#endif
	}
	return ret;
}

#if defined(__ECOS__)
/* export functions:get a node path by uid */
ssize_t xmldbc_getpathbyuid(const char * node, const char * uid_value, char * path_buf, size_t size)
{
	int i;
	ssize_t ret = -1;
	char uid_value_temp[32];
	char path_buf_temp[128];

	for(i=1;i<64;i++)
	{
		snprintf(path_buf_temp, size, "%s:%d/uid",node,i);
		if(!xmldbc_get_wb(NULL, 0, path_buf_temp, uid_value_temp, sizeof(uid_value_temp)))   //get uid value
		{
			if (!strcmp(uid_value_temp,uid_value))
			{
				ret = 0;
				snprintf(path_buf, size, "%s:%d",node,i);
				break;
			}
			else
			{
				continue;
			}
		}
		else
		{
			break;
		}
	}
	return ret;
}
#endif
/***************************************************************************/
/* export functions */

ssize_t xmldbc_get_wb(sock_t sn, flag_t f, const char * node, char * buff, size_t size)
{
#if defined(__ECOS__)
	susock_handle sk;
#endif
	int sock;
	ssize_t ret = -1;

#if !defined(__ECOS__)
	if ((sock = __open_socket(sn)) >= 0)
#else
	if ((sk = __open_socket(sn)) != NULL)
#endif
	{
#if defined(__ECOS__)
		sock = susock_fd(sk);
#endif
		if (send_xmldb_cmd(sock, XMLDB_GET, f, node, strlen(node)+1) >= 0)
		{
			redirect_to_buffer(sock, buff, size);
			ret = 0;
		}
#if !defined(__ECOS__)
		close(sock);
#else
		susock_close(sk);
#endif
	}
	return ret;
}

int xmldbc_get(sock_t sn, flag_t f, const char * node, FILE * out)
{
	return _cmd_w_out(sn, XMLDB_GET, f, node, strlen(node)+1, out);
}

ssize_t xmldbc_ephp_wb(sock_t sn, flag_t f, const char * file, char * buff, size_t size)
{
#if defined(__ECOS__)
	susock_handle sk;
#endif
	int sock;
	ssize_t ret = -1;

#if !defined(__ECOS__)
	if ((sock = __open_socket(sn)) >= 0)
#else
	if ((sk = __open_socket(sn)) != NULL)
#endif
	{
#if defined(__ECOS__)
		sock = susock_fd(sk);
#endif
		if (send_xmldb_cmd(sock, XMLDB_EPHP, f, file, strlen(file)+1) >= 0)
		{
			redirect_to_buffer(sock, buff, size);
			ret = 0;
		}
#if !defined(__ECOS__)
		close(sock);
#else
		susock_close(sk);
#endif
	}
	return ret;
}

int xmldbc_ephp(sock_t sn, flag_t f, const char * file, FILE * out)
{
	return _cmd_w_out(sn, XMLDB_EPHP, f, file, strlen(file)+1, out);
}

int xmldbc_set(sock_t sn, flag_t f, const char * node, const char * value)
{
	char buff[512];

	snprintf(buff, sizeof(buff)-1, "%s %s", node, value);
	buff[511] = '\0';
	return _cmd_wo_out(sn, XMLDB_SET, f, buff, strlen(buff)+1);
}

int xmldbc_setext(sock_t sn, flag_t f, const char * node, const char * cmd)
{
	char buff[512];

	snprintf(buff, sizeof(buff)-1, "%s %s", node, cmd);
	buff[511] = '\0';
	return _cmd_wo_out(sn, XMLDB_SETEXT, f, buff, strlen(buff)+1);
}

int xmldbc_getext(sock_t sn, flag_t f, const char * node, const char * cmd, FILE * out)
{
	char buff[512];

	snprintf(buff, sizeof(buff)-1, "%s %s", node, cmd);
	buff[511] = '\0';
	return _cmd_w_out(sn, XMLDB_GETEXT, f, buff, strlen(buff)+1, out);
}


int xmldbc_timer(sock_t sn, flag_t f, const char * cmd)
{
	return _cmd_wo_out(sn, XMLDB_TIMER, f, cmd, strlen(cmd)+1);
}

int xmldbc_killtimer(sock_t sn, flag_t f, const char * tag)
{
	return _cmd_wo_out(sn, XMLDB_KILLTIMER, f, tag, strlen(tag)+1);
}

int xmldbc_del(sock_t sn, flag_t f, const char * node)
{
	return _cmd_wo_out(sn, XMLDB_DEL, f, node, strlen(node)+1);
}

int xmldbc_reload(sock_t sn, flag_t f, const char * file)
{
	return _cmd_wo_out(sn, XMLDB_RELOAD, f, file, strlen(file)+1);
}

int xmldbc_patch(sock_t sn, flag_t f, const char * file)
{
	return _cmd_wo_out(sn, XMLDB_PATCH, f, file, strlen(file)+1);
}

int xmldbc_read(sock_t sn, flag_t f, const char * file)
{
	return _cmd_wo_out(sn, XMLDB_READ, f, file, strlen(file)+1);
}

int xmldbc_write(sock_t sn, flag_t f, const char * node, FILE * out)
{
	return _cmd_w_out(sn, XMLDB_WRITE, f, node, strlen(node)+1, out);
}

int xmldbc_dump(sock_t sn, flag_t f, const char * file)
{
	return _cmd_wo_out(sn, XMLDB_DUMP, f, file, strlen(file)+1);
}
