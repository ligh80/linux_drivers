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

#define CLOSE_CMD		_IO(0XEF, 0X1)
#define OPEN_CMD		_IO(0XEF, 0X2)
#define SETPERIOD_CMD	_IOW(0XEF, 0x3, int)
#define READ_CMD		_IOR(0xEF, 0X4, int)

int main(int argc, char *argv[])
{
    int fd, ret;
    char *filename;
    unsigned char data;
    fd_set readfds;
    struct timeval timeout;

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

    while (1)
    {   
        /* 构造FD_SET */
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        /* 构造超时时间 */
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000000;/* 500ms */
        ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
        printf("select ret = %d\r\n", ret);
        switch(ret){
            case 0:
                break;
            default:
                ret = read(fd, &data, sizeof(data));
                printf("key value=%d\r\n", data);
        }
    }
    

    usleep(100000);
    ret = close(fd);
    if(ret < 0){
        printf("App close file %s failed!\r\n", filename);
        return -1;
    }
}