#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "queue.h"
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
#include <pthread.h>
bool accept_connection = true;
extern int errno;
#define BACKLOG 20
#define BUFF_SIZE 1024
const char *log_path = "/var/tmp/aesdsocketdata";

int caught_signal;
int sockfd;
bool signal_rec = false;
bool timer_expired = false;
struct thread_data_s
{
	pthread_t id;
	int cfd;
	char peer_ip[INET6_ADDRSTRLEN];
	bool thread_complete_flag;
	SLIST_ENTRY(thread_data_s)
	next;
};
pthread_mutex_t aesdsock_mutex;
void signal_handler(int sig_num)
{
	signal_rec = true;
	shutdown(sockfd, SHUT_RDWR);
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
void init_timer()
{
	struct itimerval timer_val;
	timer_val.it_value.tv_sec = 10;
	timer_val.it_value.tv_usec = 0;
	timer_val.it_interval.tv_sec = 10;
	timer_val.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer_val, NULL);
}
void timer_handler()
{
	time_t t = time(NULL);
	struct tm *curr_time = localtime(&t);
	char RFC2822BUF[50]={0};
	strftime(RFC2822BUF, sizeof(RFC2822BUF), "timestamp:%a, %d %b %Y %T %Z\n", curr_time);
	int timeStrLen = strlen(RFC2822BUF);
	fflush(stdout);
	pthread_mutex_lock(&aesdsock_mutex);
	int log_fd = open(log_path, O_CREAT | O_APPEND | O_RDWR, S_IRWXU | S_IRGRP | S_IROTH);
	write(log_fd, RFC2822BUF, timeStrLen+1);
	pthread_mutex_unlock(&aesdsock_mutex);
}
int handle_new_connection(void *__thread_data)
{
	/**
	 * Create a tmp file with the name temp(threadID). Delete this file after the copy to aesdsocket is successful.
	 */
	char tmpfile_path[40] = "/tmp/temp";
	struct thread_data_s *thread_data = (struct thread_data_s *)__thread_data;
	pthread_t threadID = pthread_self();
	char threadIDStr[40];
	sprintf(threadIDStr, "%lu", threadID);
	strcat(tmpfile_path, threadIDStr);
	int cfd = thread_data->cfd;
	/**
	 * Buffer to store the received values.
	 */
	char rec_buff[BUFF_SIZE];
	rec_buff[BUFF_SIZE - 1] = 0; // Make the buffer null terminated for strchr to work correctly.

	/**
	 * Open both the temp file and the log file
	 */
	int log_fd = open(log_path, O_CREAT | O_APPEND | O_RDWR, S_IRWXU | S_IRGRP | S_IROTH);
	int tmp_fd = open(tmpfile_path, O_CREAT | O_RDWR, S_IRWXU | S_IRGRP | S_IROTH);

	int num_read_bytes = 0;
	int num_write_bytes = 0;
	int packet_len = 0;
	int ret_code = 0;
	if (log_fd == -1 || tmp_fd == -1)
	{
		printf("Failed to open a file \r\n");
		syslog(LOG_ERR, "open() error for log/temp file : %s\n", strerror(errno));
		ret_code = -1;
	}
	else
	{
		bool packet_complete = false;
		while (!packet_complete)
		{
			/**
			 * Read bytes into a statically allocated buffer.
			 */
			num_read_bytes = recv(cfd, rec_buff, BUFF_SIZE - 1, 0);
			if (num_read_bytes == -1)
			{
				syslog(LOG_ERR, "recv() error : %s\n", strerror(errno));
				ret_code = -1;
				break;
			}
			else
			{
				// Copy yhe buffer into a temp file.
				packet_len += num_read_bytes;

				num_write_bytes = write(tmp_fd, rec_buff, num_read_bytes);
				if (num_write_bytes != num_read_bytes)
				{
					syslog(LOG_ERR, "write() to tmp failed error : %s\n", strerror(errno));
					ret_code = -1;
					break;
				}
			}
			/**
			 * If packet was received copy the entire tmp file into aesd socket.
			 */
			if (num_read_bytes == 0 || (strchr(rec_buff, '\n') != NULL))
			{
				packet_complete = true;
				char *tmp_buff = (char *)malloc(sizeof(char) * packet_len);
				lseek(tmp_fd, 0, SEEK_SET); // Reset the cursor to the origin before reading.
				read(tmp_fd, tmp_buff, packet_len);
				int lock_status = pthread_mutex_lock(&aesdsock_mutex);
				if (lock_status != 0)
				{
					syslog(LOG_ERR, "pthread_mutex_lock() failed. error");
					ret_code = -1;
					printf("Failed to acquire mutex\r\n");
					break;
				}
				else
				{
					num_write_bytes = write(log_fd, tmp_buff, packet_len);
					if (num_write_bytes != packet_len)
					{
						syslog(LOG_ERR, "write() to aesdsock failed error : %s\n", strerror(errno));
						ret_code = -1;
					}
					if (pthread_mutex_unlock(&aesdsock_mutex) != 0)
					{
						syslog(LOG_ERR, "Failed to unlock mutex.");
						printf("Failed to unlock mutex\r\n");
						ret_code = -1;
						break;
					}
					free(tmp_buff);
					remove(tmpfile_path);
				}
			}
		}
		if (ret_code == 0)
		{
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
					ret_code = -1;
					break;
				}
			}
		}

		close(log_fd);
		int status = close(cfd);
		if (status != 0)
		{
			syslog(LOG_ERR, "close() error on cfd : %s\n", strerror(errno));
			ret_code = -1;
		}
		else
		{
			syslog(LOG_USER, "Closed connection from %s", thread_data->peer_ip);
		}
		thread_data->thread_complete_flag = true;
	}
	return ret_code;
}

