/* vi: set sw=4 ts=4: */
/*
 *	Select loop
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
#if defined(__ECOS__)
#include <cyg/kernel/kapi.h>
#include <network.h>
#endif
#include <stdio.h>
#ifdef MEMWATCH
#include "memwatch.h"
#else
#include <stdlib.h>
#endif
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#if !defined(__ECOS__)
#include <signal.h>
#include <sys/sysinfo.h>
#endif
#include "dlist.h"
#include "sloop.h"
#include "dtrace.h"

/********************************************************************/
#if DEBUG_SLOOP
#define SLOOPDBG(x)	x
#else
#define SLOOPDBG(x)
#endif

/* internal structure for sloop module */
#define SLOOP_TYPE_MASK		0x00ff
#define SLOOP_TYPE_SOCKET	1
#define SLOOP_TYPE_TIMEOUT	2
#define SLOOP_TYPE_SIGNAL	3
#define SLOOP_INUSED		0x0100

struct sloop_socket
{
	struct dlist_head list;
	unsigned int flags;
	int sock;
	void * param;
	sloop_socket_handler handler;
};

struct sloop_timeout
{
	struct dlist_head list;
	unsigned int flags;
	struct timeval time;
	void * param;
	sloop_timeout_handler handler;
};

struct sloop_signal
{
	struct dlist_head list;
	unsigned int flags;
	int sig;
	void * param;
	sloop_signal_handler handler;
};

struct sloop_data
{
	int terminate;
#if !defined(__ECOS__)
	int signal_pipe[2];
#endif
	void * sloop_data;
	struct dlist_head free_sockets;
	struct dlist_head free_timeout;
#if !defined(__ECOS__)
	struct dlist_head free_signals;
#endif
	struct dlist_head readers;
	struct dlist_head writers;
#if !defined(__ECOS__)
	struct dlist_head signals;
#endif
	struct dlist_head timeout;
};

static struct sloop_socket  _sloop_sockets[MAX_SLOOP_SOCKET];
static struct sloop_timeout _sloop_timeout[MAX_SLOOP_TIMEOUT];
#if !defined(__ECOS__)
static struct sloop_signal  _sloop_signals[MAX_SLOOP_SIGNAL];
#endif
/***************************************************************************/
/* static variables & functions */
#if defined(__ECOS__)
#define SLOOP_UDP_AS_SIGNAL
#ifdef SLOOP_UDP_AS_SIGNAL
static int o_signal_server_sock = -1;
#define sloop_signal_udp_port  39393
static int signal_server_sock(void)
{
	int sgsock;
	struct sockaddr_in where;

	sgsock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sgsock < 0) return -1;

	bzero(&where, sizeof(where));
	where.sin_family = AF_INET;
	where.sin_port = htons(sloop_signal_udp_port);
	//snprintf(where.sun_path, sizeof(where.sun_path), "%s", name);
	where.sin_addr.s_addr=inet_addr("127.0.0.1");
	where.sin_len = sizeof(where); //freebsd has this field
	if (bind(sgsock, (struct sockaddr *)&where, sizeof(where)) < 0)
	{
		fprintf(stderr, "%s: bind: %s.\n",__func__,strerror(errno));
		return -1;
	}
	return sgsock;

}
static void signal_client_send(void)
{
#if 1
	struct sockaddr_in where;

	if(o_signal_server_sock < 0)
	{
		diag_printf("sloop error signal server not ready\n");
		return;
	}
	memset(&where, 0, sizeof(struct sockaddr_in));
	where.sin_family = AF_INET;
	where.sin_port = htons(sloop_signal_udp_port);
	where.sin_addr.s_addr=inet_addr("127.0.0.1");
	where.sin_len = sizeof(where); //freebsd has this field
	if(sendto(o_signal_server_sock, "x",1,0, (struct sockaddr *)&where, sizeof(where))!=1)
		diag_printf("sloop error signal sendto\n");

#else
	int sgsock;
	struct sockaddr_in where;
	if(o_signal_server_sock < 0)
	{
		diag_printf("sloop error signal server not ready\n");
		return;
	}
	sgsock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sgsock < 0)
	{
		diag_printf("sloop error signal send\n");
		return;
	}
	where.sin_family = AF_INET;
	where.sin_port = htons(sloop_signal_udp_port);
	where.sin_addr.s_addr=inet_addr("127.0.0.1");
	where.sin_len = sizeof(where); //freebsd has this field
	if(sendto(sgsock, "x",1,0, (struct sockaddr *)&where, sizeof(where))!=1)
		diag_printf("sloop error signal sendto\n");
	//diag_printf("send x\n");
	close(sgsock);
