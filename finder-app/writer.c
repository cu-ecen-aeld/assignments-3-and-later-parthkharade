/*
 * Date : 01/28/2023
 * Author : Parth Kharade
 * Description : writer utility to open a file and write text to it. To be used as an alternative to writer.sh
 * */
#include "syslog.h"
#include "stdio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
int main(int argc ,char *argv[]){
	
	const char *ident = "AESD-WRITER";
	openlog(ident,LOG_PERROR|LOG_PID,LOG_USER);
	if(argc-1 != 2){
		syslog(LOG_ERR,"Exepcted 2 arguments , received %d",argc-1);
		syslog(LOG_ERR,"Format command path_to_file string_to_write"); 
		return 1;
		// send output to log file over here
	}
	else{
		char *filename = argv[1];
		char *string  = argv[2];

		int file_desc;

		file_desc = open(filename, O_TRUNC|O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);

		if(file_desc == -1){
		syslog(LOG_ERR,"Failed to open/create File : %s",filename);
		return 1;
		}

		syslog(LOG_DEBUG, "File: %s  opened/created for write",filename);
		// get filename and string
		// Open file with write permissions.
		// Check for result of open if failed write to log file and exit.
		// Try writing string to file.
		int nBytes = write(file_desc,string,strlen(string));
		if(nBytes == -1){
			syslog(LOG_ERR, "Failed to write data to File : %s",filename);
			return 1;
		}
		else if(nBytes < strlen(string)){
			syslog(LOG_ERR, "Write failed to File :%s . Attemped to write %d bytes but wrote %ld",filename,nBytes, strlen(string));
			return 1;
		}
		else{
			syslog(LOG_DEBUG, "Successfully wrote data to File : %s",filename);
		}
		
		int closestatus = close(file_desc);
		if(closestatus == -1){
			syslog(LOG_ERR,"Failed to close File : %s.",filename);
			return 1;
		}
		else{
			syslog(LOG_DEBUG, "Successfully closed File :%s",filename);
			return 0;
		}
		// Check if write succeceded. Log appropriately.
	}
}
