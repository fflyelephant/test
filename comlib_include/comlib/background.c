#include <cyg/kernel/kapi.h> //this is must when you call any cyg_xxx function.
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <cyg/infra/diag.h>
#include <elbox_config.h>

#define BACKGROUND_PRIORITY	10
#define BACKGROUND_STACKSIZE (8*1024*2)
#define BACKGROUND_THREAD_NUM	8
#define BACKGROUND_WAIT_NUM   3
#define BACKGROUND_COMMAND_SIZE	250
struct _background
{
char					*cmd;
int						pid;
cyg_handle_t 	thread_handle;
cyg_thread 		thread_obj;
char					*stack;
};

static struct _background	Background[BACKGROUND_THREAD_NUM];
static int gBGStop; //background stop.

static void bg_prog(cyg_addrword_t data)
{
extern void process_cmd(char*);
struct _background	*bg = (struct _background *)data;

//	diag_printf("Bg cmd = %s\n", bg->cmd);
	process_cmd(bg->cmd);
	bg->pid = -1;
#if defined(ELBOX_PROGS_PRIV_SERVD)
	extern void signal_child(void);
	signal_child();
#endif
	cyg_thread_exit();
}

static cyg_mutex_t bg_mutex;

void background_init_mutex(void)
{
int i;

	cyg_mutex_init(&bg_mutex);

	for(i=0; i < BACKGROUND_THREAD_NUM; i++)
	{
		Background[i].stack = malloc(BACKGROUND_STACKSIZE);
		Background[i].cmd = malloc(BACKGROUND_COMMAND_SIZE);
	}

}


void suspend_task(void)
{
int i;

	cyg_mutex_lock(&bg_mutex);
	for(i=0; i < BACKGROUND_THREAD_NUM; i++)
	{
		if ( Background[i].thread_handle != 0)
		{
		int id = cyg_thread_get_id(Background[i].thread_handle);
		cyg_thread_info info;

			memset(&info, 0, sizeof(info));
			if ( cyg_thread_get_info(Background[i].thread_handle, id, &info) )
			{
				if ( info.state != 16) //EXIT state. hope they won't change this.
				{
					cyg_thread_suspend(Background[i].thread_handle);
				}
			}
		}
	}
	cyg_mutex_unlock(&bg_mutex);
	gBGStop = 1;
}

void free_background(void)
{
int i;
	//suspend all task.
	suspend_task();
	for(i=0; i < BACKGROUND_THREAD_NUM; i++)
	{
		free(Background[i].stack);
		free(Background[i].cmd);
	}

}

static int gPid=100;
int background_cmd(char *cmd)
{
	int i;
	int wait_num;

//diag_printf("0006 Enter background: cmd=%s\n",cmd);	//spirit test

	if (gBGStop == 1)
	{
		diag_printf("Background is suspend\n");
		return -1;
	}

	//lets lock
	cyg_mutex_lock(&bg_mutex);
	for(wait_num=0; wait_num < BACKGROUND_WAIT_NUM; wait_num++)
	{
		for(i=0; i < BACKGROUND_THREAD_NUM; i++)
		{
			if ( Background[i].thread_handle == 0)
				break;
			else
			{
				int id = cyg_thread_get_id(Background[i].thread_handle);
				cyg_thread_info info;

				memset(&info, 0, sizeof(info));
				if ( cyg_thread_get_info(Background[i].thread_handle, id, &info) )
				{
					if ( info.state == 16) //EXIT state. hope they won't change this.
					{
						cyg_thread_delete(Background[i].thread_handle);
						break;
					}
				}
			}
		}
		
		if ( i == BACKGROUND_THREAD_NUM )
        	{
			cyg_thread_delay(100);
             		continue;
       		}
		else
		{
			break;
		}
	}
	//cyg_mutex_unlock(&bg_mutex);

	if ( i == BACKGROUND_THREAD_NUM )
	{
		diag_printf("No empty background thread available\n");
		cyg_mutex_unlock(&bg_mutex);
		return -1;
	}

	strncpy(Background[i].cmd, cmd, BACKGROUND_COMMAND_SIZE-1);
	Background[i].pid = gPid++;

	cyg_thread_create(BACKGROUND_PRIORITY, bg_prog, (cyg_addrword_t)&Background[i] ,
			"bg_thread", (void *) Background[i].stack,
			BACKGROUND_STACKSIZE, &Background[i].thread_handle, &Background[i].thread_obj);
	cyg_mutex_unlock(&bg_mutex);//kingkong change this,we have to pretect the "Background[]" before the  Background[i].thread_handle have set.
	cyg_thread_resume(Background[i].thread_handle);

	return Background[i].pid;
}

int waitpid(int pid, int *status, int flag)
{
int i;

	if ( pid >= gPid )
		return -1;

	//check if anyone has this pid.
	for(i=0; i < BACKGROUND_THREAD_NUM; i++)
	{
		if ( Background[i].pid == pid )
		{
			*status = 0; //still running.
			return pid;
		}
	}

	//can't find, this mean already exit.
	*status = 0x7f; //exit code need << 8.
	return pid;

}