#endif
}
#endif
static struct sloop_data sloop;
/*Now our get_uptime_detail is correct 10ms so we can not using less than 10 ms timer*/
void get_uptime_detail(long *second,long *usecond)
{
	unsigned long tmp;
	tmp = cyg_current_time();
	*second = tmp/100;
	*usecond = (tmp%100)*10*1000;
}
#endif

static struct sloop_data sloop;
#if !defined(__ECOS__)
static inline
#else
unsigned
#endif
long get_uptime(void)
{
#if !defined(__ECOS__)
	struct sysinfo info;
	sysinfo(&info);
	return info.uptime;
#else
	return (long)cyg_current_time()/100;
#endif
}

/* initialize list pools */
static void init_list_pools(void)
{
	int i;

	memset(_sloop_sockets, 0, sizeof(_sloop_sockets));
	memset(_sloop_timeout, 0, sizeof(_sloop_timeout));
#if !defined(__ECOS__)
	memset(_sloop_signals, 0, sizeof(_sloop_signals));
#endif
	for (i=0; i<MAX_SLOOP_SOCKET; i++) dlist_add(&_sloop_sockets[i].list, &sloop.free_sockets);
	for (i=0; i<MAX_SLOOP_SOCKET; i++)
		SLOOPDBG(daig_printf("%s: init sockets[%d]=[%x],\n", __FILE__, i, &_sloop_sockets[i].list));
	for (i=0; i<MAX_SLOOP_TIMEOUT;i++) dlist_add(&_sloop_timeout[i].list, &sloop.free_timeout);
#if !defined(__ECOS__)
	for (i=0; i<MAX_SLOOP_SIGNAL; i++) dlist_add(&_sloop_signals[i].list, &sloop.free_signals);
#endif
}

/* get socket from pool */
static struct sloop_socket * get_socket(void)
{
	struct dlist_head * entry;
	struct sloop_socket * target;

	if (dlist_empty(&sloop.free_sockets))
	{
		d_error("sloop: no sloop_socket available !!!\n");
		return NULL;
	}
	entry = sloop.free_sockets.next;
	SLOOPDBG(daig_printf("%s: get socket sd=[%x],\n", __FILE__, entry));
	dlist_del(entry);
	target = dlist_entry(entry, struct sloop_socket, list);
	target->flags = SLOOP_INUSED | SLOOP_TYPE_SOCKET;
	return target;
}

/* get timeout from pool */
static struct sloop_timeout * get_timeout(void)
{
	struct dlist_head * entry;
	struct sloop_timeout * target;

	if (dlist_empty(&sloop.free_timeout))
	{
		d_error("sloop: no sloop_timeout available !!!\n");
		return NULL;
	}
	entry = sloop.free_timeout.next;
	dlist_del(entry);
	target = dlist_entry(entry, struct sloop_timeout, list);
	target->flags = SLOOP_INUSED | SLOOP_TYPE_TIMEOUT;
	return target;
}

#if !defined(__ECOS__)
/* get signal from pool */
static struct sloop_signal * get_signal(void)
{
	struct dlist_head * entry;
	struct sloop_signal * target;

	if (dlist_empty(&sloop.free_signals))
	{
		d_error("sloop: no sloop_signal available !!!\n");
		return NULL;
	}
	entry = sloop.free_signals.next;
	dlist_del(entry);
	target = dlist_entry(entry, struct sloop_signal, list);
	target->flags = SLOOP_INUSED | SLOOP_TYPE_SIGNAL;
	return target;
}
#endif