int main(int argc, char *argv[])
{
	struct sockaddr_storage client_address;
	const char *ident = "AESD-SOCKET";
	openlog(ident, LOG_PID, LOG_USER);
	struct addrinfo hints;
	int status;
	struct addrinfo *res;
	pthread_t thread_id;
	struct thread_data_s *curr_p = NULL;
	struct thread_data_s *temp_p = NULL;
	int exit_status = EXIT_SUCCESS;

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGALRM, timer_handler);
	pthread_mutex_init(&aesdsock_mutex, NULL);
	SLIST_HEAD(threadListHead, thread_data_s)
	head;
	SLIST_INIT(&head);

	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;

	status = getaddrinfo(NULL, "9000", &hints, &res);
	if (status != 0)
	{
		syslog(LOG_ERR, "getaddrinfo() error: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd == -1)
	{
		exit(EXIT_FAILURE);
	}
	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	status = bind(sockfd, res->ai_addr, res->ai_addrlen);
	if (status != 0)
	{
		syslog(LOG_ERR, "bind() error: %s\n", strerror(errno));
		freeaddrinfo(res);
		exit(EXIT_FAILURE);
	}
	// We don't require res after this. Close this connection.
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
	init_timer();
	status = listen(sockfd, BACKLOG);
	if (status != 0)
	{
		syslog(LOG_ERR, "listen() error: %s\n", strerror(errno));
		exit_status = EXIT_FAILURE;
	}

	socklen_t client_address_size = sizeof(client_address);
	while ((!signal_rec) && (exit_status != EXIT_FAILURE)) // run infinitely until and error occurs or terminated by sigint.
	{
		int cfd = accept(sockfd, (struct sockaddr *)&client_address, &client_address_size);
		if (cfd == -1)
		{
			syslog(LOG_ERR, "accept() error : %s\n", strerror(errno));
			exit_status = EXIT_FAILURE;
			break;
		}
		else
		{
			struct thread_data_s *thread_data = (struct thread_data_s *)(malloc(sizeof(struct thread_data_s)));
			if (thread_data == NULL)
			{
				syslog(LOG_ERR, "malloc() error : %s", strerror(errno));
				exit_status = EXIT_FAILURE;
				break;
			}
			inet_ntop(client_address.ss_family, get_in_addr((struct sockaddr *)&client_address), thread_data->peer_ip, INET6_ADDRSTRLEN);
			syslog(LOG_USER, "Accepted conncetion from : %s\n", thread_data->peer_ip);

			thread_data->cfd = cfd;
			thread_data->thread_complete_flag = false;
			status = pthread_create(&thread_id, NULL, (void *)handle_new_connection, thread_data);
			if (status != 0)
			{
				syslog(LOG_ERR, "pthread_create() error : %s", strerror(errno));
				exit_status = EXIT_FAILURE;
				break;
			}
			thread_data->id = thread_id;
			SLIST_INSERT_HEAD(&head, thread_data, next);
		}
		if (timer_expired)
		{
			// https://stackoverflow.com/questions/1442116/how-to-get-the-date-and-time-values-in-a-c-program
			//  time_t t = time(NULL);
			//  struct tm *curr_time = localtime(&t);
			//  char RFC2822BUF[40];
			//  strftime(RFC2822BUF,sizeof(RFC2822BUF),"timestamp:%a, %d %b %Y %T %Z\n",curr_time);
			//  int timeStrLen = strlen(RFC2822BUF);
			//  pthread_mutex_lock(&aesdsock_mutex);
			//  int log_fd = open(log_path, O_CREAT | O_APPEND | O_RDWR, S_IRWXU | S_IRGRP | S_IROTH);
			//  write(log_fd,RFC2822BUF,timeStrLen);
			//  pthread_mutex_unlock(&aesdsock_mutex);
			// log time
			timer_expired = false;
		}
		SLIST_FOREACH_SAFE(curr_p, &head, next, temp_p)
		{
			if (curr_p->thread_complete_flag)
			{
				int status = pthread_join(curr_p->id, NULL);
				if (status != 0)
				{
					syslog(LOG_ERR, "error joining thread ID : %lu", curr_p->id);
					exit_status = EXIT_FAILURE;
				}
				else
				{
					SLIST_REMOVE(&head, curr_p, thread_data_s, next);
					free(curr_p);
				}
			}
		}
	}

	// wait and join all other threads here.
	SLIST_FOREACH_SAFE(curr_p, &head, next, temp_p)
	{
		pthread_join(curr_p->id, NULL);
		free(curr_p);
	}

	status = close(sockfd);
	if (status != 0)
	{
		syslog(LOG_ERR, "Failed to close socket");
		exit_status = EXIT_FAILURE;
	}
	closelog();
	remove(log_path);
	if (signal_rec)
	{
		syslog(LOG_USER, "Closing Signal : %s\n", strsignal(caught_signal));
		exit_status = EXIT_SUCCESS;
	}
	else
	{
		exit_status = EXIT_FAILURE; // if it exited the while loop it did so beacuse of a connection error.
	}
	exit(exit_status);
}
