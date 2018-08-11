//------------------includes--------------------

#include <setjmp.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include "Scheduler.h"
#include "uthreads.h"

//---------------global variables----------------
bool availableIds[MAX_THREAD_NUM];
Scheduler *manager;
sigjmp_buf _env[MAX_THREAD_NUM];
sigset_t set;
struct sigaction sa;

//--------------ERRORS----------------------
#define THREAD_ID_ERR "thread id 'tid' non existent"
#define THREAD_BLOCK_ERR "thread block unsuccessful"
#define THREAD_SYNC_ERR "thread sync unsuccessful"
#define THREAD_RESUME_ERR "resume of thread unsuccesful"
#define THREAD_TERMINATE_ERR "termination of thread unsuccessful"
#define THREAD_SPAWN_ERR "initialization of thread unsuccessful"
#define THREAD_SIG_ERR "sigaction error"

//--------------functions-------------------
void gKillThreadWithID()
{
    int killID = manager->get_tidTBT();
    Thread *nextThread = manager->get_nextThread();
    manager->_killThread(killID);

    siglongjmp(*(manager->getEnvById(nextThread->getId())), 1);
}

/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
    try
    {
        manager = new Scheduler(quantum_usecs);
    } catch (...)
    {
        std::cerr << SYS_ERROR << ALLOC_FAIL << std::endl;
        exit(SYS_ERR_CODE);
    }
    sa.sa_handler = switchThreadWrapper;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr << THREAD_LIB_ERR << THREAD_SIG_ERR << std::endl;
        return FAILURE;

    }
    manager->startTimer();
    return 0;
}


/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void))
{
    //block signal
    sigprocmask(SIG_BLOCK, &set, NULL);

    //creates new thread and returns its tid, if unsuccessful will return -1
    int newThreadID = manager->createNewThread(f);
    if (newThreadID == FAILURE)
    {
        std::cerr << THREAD_LIB_ERR << THREAD_SPAWN_ERR << std::endl;
    }
    //unblock signal
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    return newThreadID;
}

/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{

    if (tid < 0 || tid >= MAX_THREAD_NUM)
    {
//        if (tid == 0)
//        { printf("hi"); }
        {
            std::cerr << THREAD_LIB_ERR << THREAD_ID_ERR << std::endl;
        }
        return FAILURE;
    } // illegal TID

    //block signal
    sigprocmask(SIG_BLOCK, &set, NULL);
    if (tid == MAIN_TID)
    {
        int runningTid = manager->getCurrentTid();
        if (runningTid != MAIN_TID)
        {
            // switch the current stack to the main thread's stack so that we can safely delete
            // the running thread's stack

            sigjmp_buf *runningImg = manager->getEnvById(runningTid);
            int retVal = sigsetjmp(*runningImg, 1);
            if (retVal != 1)
            {
                jmp_buf *mainImg = manager->getEnvById(MAIN_TID);
                ((*runningImg)->__jmpbuf)[JB_SP] = ((*mainImg)->__jmpbuf)[JB_SP];
                siglongjmp(*runningImg, 1);
            }
        }

        manager->killEmAll(runningTid); // make sure running thread is killed last
        manager->_killThread(runningTid);
        delete (manager);
        exit(0); // exit and delete scheduler

    } // if succeeded, won't continue because we closed everything with exit(0);

    int terminateSuccess = manager->terminateThread(tid);
    if (terminateSuccess == FAILURE)
    {
        std::cerr << THREAD_LIB_ERR << THREAD_TERMINATE_ERR << std::endl;
        return FAILURE;
    }
    //unblock signal
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    return terminateSuccess;

}


/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
    if (tid <= 0 || tid >= MAX_THREAD_NUM)
    {

        std::cerr << THREAD_LIB_ERR << THREAD_ID_ERR << std::endl;
        return FAILURE;
    } // NO BLOCKING THE MAIN THREAD, or an illegal TID

    //block signal
    sigprocmask(SIG_BLOCK, &set, NULL);

    //creates new thread and returns its tid, if unsuccessful will return -1
    int blockSuccess = manager->blockThread(tid);
    if (blockSuccess == FAILURE)
    {
        std::cerr << THREAD_LIB_ERR << THREAD_BLOCK_ERR << std::endl;
        return FAILURE;
    }
    //unblock signal
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    return blockSuccess;

}


/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{

    if (tid < 0 || tid >= MAX_THREAD_NUM)
    {
        std::cerr << THREAD_LIB_ERR << THREAD_ID_ERR << std::endl;
        return FAILURE;
    } // no such tid

    //block signal
    sigprocmask(SIG_BLOCK, &set, NULL);

    //creates new thread and returns its tid, if unsuccessful will return -1
    int resumeSuccess = manager->resumeThread(tid);
    if (resumeSuccess == FAILURE)
    {
        std::cerr << THREAD_LIB_ERR << THREAD_RESUME_ERR << std::endl;
        return FAILURE;
    }
    //unblock signal
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    return resumeSuccess;
}


/*
 * Description: This function blocks the RUNNING thread until thread with
 * ID tid will terminate. It is considered an error if no thread with ID tid
 * exists, if thread tid calls this function or if the main thread (tid==0) calls this function.
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision
 * should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sync(int tid)
{
    if (tid <= 0 || tid >= MAX_THREAD_NUM)
    {

        std::cerr << THREAD_LIB_ERR << THREAD_ID_ERR << std::endl;
        return FAILURE;
    } // no such tid

    //block signal
    sigprocmask(SIG_BLOCK, &set, NULL);

    //creates new thread and returns its tid, if unsuccessful will return -1
    int syncSuccess = manager->syncThread(tid);
    if (syncSuccess == FAILURE)
    {
        std::cerr << THREAD_LIB_ERR << THREAD_SYNC_ERR << std::endl;
        return FAILURE;
    }
    //unblock signal
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    return syncSuccess;

}


/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid()
{
    return manager->getCurrentTid();
}


/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums()
{
    return manager->getTotalQuants();
}

/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
    if (tid >= MAX_THREAD_NUM || tid < 0)
    {

        std::cerr << THREAD_LIB_ERR << THREAD_ID_ERR << std::endl;
        return FAILURE;
    }

    return manager->getThreadQuants(tid);
}