/* return socket to pool */
static void free_socket(struct sloop_socket * target)
{
	dassert((target->flags & SLOOP_TYPE_MASK) == SLOOP_TYPE_SOCKET);
	target->flags &= (~SLOOP_INUSED);
	dlist_add(&target->list, &sloop.free_sockets);
}

/* return timeout to pool */
static void free_timeout(struct sloop_timeout * target)
{
	dassert((target->flags & SLOOP_TYPE_MASK) == SLOOP_TYPE_TIMEOUT);
	target->flags &= (~SLOOP_INUSED);
	dlist_add(&target->list, &sloop.free_timeout);
}

#if !defined(__ECOS__)
/* return signal to pool */
static void free_signal(struct sloop_signal * target)
{
	dassert((target->flags & SLOOP_TYPE_SIGNAL) == SLOOP_TYPE_SIGNAL);
	target->flags &= (~SLOOP_INUSED);
	dlist_add(&target->list, &sloop.free_signals);
}
#endif

/**********************************************************************/

static struct sloop_socket * register_socket(int sock,
	sloop_socket_handler handler, void * param, struct dlist_head * head)
{
	struct sloop_socket * entry;

	/* allocate a new structure sloop_socket */
	entry = get_socket();
	if (entry == NULL) return NULL;

	/* setup structure and insert into list. */
	entry->sock = sock;
	entry->param = param;
	entry->handler = handler;
	dlist_add(&entry->list, head);
	SLOOPDBG(d_dbg("sloop: new socket : 0x%x (fd=%d)\n", (unsigned int)entry, entry->sock));
#if defined(__ECOS__)
#ifdef SLOOP_UDP_AS_SIGNAL
	signal_client_send();
#endif
#endif
	return entry;
}
static void cancel_socket(struct sloop_socket * target, struct dlist_head * head)
{
	struct dlist_head * entry;

	if (target)
	{
		dlist_del(&target->list);
		SLOOPDBG(d_dbg("sloop: free socket : 0x%x\n", (unsigned int)target));
		free_socket(target);
	}
	else
	{
		while (!dlist_empty(head))
		{
			entry = head->next;
			dlist_del(entry);
			target = dlist_entry(entry, struct sloop_socket, list);
			SLOOPDBG(d_dbg("sloop: free socket : 0x%x\n", (unsigned int)target));
			free_socket(target);
		}
	}
#if defined(__ECOS__)
#ifdef SLOOP_UDP_AS_SIGNAL
	signal_client_send();
#endif
#endif
}

#if !defined(__ECOS__)
/* signal handler */
static void sloop_signals_handler(int sig)
{
	d_info("sloop: sloop_signals_handler(%d)\n", sig);
	if (write(sloop.signal_pipe[1], &sig, sizeof(sig)) < 0)
	{
		d_error("sloop: sloop_signals_handler(): Cound not send signal: %s\n", strerror(errno));
	}
}
#endif

/***************************************************************************/
/* sloop APIs */

/* Get system uptime */
long sloop_uptime(void)
{
	return get_uptime();
}

/* sloop module initialization */
void sloop_init(void * sloop_data)
{
	memset(&sloop, 0, sizeof(sloop));
	INIT_DLIST_HEAD(&sloop.readers);
	INIT_DLIST_HEAD(&sloop.writers);
#if !defined(__ECOS__)
	INIT_DLIST_HEAD(&sloop.signals);
#endif
	INIT_DLIST_HEAD(&sloop.timeout);
	INIT_DLIST_HEAD(&sloop.free_sockets);
	INIT_DLIST_HEAD(&sloop.free_timeout);
#if !defined(__ECOS__)
	INIT_DLIST_HEAD(&sloop.free_signals);
#endif
	init_list_pools();
#if !defined(__ECOS__)
	pipe(sloop.signal_pipe);
#endif
	sloop.sloop_data = sloop_data;
}

/* register a read socket */
sloop_handle sloop_register_read_sock(int sock, sloop_socket_handler handler, void * param)
{
	return register_socket(sock, handler, param, &sloop.readers);
}

/* register a write socket */
sloop_handle sloop_register_write_sock(int sock, sloop_socket_handler handler, void * param)
{
	return register_socket(sock, handler, param, &sloop.writers);
}

