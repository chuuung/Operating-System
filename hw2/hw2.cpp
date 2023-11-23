// #define _GNU_SOURCE
#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <cassert>

using namespace std;

typedef struct {
    pthread_t thread_id;
    int thread_num;
    int sched_policy;
    int sched_priority;
    // float time_wait;
} thread_info_t;

static pthread_mutex_t outputlock;
pthread_barrier_t barrier;


int number_thread;
float time_wait;
vector<string> policies;
vector<int> priorities;
void parse(int argc, char* argv[]){
    int ch;
    while((ch = getopt(argc, argv, "n:t:s:p:")) != -1){ // t:s:p:
        if (ch == 'n') number_thread = stoi(optarg);
        if (ch == 't') time_wait = stof(optarg);
        if (ch == 's'){
            char *p;
            const char* delim = ",";
            p = strtok(optarg, delim);
            while(p != NULL){
                policies.push_back(p);
                p = strtok(NULL, delim);
            }
        }
        if (ch == 'p'){
            char *p;
            const char* delim = ",";
            p = strtok(optarg, delim);
            while(p != NULL){
                priorities.push_back(stoi(p));
                p = strtok(NULL, delim);
            }
        }
    }
}

static double my_clock(void) {
    struct timespec t;
    assert(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) == 0); //get thread clock
    return 1e-9 * t.tv_nsec + t.tv_sec;
}

void *thread_func(void *arg)
{
    thread_info_t *threadParams = (thread_info_t *)arg;
    /* 1. Wait until all threads are ready */
    pthread_barrier_wait(&barrier); //thread is ready
    /* 2. Do the task */ 
    for (int i = 0; i < 3; i++) {
        pthread_mutex_lock(&outputlock);
        {
            
            cout << "Thread " << threadParams->thread_num << " is running"<< endl;
            fflush(stdout);
            // busy for t second
            struct timeval t;
            double sttime = my_clock();
            while (1) {
                if (my_clock() - sttime > 1* time_wait)
                    break;
            }
        }
         pthread_mutex_unlock(&outputlock);

    }
    /* 3. Exit the function  */
    pthread_exit(0);
}

void create_threads_worker(vector<thread_info_t> &threads_info){
    for(int i = 0; i < number_thread; i++){

        thread_info_t cur_thread;
        
        cur_thread.thread_num = i;
        //NORMAL: 1, FIFO:0
        if (policies[i] == "NORMAL") cur_thread.sched_policy = 1;
        else cur_thread.sched_policy = 0;
        
        cur_thread.sched_priority = priorities[i];
        threads_info.push_back(cur_thread);
    }
}

int main(int argc, char* argv[]){
    parse(argc, argv); //Parse program arguments

    /* 2. Create <num_threads> worker threads */
    vector<thread_info_t> threads_info;
    create_threads_worker(threads_info);
    
    /* 3. Set CPU affinity */
    cpu_set_t cpuset;
    //zero out the set o CPU cores
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    
    /* 4. Set the attributes to each thread */
    pthread_barrier_init(&barrier, NULL, number_thread); // set a barrier, let threads execute simultaneously

    pthread_attr_t attr; //thread's attribute

    for(int i = 0; i < number_thread; i++){
        pthread_attr_init(&attr); //init thread attribute
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED); //don't inherit parent process scheduling policy
        // The scheduling policy of a thread can only be modified if it does not inherit the scheduling policy of its parent proces.

        /* set thread scheduling policy and thread priority level*/
        struct sched_param sched_param;
        if (threads_info[i].sched_policy == 0){ // FIFO
            pthread_attr_setschedpolicy(&attr, SCHED_FIFO); //set thread's schedule policy, real time or not
            sched_param.sched_priority = threads_info[i].sched_priority; // setting FIFO thread priority, higher value higher priority
        }
        else{ // Normal
            pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
            sched_param.sched_priority = sched_get_priority_max(SCHED_OTHER); //get maximum priority of schedule
        }
        pthread_attr_setschedparam(&attr, &sched_param);
        
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset); //set thread only execute on cpu 0

        pthread_create( &threads_info[i].thread_id,       //Pointer to thread descriptor
                        &attr,     //Use customized attributes
                        thread_func,         //Thread function entry point
                        (void *)&(threads_info[i]) //thread_func Parameters to pass in, 將指向thread_info[i]的pointer轉換為void *的通用pointer
                      );

    }

    for(int i = 0; i < number_thread; i++){
        pthread_join(threads_info[i].thread_id, NULL); //wait for pthread execute finish
    }
    pthread_barrier_destroy(&barrier);// relase barrier init resource
    return 0;

}