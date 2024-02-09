#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data *thread_args = (struct thread_data *)thread_param;
    thread_args->thread_complete_success = false;
    sleep(thread_args->wait_to_obtain_ms/1000);

    int lock_status = pthread_mutex_lock(thread_args->thread_data_mutex);
    if(lock_status != 0){
        ERROR_LOG("Failed to obtain mutex. Error Code %d",lock_status);
    }
    else{
        sleep(thread_args->wait_to_release_ms/1000);
        lock_status = pthread_mutex_unlock(thread_args->thread_data_mutex);
        if(lock_status!=0){
            ERROR_LOG("Failed to unlock mutex. Error Code %d",lock_status);
        }
        else{
            thread_args->thread_complete_success = true;
        }
    }
    return thread_param;
}
bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    DEBUG_LOG("Hello World");
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    bool thread_create_status = false;
    struct thread_data *threadfunc_data = (struct thread_data *)malloc(sizeof(struct thread_data));
    threadfunc_data->thread_data_mutex = mutex;
    threadfunc_data->wait_to_obtain_ms = wait_to_obtain_ms;
    threadfunc_data->wait_to_release_ms = wait_to_release_ms;

    bool status  = pthread_create(thread,NULL,threadfunc,threadfunc_data);
    if(status!=0){
        ERROR_LOG("Failed to create a thread. Error Code %d\r\n",status);
    }
    else{
        DEBUG_LOG("Created Thread Successfully");
        thread_create_status = true;
    }
    
    return thread_create_status;
}