/* cancel a read socket */
void sloop_cancel_read_sock(sloop_handle handle)
{
	cancel_socket((struct sloop_socket *)handle, &sloop.readers);
}

/* cancel a write socket */
void sloop_cancel_write_sock(sloop_handle handle)
{
	cancel_socket((struct sloop_socket *)handle, &sloop.writers);
}

#if !defined(__ECOS__)
/* register a signal handler */
sloop_handle sloop_register_signal(int sig, sloop_signal_handler handler, void * param)
{
	struct sloop_signal * entry;

	/* allocate a new structure sloop_signal */
	entry = get_signal();
	if (entry == NULL) return NULL;

	/* setup structure and insert into list. */
	entry->sig = sig;
	entry->param = param;
	entry->handler = handler;
	dlist_add(&entry->list, &sloop.signals);
	SLOOPDBG(d_dbg("sloop: sloop_register_signal(%d)\n", sig));
	signal(sig, sloop_signals_handler);
	return entry;
}

/* cancel a signal handler */
void sloop_cancel_signal(sloop_handle handle)
{
	struct sloop_signal * entry = (struct sloop_signal *)handle;
	struct dlist_head * list;

	if (handle)
	{
		SLOOPDBG(d_dbg("sloop: sloop_cancel_signal(%d)\n", entry->sig));
		signal(entry->sig, SIG_DFL);
		dlist_del(&entry->list);
		free_signal(entry);
	}
	else
	{
		while (!dlist_empty(&sloop.signals))
		{
			list = sloop.signals.next;
			entry = dlist_entry(list, struct sloop_signal, list);
			SLOOPDBG(d_dbg("sloop: sloop_cancel_signal(%d)\n", entry->sig));
			signal(entry->sig, SIG_DFL);
			dlist_del(list);
			free_signal(entry);
		}
	}
}
#endif

/* register a timer  */
sloop_handle sloop_register_timeout(unsigned int secs, unsigned int usecs, sloop_timeout_handler handler, void * param)
{
	struct sloop_timeout * timeout, * tmp;
	struct dlist_head * entry;

	/* allocate a new struct sloop_timeout. */
	timeout = get_timeout();
	if (timeout == NULL) return NULL;

#ifdef SLOOP_USE_GETTIMEOFDAY
	gettimeofday(&timeout->time, NULL);
	timeout->time.tv_sec += secs;
	timeout->time.tv_usec += usecs;
#else
#if !defined(__ECOS__)
	timeout->time.tv_sec = get_uptime() + secs;
	timeout->time.tv_usec = usecs;
#else
	get_uptime_detail(&timeout->time.tv_sec,&timeout->time.tv_usec);
	timeout->time.tv_sec += secs;
	timeout->time.tv_usec += usecs;
#endif
#endif
	while (timeout->time.tv_usec >= 1000000)
	{
		timeout->time.tv_sec++;
		timeout->time.tv_usec -= 1000000;
	}
	timeout->handler = handler;
	timeout->param = param;
	INIT_DLIST_HEAD(&timeout->list);

	/* put into the list */
	if (dlist_empty(&sloop.timeout))
	{
		dlist_add(&timeout->list, &sloop.timeout);
		SLOOPDBG(d_dbg("sloop: timeout(0x%x) added !\n", timeout));
#if defined(__ECOS__)
#ifdef SLOOP_UDP_AS_SIGNAL
	signal_client_send();
#endif
#endif
		return timeout;
	}

	entry = sloop.timeout.next;
	while (entry != &sloop.timeout)
	{
		tmp = dlist_entry(entry, struct sloop_timeout, list);
		if (timercmp(&timeout->time, &tmp->time, <)) break;
		entry = entry->next;
	}
	dlist_add_tail(&timeout->list, entry);

	SLOOPDBG(d_dbg("sloop: timeout(0x%x) added !!\n", timeout));
#if defined(__ECOS__)
#ifdef SLOOP_UDP_AS_SIGNAL
	signal_client_send();
#endif
#endif
	return timeout;
}

