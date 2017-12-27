/* vi: set sw=4 ts=4: */
/*
 *	APIs to use unix domain socket with DGRAM
 *
 *	Created by David Hsieh <david_hsieh@alphanetworks.com>
 *	Copyright (C) 2007-2009 by Alpha Networks, Inc.
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
 #if defined(__ECOS__)//for ecos 

#include <network.h> //for ecos 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <usock.h>

#define USOCK_MAGIC	0x48823380
static int o_udp_port = 40000;
struct usock_entry
{
	uint32_t magic;
	int sock;
	int port;
	int is_server;
};

extern void write_portnum(const char *name, int port);
extern int read_portnum(const char *name);

/***********************************************************************/

int usock_fd(usock_handle usock)
{
	struct usock_entry * us = (struct usock_entry *)usock;
	return us->sock;
}

/***********************************************************************/

usock_handle usock_open(int server, const char * name)
{
	struct usock_entry * entry = NULL;
	struct sockaddr_in where;
	int port;
	do
	{
		/* check socket name */
		if (!name) break;

		/* allocate entry space. */
		entry = (struct usock_entry *)malloc(sizeof(struct usock_entry));
		if (!entry) break;
		memset(entry, 0, sizeof(struct usock_entry));

		/* get socket fd */
		entry->sock = socket(AF_INET, SOCK_DGRAM, 0);
		
		if (entry->sock < 0) break;
	
		
		if (server)//if server bind to 127.0.0.1
		{
			port = o_udp_port++;
			bzero(&where, sizeof(where));
			where.sin_family = AF_INET;
			where.sin_port = htons(port);
			//snprintf(where.sun_path, sizeof(where.sun_path), "%s", name);
			where.sin_addr.s_addr=inet_addr("127.0.0.1");
			where.sin_len = sizeof(where); //freebsd has this field
			if (bind(entry->sock, (struct sockaddr *)&where, sizeof(where)) < 0)
			{
				fprintf(stderr, "%s: bind: %s.\n",__func__,strerror(errno));
				break;
			}
			write_portnum(name,port);
			entry->is_server = 1;
			entry->port = port;
		}
		else
		{
			entry->is_server = 0;
			entry->port = read_portnum(name);
			if(!entry->port) 
			{
				diag_printf("server not found\n");
				break;//no this server
			}
		}	
		entry->magic = USOCK_MAGIC;
		/* we are done ! */
		return entry;
	} while (0);

	if (entry->sock >= 0)
		close(entry->sock);

	free(entry);
	return NULL;
}

void usock_close(usock_handle usock)
{
	struct usock_entry * entry = (struct usock_entry *)usock;
	if (entry->sock >= 0) close(entry->sock);

	free(entry);
}

int usock_send(usock_handle usock, const void * buf, unsigned int len, int flags)
{
	struct usock_entry * entry = (struct usock_entry *)usock;
	struct sockaddr_in where;
#if 0	
	if (entry->is_server) 
	{
		diag_printf("server can not send message\n");
		return 0;
	}
#endif	
	where.sin_family = AF_INET;
	where.sin_port = htons(entry->port);
	//snprintf(where.sun_path, sizeof(where.sun_path), "%s", name);
	where.sin_addr.s_addr=inet_addr("127.0.0.1");
	where.sin_len = sizeof(where); //freebsd has this field
	return sendto(entry->sock, buf, len, flags, (struct sockaddr *)&where, sizeof(where));
}

int usock_recv(usock_handle usock, void * buf, unsigned int len, int flags)
{
	struct usock_entry * entry = (struct usock_entry *)usock;
	//socklen_t fromlen;

	if (!entry->is_server)
	{
		diag_printf("client can not send message\n");
		return 0;
	}
	return recv(entry->sock, buf, len, flags);
}

int usock_recv_timed(usock_handle usock, void * buf, unsigned int len, int flags, int timeout)
{
	int ret;
	struct timeval tv;
	fd_set fds;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(usock_fd(usock), &fds);
	ret = select(usock_fd(usock)+1, &fds, NULL, NULL, &tv);
	if (ret > 0) return usock_recv(usock, buf, len, flags);
	return ret;
}

/***********************************************************************/
#endif