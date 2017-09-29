#include <termios.h>  
#include <string.h>
#include <stdio.h>  
#include <stdlib.h>
#include <signal.h> 
#include <fcntl.h> 
#include <unistd.h>
#define DEFAULT_USERNAME    "ws3cd" 
#define DEFAULT_PASSWORD    "new_home!secu6-aar" 
#define DEFAULT_LEN         64
struct termios oldtermios, newtermios;  

static void wait_enter()
{
    char    c;
    while((c = getchar()) != EOF && c != '\n') {
        ;
    }
    printf("************************\n");
    printf("*    Please login      *\n");
    printf("************************\n");
    return;
}

static void block_signal(sigset_t *newmask, sigset_t *oldmask)
{
    sigemptyset(newmask);
#if 0
    sigfillset(newmask);// block every signal
#endif
    sigaddset(newmask, SIGINT);
    sigaddset(newmask, SIGTSTP);
    if(sigprocmask(SIG_BLOCK, newmask, oldmask) < 0) {
        fprintf(stderr,"Sigprocmask error\n");  
        return ;
    }  
}

static void revert_all_signal(sigset_t *oldmask)
{
    if(sigprocmask(SIG_BLOCK, oldmask, NULL) < 0) {
        fprintf(stderr,"Sigprocmask error\n");  
        return ;
    }   
}

static int get_login_name(char *name)
{   
    char    c, *ptr = NULL;
    ptr = name;
    printf("Enter name: ");  
    while((c = getchar()) != EOF && c != '\n') {
    // 将用户输入的数据只读前DEFAULT_LEN个字节, 后续的也要用getchar从缓冲区读出, 防止这些数据被
    // get_login_passwd函数从缓冲区读出来当做密码输入(或者有丢弃缓冲区数据的方法?)
        if(ptr < &name[DEFAULT_LEN])
            *ptr++ = c;
    }
    *ptr = '\0';
    return 0;
}

static int get_login_passwd(char *password)
{
    char     c, *ptr = NULL;
    tcgetattr(STDIN_FILENO, &oldtermios);  
    newtermios = oldtermios;  
    ptr = password;
    // 关闭ECHOX系列, 并输出'*'来替代, 关闭ICANON使用非标准模式, 使得终端一次读取一个字节
    newtermios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON);

    printf("Enter password: ");  
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &newtermios) != 0) {  
        fprintf(stderr,"Could not set attributes\n");  
        return -1;
    }  
    else {  
        while((c = getchar()) != EOF && c != '\n') {
            if(ptr < &password[DEFAULT_LEN]) {
                *ptr++ = c;
                putchar('*');               
            }
        }
        *ptr = '\0';
        tcsetattr(STDIN_FILENO, TCSANOW, &oldtermios); 
    } 
    return 0;
}

int main()  
{  
    sigset_t  newmask, oldmask;
    char login_name[DEFAULT_LEN + 1]     = {0};// 多一个字符用于在64字节填满时尾部添加'\0'
    char login_password[DEFAULT_LEN + 1] = {0};
    int  retrycount = 0;
    int  rebootflag = 0;
    //FILE *fp = NULL;
#if 0
    if((fp = fopen(ctermid(NULL), "r+")) == NULL){
        perror("Open terminal with RDWR error!");
        return -1;
    }
#endif
#if 1
    if(isatty(STDIN_FILENO) != 1) {
        fprintf(stderr,"STDIN_FILENO is not a terminal device!\n");  
        return -1;
    }
#endif

    block_signal(&newmask, &oldmask);
    wait_enter();
    while(retrycount++ < 3)
    {
        memset(login_name, 0, DEFAULT_LEN + 1);
        memset(login_password, 0, DEFAULT_LEN + 1);

        get_login_name(login_name);
        //printf("Your name:%s len:%d compare:%d\n", login_name, strlen(login_name), strcmp(login_name, DEFAULT_USERNAME)); 
        get_login_passwd(login_password);
        //printf("\nYour passwd:%s len:%d compare:%d\n", login_password, strlen(login_password), strcmp(login_password, DEFAULT_PASSWORD)); 

        if(strcmp(login_name, DEFAULT_USERNAME)==0 && strcmp(login_password, DEFAULT_PASSWORD)==0) {
            printf("\nlogin in!\n");
            revert_all_signal(&oldmask);
            exit(0);
        }
        if(retrycount > 2)
            rebootflag = 1;
        printf("\n\tPlease retry!\n");
    }
    revert_all_signal(&oldmask);
    if(rebootflag){
        printf("will be reboot, and terminal do not receive anything input during rebooting!\n");
        system("reboot");
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &newtermios) != 0) {  
            fprintf(stderr,"Could not set attributes\n");  
            return -1;
        }  
    }
    exit(0);  
}