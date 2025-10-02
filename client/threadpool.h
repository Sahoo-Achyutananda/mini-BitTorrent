#if !defined(THREADPOOL_H)
#define THREADPOOL_H

/*
I watched this video - https://www.youtube.com/watch?v=WmDOHh7k0Ag for this !
*/

#include <pthread.h>
#include <queue>
#include <string>
#include <unistd.h>
#include "./tracker/colors.h"
#include "client_constructs.h"

using namespace std;

// Forward declaration
class DownloadInfo;
struct PieceSeederInfo;

// Task structure for download jobs
class DownloadTask {
public :
    string fileName;
    int pieceIndex;
    string expectedHash;
    PieceSeederInfo seeder;
    
    DownloadTask(string fname, int idx, string hash, PieceSeederInfo seed): fileName(fname), pieceIndex(idx), expectedHash(hash), seeder(seed){}
};

// Thread Pool Class
class ThreadPool {
private:
    pthread_t* workers; // will hold a set of worker threads
    int numThreads; // total threads -
    queue<DownloadTask*> taskQueue;
    pthread_mutex_t queueMutex; // for mutual exclusion access into the queue
    pthread_cond_t queueCondition; // for signalling other threads and stuff
    bool shutdown;
    int activeTasks;
    pthread_mutex_t activeTasksMutex;
    
public:
    ThreadPool(int threads){
        numThreads = threads;
        shutdown = false;
        activeTasks = 0;
        
        pthread_mutex_init(&queueMutex, NULL);
        pthread_mutex_init(&activeTasksMutex, NULL);
        pthread_cond_init(&queueCondition, NULL);
        
        workers = new pthread_t[numThreads];
        
        // Create worker threads
        for(int i = 0; i < numThreads; i++){
            pthread_create(&workers[i], NULL, workerThread, this);
        }
        
        cout << fontBold << colorGreen << "Thread pool initialized with " << numThreads << " worker threads" << reset << endl;
    }
    
    // need to use a deconstructor - why is the needede ??
    /*
    1. im using raw resources - workers = new pthread_t[numThreads]; (line 50)
    
    */
    ~ThreadPool(){
        pthread_mutex_lock(&queueMutex);
        shutdown = true;
        pthread_cond_broadcast(&queueCondition);
        pthread_mutex_unlock(&queueMutex);
        
        // Wait for all threads to finish
        for(int i = 0; i < numThreads; i++){
            pthread_join(workers[i], NULL);
        }
        
        delete[] workers;
        pthread_mutex_destroy(&queueMutex);
        pthread_mutex_destroy(&activeTasksMutex);
        pthread_cond_destroy(&queueCondition);
    }
    
    // Add a task to the queue
    void addTask(DownloadTask* task){
        pthread_mutex_lock(&queueMutex);
        taskQueue.push(task);
        pthread_cond_signal(&queueCondition);
        pthread_mutex_unlock(&queueMutex);
    }
    
    // Get number of pending tasks
    int pendingTasks(){
        pthread_mutex_lock(&queueMutex);
        int size = taskQueue.size();
        pthread_mutex_unlock(&queueMutex);
        return size;
    }
    
    // Get number of active tasks being processed
    int getActiveTasks(){
        pthread_mutex_lock(&activeTasksMutex);
        int active = activeTasks;
        pthread_mutex_unlock(&activeTasksMutex);
        return active;
    }
    
    // Check if thread pool is idle (no pending or active tasks)
    bool isIdle(){
        return (pendingTasks() == 0 && getActiveTasks() == 0);
    }
    
    // Worker thread function
    static void* workerThread(void* arg){
        ThreadPool* pool = (ThreadPool*)arg;
        
        while(true){
            pthread_mutex_lock(&pool->queueMutex);
            
            // Wait for tasks or shutdown signal
            while(pool->taskQueue.empty() && !pool->shutdown){
                pthread_cond_wait(&pool->queueCondition, &pool->queueMutex);
            }
            
            if(pool->shutdown && pool->taskQueue.empty()){
                pthread_mutex_unlock(&pool->queueMutex);
                break;
            }
            
            // Get task from queue
            DownloadTask* task = pool->taskQueue.front();
            pool->taskQueue.pop();
            
            pthread_mutex_unlock(&pool->queueMutex);
            
            // Increment active tasks
            pthread_mutex_lock(&pool->activeTasksMutex);
            pool->activeTasks++;
            pthread_mutex_unlock(&pool->activeTasksMutex);
            
            // Execute the task
            pool->executeTask(task);
            
            // Decrement active tasks
            pthread_mutex_lock(&pool->activeTasksMutex);
            pool->activeTasks--;
            pthread_mutex_unlock(&pool->activeTasksMutex);
            
            delete task;
        }
        
        return NULL;
    }
    
    // Execute a download task
    void executeTask(DownloadTask* task);
};

// Global thread pool instance
ThreadPool* globalDownloadPool = nullptr;

// Initialize global thread pool 
// Apparantly there is a formula to calculate the total threads needed -- for simplicity : total threads = total cores LOL
// for getting total cores i need to use some system commands - not sure if it is allowed !
/*
resources - 
https://www.youtube.com/watch?v=kdrnu0xSdbs
https://stackoverflow.com/questions/44614931/ideal-thread-pool-size-in-other-multi-threaded-framework
*/

void initializeThreadPool(){
    if(globalDownloadPool != nullptr) return;
    long long optimalThreads = 8;

    /////////////////////////////////////
    int numCores = sysconf(_SC_NPROCESSORS_ONLN);
    optimalThreads = numCores;
    /////////////////////////////////////

    globalDownloadPool = new ThreadPool(optimalThreads);
    
    cout << fontBold << colorBlue<< "Using " << optimalThreads << " download threads." << reset << endl;
}

// Cleanup thread pool
void cleanupThreadPool(){
    if(globalDownloadPool){
        delete globalDownloadPool;
        globalDownloadPool = nullptr;
    }
}

#endif // THREADPOOL_H