/* cancel the timer */
void sloop_cancel_timeout(sloop_handle handle)
{
	struct sloop_timeout * entry = (struct sloop_timeout *)handle;
	struct dlist_head * list;

	if (handle)
	{
		dlist_del(&(entry->list));
		SLOOPDBG(d_dbg("sloop: sloop_cancel_timeout(0x%x)\n", handle));
		free_timeout(entry);
	}
	else
	{
		while (!dlist_empty(&sloop.timeout))
		{
			list = sloop.timeout.next;
			dlist_del(list);
			entry = dlist_entry(list, struct sloop_timeout, list);
			SLOOPDBG(d_dbg("sloop: sloop_cancel_timeout(0x%x)\n", handle));
			free_timeout(entry);
		}
	}
#if defined(__ECOS__)
#ifdef SLOOP_UDP_AS_SIGNAL
	signal_client_send();
#endif
#endif
}

void sloop_run(void)
{
	fd_set rfds;
	fd_set wfds;
	struct timeval tv, now;
	struct sloop_timeout * entry_timeout = NULL;
	struct sloop_socket * entry_socket;
#if !defined(__ECOS__)
	struct sloop_signal * entry_signal;
#endif
	struct dlist_head * entry;
	int max_sock;
	int res;
	int sig;
#if !defined(__ECOS__)
	while (!sloop.terminate &&
		   (!dlist_empty(&sloop.timeout) || !dlist_empty(&sloop.readers) ||
			!dlist_empty(&sloop.writers) || !dlist_empty(&sloop.signals)))
#else
#ifdef SLOOP_UDP_AS_SIGNAL
	o_signal_server_sock = signal_server_sock();
#endif
	while (!sloop.terminate)
#endif
	{
		/*****************************************
		 * setup the tv
		 */

		/* Do we have timeout event ? */
		if (!dlist_empty(&sloop.timeout))
		{
			entry = sloop.timeout.next;
			entry_timeout = dlist_entry(entry, struct sloop_timeout, list);
		}
		else
		{
			entry_timeout = NULL;
		}
		/* preprare tv for timeout. */
		if (entry_timeout)
		{
#ifdef SLOOP_USE_GETTIMEOFDAY
			gettimeofday(&now, NULL);
#else
			timerclear(&now);
#if !defined(__ECOS__)
			now.tv_sec = get_uptime();
#else
			get_uptime_detail(&now.tv_sec,&now.tv_usec);
#endif
#endif
			if (timercmp(&now, &entry_timeout->time, >=))
				tv.tv_sec = tv.tv_usec = 0;
			else
				timersub(&entry_timeout->time, &now, &tv);
			SLOOPDBG(d_dbg("sloop: sloop_run(): next timeout in %lu.%06lu sec\n", tv.tv_sec, tv.tv_usec));
		}

		/******************************************
		 * setup FDs
		 */

		/* reset FDs first */
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		max_sock = 0;

#if !defined(__ECOS__)
		/* signals */
		FD_SET(sloop.signal_pipe[0], &rfds);
		if (max_sock < sloop.signal_pipe[0]) max_sock = sloop.signal_pipe[0];
#endif

		/* readers */
		for (entry = sloop.readers.next; entry != &sloop.readers; entry = entry->next)
		{
			entry_socket = dlist_entry(entry, struct sloop_socket, list);
			FD_SET(entry_socket->sock, &rfds);
			if (max_sock < entry_socket->sock) max_sock = entry_socket->sock;
		}
		/* writers */
		for (entry = sloop.writers.next; entry != &sloop.writers; entry = entry->next)
		{
			entry_socket = dlist_entry(entry, struct sloop_socket, list);
			FD_SET(entry_socket->sock, &wfds);
			if (max_sock < entry_socket->sock) max_sock = entry_socket->sock;
		}

		/**********************************************
		 * enter select loop
		 */
#if !defined(__ECOS__)
		SLOOPDBG(d_dbg("sloop: >>> enter select sloop !!\n"));
		res = select(max_sock + 1, &rfds, &wfds, NULL, entry_timeout ? &tv : NULL);
		SLOOPDBG(d_dbg("sloop: <<< exit select sloop !! (%d)%s\n", res, res<0&&errno==EINTR ? " EINTR":""));
#else
#ifdef SLOOP_UDP_AS_SIGNAL
		if(o_signal_server_sock >= 0)
		{
			FD_SET(o_signal_server_sock, &rfds);
			if (max_sock < o_signal_server_sock) max_sock = o_signal_server_sock;
		}
		SLOOPDBG(d_dbg("sloop: >>> enter select sloop !!\n"));
		res = select(max_sock + 1, &rfds, &wfds, NULL, entry_timeout ? &tv : NULL);
		SLOOPDBG(d_dbg("sloop: <<< exit select sloop !! (%d)%s\n", res, res<0&&errno==EINTR ? " EINTR":""));
#else
		tv.tv_sec=1; tv.tv_usec=0; //add by siyou. bc. we are share by all process.
		SLOOPDBG(d_dbg("sloop: >>> enter select sloop !!\n"));
		res = select(max_sock + 1, &rfds, &wfds, NULL, &tv);
		SLOOPDBG(d_dbg("sloop: <<< exit select sloop !! (%d)%s\n", res, res<0&&errno==EINTR ? " EINTR":""));
#endif
#endif
		if (res < 0)
		{
			if (errno == EINTR)
			{
				d_info("sloop: sloop_run(): EINTR!\n");
				continue;
			}
			else
			{
				d_error("sloop: sloop_run(): select error (%s)!\n", strerror(errno));
				break;
			}
		}

#if !defined(__ECOS__)
		/* check signal first */
		if (res > 0 && FD_ISSET(sloop.signal_pipe[0], &rfds))
		{
			if (read(sloop.signal_pipe[0], &sig, sizeof(sig)) < 0)
			{
				/* probabaly just EINTR */
				d_error("sloop: sloop_run(): Could not read signal: %s\n", strerror(errno));
			}
			else if (sig == 0)
			{
				d_info("sloop: get myself signal !!\n");
			}
			else if (!dlist_empty(&sloop.signals))
			{
				for (entry = sloop.signals.next; entry != &sloop.signals; entry = entry->next)
				{
					entry_signal = dlist_entry(entry, struct sloop_signal, list);
					if (entry_signal->sig == sig)
					{
						if (entry_signal->handler(entry_signal->sig, entry_signal->param, sloop.sloop_data)<0)
						{
							dlist_del(entry);
							free_signal(entry_signal);
						}
						break;
					}
				}
				if (sloop.terminate) break;
			}
			else
			{
				SLOOPDBG(d_info("sloop: should not be here !!\n"));
			}
		}
#endif

		/* check if someone timeout. */
		if (entry_timeout)
		{
			if (sloop.timeout.next == &entry_timeout->list)
			{
#ifdef SLOOP_USE_GETTIMEOFDAY
				gettimeofday(&now, NULL);
#else
				timerclear(&now);
#if !defined(__ECOS__)
				now.tv_sec = get_uptime();
#else
				get_uptime_detail(&now.tv_sec,&now.tv_usec);
#endif
#endif
#if !defined(__ECOS__)
				if (res == 0 || timercmp(&now, &entry_timeout->time, >=))
#else
				if ( timercmp(&now, &entry_timeout->time, >=))
#endif
				{
					if (entry_timeout->handler)
						entry_timeout->handler(entry_timeout->param, sloop.sloop_data);
					dlist_del(&entry_timeout->list);
					free_timeout(entry_timeout);
				}
			}
			else
			{
				SLOOPDBG(d_info("sloop: timeout (0x%x) is gone, should be canceled !!!\n", entry_timeout));
			}
		}

		/* do we have fds to process ? */
		if (res <=0) continue;
#if defined(__ECOS__)
#ifdef SLOOP_UDP_AS_SIGNAL
		if (FD_ISSET(o_signal_server_sock, &rfds))
		{
			char garbag[32];//maybe queue many command
			recv(o_signal_server_sock, garbag,sizeof(garbag), 0);
			//diag_printf("got sig %c\n",garbag[0]);
			//continue;
		}
#endif
#endif
		/* checking readers */
		if (!dlist_empty(&sloop.readers))
		{
			entry = sloop.readers.next;
			while (entry != &sloop.readers)
			{
				entry_socket = dlist_entry(entry, struct sloop_socket, list);
				if (FD_ISSET(entry_socket->sock, &rfds))
					res = entry_socket->handler(entry_socket->sock, entry_socket->param, sloop.sloop_data);
				else
					res = 0;
				entry = entry->next;

				if (res < 0)
				{
					/* if handler return -1, cancel this read sock. */
					SLOOPDBG(d_info("sloop: remove entry (0x%08x)!!!\n", entry_socket));
					dlist_del(&entry_socket->list);
					free_socket(entry_socket);
				}
			}
		}

		/* checking writers */
		if (!dlist_empty(&sloop.writers))
		{
			entry = sloop.writers.next;
			while (entry != &sloop.writers)
			{
				entry_socket = dlist_entry(entry, struct sloop_socket, list);
				if (FD_ISSET(entry_socket->sock, &wfds))
					res = entry_socket->handler(entry_socket->sock, entry_socket->param, sloop.sloop_data);
				else
					res = 0;
				entry = entry->next;

				if (res < 0)
				{
					/* if handler return -1, cancel this write sock. */
					dlist_del(&entry_socket->list);
					free_socket(entry_socket);
				}
			}
		}
	}
#if !defined(__ECOS__)
	sloop_cancel_signal(NULL);
#endif
	sloop_cancel_timeout(NULL);
	sloop_cancel_read_sock(NULL);
	sloop_cancel_write_sock(NULL);
}

