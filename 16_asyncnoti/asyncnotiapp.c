#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"
#include "poll.h"
#include "sys/select.h"
#include "signal.h"

#define CLOSE_CMD		_IO(0XEF, 0X1)
#define OPEN_CMD		_IO(0XEF, 0X2)
#define SETPERIOD_CMD	_IOW(0XEF, 0x3, int)
#define READ_CMD		_IOR(0xEF, 0X4, int)

static int fd = 0;
static void sigio_signal_func(int signum)
{
    int ret = 0;
    unsigned int keyvalue = 0;

    ret = read(fd, &keyvalue, sizeof(keyvalue));
    printf("sigio signal! key value=%d\r\n", keyvalue);
}

int main(int argc, char *argv[])
{
    int ret, flags;
    char *filename;

    if(argc !=2){
        printf("Error Usage!\r\n");
        return -1;
    }

    filename = argv[1];

    fd = open(filename, O_RDWR | O_NONBLOCK);
    if(fd < 0){
        printf("Can't open file %s\r\n", filename);
        return -1;
    }

    /* 设置信号SIGIO的处理函数 */
    signal(SIGIO, sigio_signal_func);
    fcntl(fd, __F_SETOWN, getpid());/* 告诉内核，当前进程的进程号 */
    flags = fcntl(fd, F_GETFD);/* 获取设备文件的flags */
    fcntl(fd, F_SETFL, flags | FASYNC);/* 设备文件中添加FASYNC标志，驱动中就会调用fasync函数，并启动异步通知功能 */

    while(1){
        sleep(2);
    }
    

    usleep(100000);
    ret = close(fd);
    if(ret < 0){
        printf("App close file %s failed!\r\n", filename);
        return -1;
    }
}