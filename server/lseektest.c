#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
const char *log_path = "/dev/aesdchar";
int main(){
    int log_fd = open(log_path, O_APPEND | O_WRONLY);
    char read_buff[10];
    int num_write_bytes = write(log_fd, "TEST CODE\n", 10);
    lseek(log_fd,0,SEEK_SET);
    // close(log_fd);
    // log_fd = open(log_path,O_RDONLY);
    int num_read_bytes = read(log_fd,read_buff,10);
    if(num_read_bytes<0){
        printf("Error during read. %s \r\n",strerror(num_read_bytes));
    }
    printf("Read Back %s\r\n", read_buff);
}
