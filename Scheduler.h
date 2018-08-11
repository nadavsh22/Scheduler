
#ifndef EX2_SCHEDULER_H
//------------------defines--------------------

#define EX2_SCHEDULER_H
#define MAIN_TID 0
#define MICRO_SECS 1000000

//------------------includes--------------------
#include <deque>
#include "Thread.h"
#include <sys/time.h>

void gKillThreadWithID();

void switchThreadWrapper(int sig);

class Scheduler
{
private:
    //--------------------members--------------------------------

    int _numThreads;
    int _quantumsPassed; // counter
    int _quantumUSecs, _quantumSecs;
    struct itimerval _timer;
    std::deque<Thread *> _readyFreddie;
    Thread *_tidMap[MAX_THREAD_NUM];
    Thread *_currentThread;

    // used for when deleting current thread
    Thread *_nextThread;
    int _tidTBT;
    char _extraStack[STACK_SIZE];

    jmp_buf _extraBuf;

    //--------------------functions------------------------------
    /**
     *
     * @return the next available id for a new thread
     */
    int _getNextAvailableID();

    /**
     *
     * @return the pointer to the next thread that is ready
     */
    Thread *_popNextThread();

public:

    /**
     * kill all threads except for the excluded tid
     * @param excluded tid of thread to exclude from killing, -1 if non should be excluded
     */
    void killEmAll(int excluded);

    /**
     * Construct new scheduler
     * @param quantumUsecs definition of class's quantum
     */
    Scheduler(int quantumUsecs);

    /**
     * destructor of scheduler
     */
    ~Scheduler();

    /**
     * kill thread with tid
     * @param tid
     */
    void _killThread(int tid);

    /**
     * creates new thread and adds it to queue
     * @param f the function represented by thread
     * @return 0 on success
     */
    int createNewThread(void (*f)(void));

    /**
     * terminates thread with tid
     * @param tid
     * @return 0 on success, -1 otherwise
     */
    int terminateThread(int tid);

    /**
     * block thread with tid
     * @param tid
     * @return 0 on success, -1 on failure
     */
    int blockThread(int tid);

    /**
     * resume blocked thread with tid
     * @param tid
     * @return 0 on success, -1 on failure
     */
    int resumeThread(int tid);

    /**
     * sync the current thread with the thread 'tid'
     * @param tid
     * @return 0 on success, -1 otherwise
     */
    int syncThread(int tid);

    /**
     *
     * @return current threads' tid
     */
    int getCurrentTid();

    /**
     *
     * @return number of total quants elapsed in scheduler
     */
    int getTotalQuants();

    /**
     *
     * @param tid
     * @return quantum count of thread with tid
     */
    int getThreadQuants(int tid);

    /**
     * start/restart timer
     */
    void startTimer();

    void stopTimer();

    /**
     * switch between threads on signal sig
     * @param sig currently unused
     */
    void threadSwitch(int sig);

    /**
     * used by teminate to kill threads
     * @return 0 on success, -1 otherwise
     */
    int get_tidTBT() const;

    /**
     *
     * @return next thread on ready queue
     */
    Thread *get_nextThread() const;

    /**
     *
     * @param tid
     * @return environment of thread tid
     */
    sigjmp_buf *getEnvById(int tid);

    /**
     * terminate tid when it's the current thread
     * @param tid
     * @return 0 on success -1 otherwise
     */
    int terminateSelf(int tid);
};


#endif //EX2_SCHEDULER_H