void sloop_terminate(void)
{
	sloop.terminate = 1;
}

#if DEBUG_SLOOP_DUMP
static void sloop_dump_socket(struct dlist_head * head)
{
	struct dlist_head * entry;
	struct sloop_socket * socket;

	entry = head->next;
	while (entry != head)
	{
		socket = dlist_entry(entry, struct sloop_socket, list);
		printf("socket(0x%x), fd(%d), param(0x%x), handler(0x%x)\n",
				(unsigned int)socket, socket->sock, (unsigned int)socket->param, (unsigned int)socket->handler);
		entry = entry->next;
	}
}
void sloop_dump_readers(void)
{
	printf("=================================\n");
	printf("sloop readers\n");
	sloop_dump_socket(&sloop.readers);
	printf("---------------------------------\n");
}
void sloop_dump_writers(void)
{
	printf("=================================\n");
	printf("sloop writers\n");
	sloop_dump_socket(&sloop.writers);
	printf("---------------------------------\n");
}
void sloop_dump_timeout(void)
{
	struct dlist_head * entry;
	struct sloop_timeout * timeout;

	printf("=================================\n");
	printf("sloop timeout\n");
	entry = sloop.timeout.next;
	while (entry != &sloop.timeout)
	{
		timeout = dlist_entry(entry, struct sloop_timeout, list);
		printf("timeout(0x%x), time(%d:%d), param(0x%x), handler(0x%x)\n",
				(unsigned int)timeout, (int)timeout->time.tv_sec, (int)timeout->time.tv_usec,
				(unsigned int)timeout->param, (unsigned int)timeout->handler);
		entry = entry->next;
	}
	printf("---------------------------------\n");
}

