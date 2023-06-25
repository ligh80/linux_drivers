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
    char readbuf[100], writebuf[100];

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
    if(atoi(argv[2]) == 1){
        retvalue = read(fd, readbuf, 50);
        if(retvalue < 0){
            printf("App read file %s failed!\r\n", filename);
        }else{
            printf("App read data : %s \r\n", readbuf);
        }
    }

    if(atoi(argv[2]) == 2){
        memcpy(writebuf, usrdata, sizeof(usrdata));
        retvalue = write(fd, writebuf, 50);
        if(retvalue < 0){
            printf("App write file %s failed!\r\n", filename);
        }else{
            printf("App write data : %s\r\n", usrdata);
        }
    }
    usleep(100000);
    retvalue = close(fd);
    if(retvalue < 0){
        printf("App close file %s failed!\r\n", filename);
        return -1;
    }
}