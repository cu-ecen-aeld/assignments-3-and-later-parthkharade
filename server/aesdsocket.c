#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
bool accept_connection = true;
extern int errno;
#define BACKLOG 20
#define BUFF_SIZE 512
const char *log_path = "/var/tmp/aesdsocketdata";
int caught_signal;
int sockfd;
void signal_handler(int sig_num)
{
    int status;
    syslog(LOG_USER, "Closing Signal : %s\n", strsignal(caught_signal));
    status = close(sockfd);
    if (status != 0)
    {
        syslog(LOG_ERR, "Failed to close socket");
    }
    closelog();
    remove(log_path);
    exit(EXIT_SUCCESS);
}
// reference : https://beej.us/guide/bgnet/html/
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
int main(int argc, char *argv[])
{
    struct sockaddr_storage client_address;
    const char *ident = "AESD-SOCKET";
    openlog(ident, LOG_PID, LOG_USER);
    struct addrinfo hints;
    int status;
    struct addrinfo *res;

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    status = getaddrinfo(NULL, "9000", &hints, &res);
    if (status != 0)
    {
        syslog(LOG_ERR, "getaddrinfo() error: %s\n", gai_strerror(status));
        return -1;
    }
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
    {
        return -1;
    }
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    status = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (status != 0)
    {
        syslog(LOG_ERR, "bind() error: %s\n", strerror(errno));
        freeaddrinfo(res);
        return -1;
    }
    //We don't require res after this. Close this connection.
    freeaddrinfo(res);


    if (argc > 1)
    {
        if (!strcmp(argv[1], "-d"))
        {
            pid_t child_pid = fork();
            if (child_pid == -1)
            {
                syslog(LOG_ERR, "Error creating Daemon\n");
                close(sockfd);
                closelog();
                exit(EXIT_FAILURE);
            }
            else if (child_pid > 0)
            {
                exit(EXIT_SUCCESS);
            }
            else
            {
                // This is the child. Convert this to a daemon.
                if (setsid() == -1)
                {
                    syslog(LOG_ERR, "Error creating new session");
                    close(sockfd);
                    closelog();
                    exit(EXIT_FAILURE);
                }
                chdir("/");

                // Taken from Linux System Programming - Robert Love
                /* redirect fd's 0,1,2 to /dev/null */
                open("/dev/null", O_RDWR);
                /* stdin */
                dup(0);
                /* stdout */
                dup(0);
                /* stderror */
            }
        }
    }
    status = listen(sockfd, BACKLOG);
    if (status != 0)
    {
        syslog(LOG_ERR, "listen() error: %s\n", strerror(errno));
    }

    socklen_t client_address_size = sizeof(client_address);
    while (true) // run infinitely until and error occurs or terminated by sigint.
    {
        int cfd = accept(sockfd, (struct sockaddr *)&client_address, &client_address_size);

        if (cfd == -1)
        {
            syslog(LOG_ERR, "accept() error : %s\n", strerror(errno));
        }

        else
        {
            char peer_ip[INET6_ADDRSTRLEN];
            char rec_buff[BUFF_SIZE];
            rec_buff[BUFF_SIZE - 1] = 0; // Make the buffer null terminated for strchr to work correctly.

            inet_ntop(client_address.ss_family, get_in_addr((struct sockaddr *)&client_address), peer_ip, INET6_ADDRSTRLEN);
            syslog(LOG_USER, "Accepted conncetion from : %s\n", peer_ip);
            
            int log_fd = open(log_path, O_CREAT | O_APPEND | O_RDWR, S_IRWXU | S_IRGRP | S_IROTH);
            int num_read_bytes = 0;
            int num_write_bytes = 0;
            
            if (log_fd == -1)
            {
                syslog(LOG_ERR, "open() error for log file : %s\n", strerror(errno));
                close(cfd);
                break;
            }
            else
            {
                bool packet_complete = false;
                while (!packet_complete)
                {
                    num_read_bytes = recv(cfd, rec_buff, BUFF_SIZE - 1, 0);
                    if (num_read_bytes == -1)
                    {
                        syslog(LOG_ERR, "recv() error : %s\n", strerror(errno));
                        close(cfd);
                        break;
                    }
                    else if (num_read_bytes == 0 || (strchr(rec_buff, '\n') != NULL))
                    {
                        packet_complete = true;
                    }
                    num_write_bytes = write(log_fd, rec_buff, num_read_bytes);
                    if (num_write_bytes != num_read_bytes)
                    {
                        syslog(LOG_ERR, "write() failed error : %s\n", strerror(errno));
                        close(cfd);
                        break;
                    }
                }
                // reinitialize bool before starting send()
                packet_complete = false;
                lseek(log_fd, 0, SEEK_SET); // Reset the cursor to the origin before reading.
                while (!packet_complete)
                {
                    num_read_bytes = read(log_fd, rec_buff, BUFF_SIZE);
                    if (num_read_bytes < BUFF_SIZE)
                    {
                        packet_complete = true;
                    }
                    num_write_bytes = send(cfd, rec_buff, num_read_bytes, 0);
                    if (num_write_bytes != num_read_bytes)
                    {
                        syslog(LOG_ERR, "send() error : %s\n", strerror(errno));
                        close(cfd);
                        break;
                    }
                }


                close(log_fd);
                status = close(cfd);
                if (status != 0)
                {
                    syslog(LOG_ERR, "close() error on cfd : %s\n", strerror(errno));
                    break;
                }
                else
                {
                    syslog(LOG_USER, "Closed connection from %s", peer_ip);
                }
            }
        }
    }

    // exited due to some error on cfd. 
    status = close(sockfd);
    if (status != 0)
    {
        syslog(LOG_ERR, "Failed to close socket");
    }
    closelog();
    remove(log_path);
    return -1; // if it exited the while loop it did so beacuse of a connection error.
}