#if !defined(__ECOS__)
void sloop_dump_signals(void)
{
	struct dlist_head * entry;
	struct sloop_signal * signal;

	printf("=================================\n");
	printf("sloop signals\n");
	entry = sloop.signals.next;
	while (entry != &sloop.signals)
	{
		signal = dlist_entry(entry, struct sloop_signal, list);
		printf("signals(0x%x), sig(%d), param(0x%x), handler(0x%x)\n",
				(unsigned int)signal, signal->sig, (unsigned int)signal->param, (unsigned int)signal->handler);
		entry = entry->next;
	}
	printf("---------------------------------\n");
}
#endif

void sloop_dump(void)
{
	sloop_dump_readers();
	sloop_dump_writers();
	sloop_dump_timeout();
	sloop_dump_signals();
}
#endif

#if 0
static int SIGINT_handler(int sig, void * param, void * sloop_data)
{
	printf("SIGINT_handler(%d, 0x%x, 0x%x)\n", sig, (unsigned int)param, (unsigned int)sloop_data);
	sloop_terminate();
	return 0;
}

static int SIGTERM_handler(int sig, void * param, void * sloop_data)
{
	printf("SIGTERM_handler(%d, 0x%x, 0x%x)\n", sig, (unsigned int)param, (unsigned int)sloop_data);
	sloop_terminate();
	return 0;
}

