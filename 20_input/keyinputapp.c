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
#include "linux/input.h"

#define CLOSE_CMD		_IO(0XEF, 0X1)
#define OPEN_CMD		_IO(0XEF, 0X2)
#define SETPERIOD_CMD	_IOW(0XEF, 0x3, int)
#define READ_CMD		_IOR(0xEF, 0X4, int)

/* 定义一个input_event变量，存放输入事件信息 */
static struct input_event input_event;


int main(int argc, char *argv[])
{
    int fd, ret;
    char *filename;

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

    while(1){
        ret = read(fd, &input_event, sizeof(input_event));
        if(ret > 0) {/* 读取数据成功 */
            switch (input_event.type) {
            case EV_KEY:
                if(input_event.code < BTN_MISC) {
                    printf("key %d %s\r\n", input_event.code, 
                    input_event.value ? "pressed":"unpressed");
                } else {
                    printf("button %d %s\r\n", input_event.code, 
                    input_event.value ? "pressed":"unpressed");
                }
                break;
            /* 其他类型的事件 */
            case EV_REL:
                break;
            default:
                break;
            }
        } else {
            printf("读取数据失败\r\n");
        }
    }
    usleep(100000);
    ret = close(fd);
    if(ret < 0){
        printf("App close file %s failed!\r\n", filename);
        return -1;
    }
}