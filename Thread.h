//
// Created by nadavsh22 on 5/5/18.
//

#ifndef EX2_THREAD_H
#define EX2_THREAD_H

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
inline address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
            "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
inline address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
        "rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

//------------------includes--------------------
#include <vector>
#include <csetjmp>
#include <signal.h>
#include <iostream>
#include "uthreads.h"

//------------------defines--------------------
#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define SYS_ERROR "system error: "
#define THREAD_LIB_ERR "thread library error: "
#define TIMER_ERR "timer mailfunction\n"
#define ALLOC_FAIL "memory allocation caused an issue"
#define SYS_ERR_CODE 1
#define FAILURE -1
//---------------class---------------------------


class Thread
{
private:
    int _tid, _state, _quants;
    bool _imWaiting; //am i waiting for someone
    bool _imDelaying;

    Thread *_imWaitingForTP; // who am i waiting for

    char *_tStack;

    // list of IDs that are waiting for this
    std::vector<int> delayedByMeTids;


public:
    /**
     * Constructor for Thread object
    * @param tid the id for the new thread
    * @param f
    */
    Thread(int tid, void (*f)(void));

    /**
     * destructor
     */
    ~Thread();

    /**
     * return an environment pointer of the thread
     * @return
     */
    sigjmp_buf *getEnv();

    /**
     *
     * @return tid
     */
    int getId();


    /**
     * set the state of the thread either RUNNING,BLOCKED,READY
     * @param newState
     */
    void setState(int newState);

    /**
     *
     * @return threads current state
     */
    int getState() const;

    /**
     * @return threads total quants elpased
     */
    int getQuants() const;

    /**
     * increments the threads quant count by 1
     */
    void incQuants();

    /**
     * inform thread if it is waiting for another thread
     * @param syncState boolean parameter for the flag
     * @param tpSyncer the thread that we'll wait for
     */
    void setImWaiting(bool syncState, Thread *tpSyncer);

    /**
     *
     * @return whether thread is synced and waiting for another
     */
    bool amIwaiting() const;

    /**
     * add thread with tid to the list of threads that are withing for this Thread
     * @param tid
     */
    void addDelayedByMe(int tid);

    /**
     * remove thread with tid from this threads delayed threads list
     * @param tid
     */
    void removeDelayedByMe(int tid);

    /**
     *
     * @return true if threads are synced with this object, false otherwise
     */
    bool get_imDelaying() const;

    /**
     *
     * @return pointer to the thread that im waiting for
     */
    Thread *get_imWaitingForTP() const;

    /**
     *
     * @return pointer to the vector that im delaying.
     */
    const std::vector<int> &getDelayedByMeTids() const;

};

#endif //EX2_THREAD_H