static int SIGUSR1_handler(int sig, void * param, void * sloop_data)
{
	printf("SIGUSR1_handler(%d, 0x%x, 0x%x)\n", sig, (unsigned int)param, (unsigned int)sloop_data);
	sloop_dump();
	return 0;
}

static sloop_handle h_timeout1 = NULL;
static sloop_handle h_timeout2 = NULL;
static sloop_handle h_timeout3 = NULL;
static sloop_handle h_timeout4 = NULL;

static void timer1_handler(void * param, void * sloop_data)
{
	sloop_handle * handle = (sloop_handle *)param;
	printf("timer1_handler(0x%x, 0x%x)\n", (unsigned int)param, (unsigned int)sloop_data);
	*handle = NULL;
}

static int SIGUSR2_handler(int sig, void * param, void * sloop_data)
{
	printf("SIGUSR2_handler(%d, 0x%x, 0x%x)\n", sig, (unsigned int)param, (unsigned int)sloop_data);
	printf(" handle1 = 0x%x\n", (unsigned int)h_timeout1);
	printf(" handle2 = 0x%x\n", (unsigned int)h_timeout2);
	printf(" handle3 = 0x%x\n", (unsigned int)h_timeout3);
	printf(" handle4 = 0x%x\n", (unsigned int)h_timeout4);
	if (h_timeout1)
	{
		sloop_cancel_timeout(h_timeout1);
		h_timeout1 = NULL;
	}
	else if (h_timeout2)
	{
		sloop_cancel_timeout(h_timeout2);
		h_timeout2 = NULL;
	}
	else if (h_timeout3)
	{
		sloop_cancel_timeout(h_timeout3);
		h_timeout3 = NULL;
	}
	else if (h_timeout4)
	{
		sloop_cancel_timeout(h_timeout4);
		h_timeout4 = NULL;
	}
	else
	{
		h_timeout1 = sloop_register_timeout(50,0, timer1_handler, &h_timeout1);
		h_timeout2 = sloop_register_timeout(10,0, timer1_handler, &h_timeout2);
		h_timeout3 = sloop_register_timeout(30,0, timer1_handler, &h_timeout3);
		h_timeout4 = sloop_register_timeout(40,0, timer1_handler, &h_timeout4);
	}
	printf("SIGUSR2_handler !!!\n");
	return 0;
}

int main(int argc, char * argv[])
{
	printf("SIGINT =%d\n", SIGINT);
	printf("SIGTERM=%d\n", SIGTERM);
	printf("SIGUSR1=%d\n", SIGUSR1);
	printf("SIGUSR2=%d\n", SIGUSR2);

	sloop_init(NULL);

	sloop_register_signal(SIGINT,  SIGINT_handler,  NULL);
	sloop_register_signal(SIGTERM, SIGTERM_handler, NULL);
	sloop_register_signal(SIGUSR1, SIGUSR1_handler, NULL);
	sloop_register_signal(SIGUSR2, SIGUSR2_handler, NULL);

	sloop_run();


	return 0;
}
#endif

#if defined(__ECOS__)
void sloop_main(unsigned long data)
{
	//diag_printf("sloop_main start\n");
	sloop_init(NULL);
	sloop_run();
}


static cyg_handle_t thread;
static cyg_thread thread_obj;
static char stack[10*1024];

void init_thread_sloop(void)
{
	//diag_printf("init_thread_sloop start\n");
	cyg_thread_create(8, (cyg_thread_entry_t  *)sloop_main, (cyg_addrword_t) 0,
		"sloop_main", (void *) stack,
		sizeof(stack), &thread, &thread_obj);
	cyg_thread_resume(thread);
}
#endif
