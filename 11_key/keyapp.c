#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#define KEY0VALUE	0XF0	/* 按键值 */
#define INVAKEY		0X00	/* 无效的按键值 */


int main(int argc, char *argv[])
{
    int fd, retvalue;
    char *filename;
    unsigned char keyvalue = 0;


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
        read(fd, &keyvalue, sizeof(keyvalue));
        if(keyvalue == KEY0VALUE){
            printf("KEY0 Press, value = %#X\r\n", keyvalue);
        }
    }
    

    usleep(100000);
    retvalue = close(fd);
    if(retvalue < 0){
        printf("App close file %s failed!\r\n", filename);
        return -1;
    }
}