#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

static char usrdata[] = {"usr data!"};

int main(int argc, char *argv[])
{
    int fd, retvalue;
    char *filename;
    unsigned char cnt = 0;
    unsigned char databuf[1];

    if(argc !=3){
        printf("Error Usage!\r\n");
    }

    filename = argv[1];

    fd = open(filename, O_RDWR);
    if(fd < 0){
        printf("Can't open file %s\r\n", filename);
        return -1;
    }

    printf("App start read/write\r\n");
    usleep(100000);   

    databuf[0] = atoi(argv[2]);
    retvalue = write(fd, databuf, sizeof(databuf));
    if(retvalue < 0){
        printf("ledApp control failed!\r\n");
        close(fd);
    }else{
        printf("ledApp control successed!\r\n");
    }

    /* 延迟25s,模拟占用atomic设备 */
    while (1)
    {
        sleep(5);
        cnt++;
        printf("App running times:%d\r\n", cnt);
        if (cnt >= 5)   break;        
    }
    

    usleep(100000);
    retvalue = close(fd);
    if(retvalue < 0){
        printf("App close file %s failed!\r\n", filename);
        return -1;
    }
}