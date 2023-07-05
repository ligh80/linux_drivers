#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"

#define CLOSE_CMD		_IO(0XEF, 0X1)
#define OPEN_CMD		_IO(0XEF, 0X2)
#define SETPERIOD_CMD	_IOW(0XEF, 0x3, int)
#define READ_CMD		_IOR(0xEF, 0X4, int)

int main(int argc, char *argv[])
{
    int fd, ret;
    char *filename;
    unsigned char data;

    if(argc !=2){
        printf("Error Usage!\r\n");
        return -1;
    }

    filename = argv[1];

    fd = open(filename, O_RDWR);
    if(fd < 0){
        printf("Can't open file %s\r\n", filename);
        return -1;
    }

    while (1)
    {     
        ret = read(fd, &data, sizeof(data));
        if (ret >= 0){
            if (data)
                printf("key value = %#X\r\n", data);
        }
        scanf
    }
    

    usleep(100000);
    ret = close(fd);
    if(ret < 0){
        printf("App close file %s failed!\r\n", filename);
        return -1;
    }
}