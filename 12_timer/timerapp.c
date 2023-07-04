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
    unsigned char databuf[1];
    unsigned int cmd;
    unsigned int arg;
    unsigned char str[100];


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
        printf("please input cmd:");
        ret = scanf("%d", &cmd);
        if(ret !=1 ){                       /* 参数输入错误 */
            fgets(str, sizeof(str), stdin); /* 防止卡死 */
        }

        if(cmd == 0){                       /* 退出 */
            break;
        } else if(cmd == 1){                /* 关闭 */
            cmd = CLOSE_CMD;
        } else if (cmd == 2){               /* 打开 */
            cmd = OPEN_CMD;
        } else if (cmd == 3){               /* 设置周期 */
            cmd = SETPERIOD_CMD;
            printf("please input timer period:");
            ret = scanf("%d", &arg);
            if(ret !=1 ){                       /* 参数输入错误 */
                fgets(str, sizeof(str), stdin); /* 防止卡死 */
            }
        }
        printf("cmd=%d arg=%d\r\n", cmd, arg);
        ioctl(fd, cmd, arg);                    /* &是取地址符号！ */
    }
    

    usleep(100000);
    ret = close(fd);
    if(ret < 0){
        printf("App close file %s failed!\r\n", filename);
        return -1;
    }
}