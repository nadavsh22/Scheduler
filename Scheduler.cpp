//------------------includes--------------------
#include "Scheduler.h"


extern sigjmp_buf _env[MAX_THREAD_NUM];
extern Scheduler *manager;
extern sigset_t set;

//------------------functions-------------------
// written so that we can call this from the signal alarm handler in uthreads
void switchThreadWrapper(int sig)
{
    manager->threadSwitch(sig);
}

Scheduler::Scheduler(int quantumUsecs) : _numThreads(1), _quantumsPassed(1),
                                         _quantumUSecs(quantumUsecs % MICRO_SECS),
                                         _quantumSecs(quantumUsecs / MICRO_SECS),
                                         _tidMap(),
                                         _currentThread(),
                                         _nextThread(nullptr),
                                         _tidTBT(-1),
                                         _extraStack{0}
{
    try
    {
        _currentThread = new Thread(MAIN_TID, nullptr);
    }
    catch (...)
    {
        std::cerr << SYS_ERROR << ALLOC_FAIL << std::endl;
    }

    for (auto &i : _tidMap)
    {
        i = nullptr;
    }
    _tidMap[MAIN_TID] = _currentThread;
    _currentThread->incQuants();

}

Scheduler::~Scheduler()
{

    killEmAll(-1);
}

void Scheduler::killEmAll(int excluded)
{
    stopTimer();
    for (int i = MAX_THREAD_NUM - 1; i > 0; i--)
    {
        if (_tidMap[i] != nullptr && i != excluded)
        {
            _killThread(i);
        }
    }
}

int Scheduler::createNewThread(void (*f)(void))
{
    int newID = _getNextAvailableID();
    if (newID == -1)
    { return -1; }
    try
    {
        auto newThread = new Thread(newID, f);
        _readyFreddie.push_back(newThread);
        _tidMap[newID] = newThread;
        _numThreads++;
        return newID;
    }
    catch (...)
    {

        std::cerr << SYS_ERROR << ALLOC_FAIL << std::endl;
        exit(SYS_ERR_CODE);
    }
}

Thread *Scheduler::_popNextThread()
{
    if (_readyFreddie.empty())
    { return nullptr; }
    Thread *retVal = _readyFreddie.front();
    _readyFreddie.pop_front();
    // retVal->incQuants();
    //_quantumsPassed++;
    return retVal;
}

int Scheduler::_getNextAvailableID()
{
    for (int i = 0; i < MAX_THREAD_NUM; ++i)
    {
        if (_tidMap[i] == nullptr)
        { return i; }
    }
    return -1;
}

void Scheduler::threadSwitch(int sig)
{
    // sigprocmask(SIG_BLOCK, &set, nullptr); // make sure we don't get another alarm..
    stopTimer();

    Thread *newThread = _popNextThread();
    int retVal = sigsetjmp(_env[_currentThread->getId()], 1);
    if (retVal == 1)
    {
        return;
    }
    if (newThread != nullptr) // current thread is not the only one
    {
        // switch threads;
        _quantumsPassed++;
        newThread->incQuants();
        _readyFreddie.push_back(_currentThread);
        _currentThread = newThread;
        startTimer();
        siglongjmp(_env[_currentThread->getId()], 1);
    }
    else
    {

        _currentThread->incQuants();
        _quantumsPassed++;
        startTimer();
//            stopTimer();
//            sigprocmask(SIG_UNBLOCK, &set, nullptr);
//            startTimer();
    }

//    }

}

int Scheduler::blockThread(int tid)
{
    if (tid == _currentThread->getId()) // Thread blocking itself
    {
        Thread *newThread = _popNextThread();

        newThread->setState(RUNNING);
        _currentThread->setState(BLOCKED);
        Thread *currRunnning = _currentThread;
        _currentThread = newThread;
        _currentThread->incQuants();
        _quantumsPassed++;

        // switch
        int retVal = sigsetjmp(*getEnvById(currRunnning->getId()), 1);
        if (retVal != 1)
        {
//            stopTimer();
            startTimer();
            siglongjmp(*getEnvById(_currentThread->getId()), 1);
        }

    }
    // else: block other thread

    for (auto it = _readyFreddie.cbegin(); it != _readyFreddie.cend(); it++)
    {
        Thread *tp = *it;
        if (tid == tp->getId())
        {
            tp->setState(BLOCKED);
            _readyFreddie.erase(it);
            return 0;
        }
    } // thread not found in queue - it is either block or synced

    Thread *tp = _tidMap[tid];
    if (tp == nullptr) // no such thread exists
    {
        return -1;
    }
    else if (tp->getState() == BLOCKED)
    { return 0; } // already blocked
    else
    {
        tp->setState(BLOCKED); // thread is not in queue - it's (probably) synced
        return 0;
    }
}

/*
 * executes the process of terminating a thread, deciding the actions to be taken according to
 * thread state
 */
int Scheduler::terminateThread(int tid)
{
    if (_tidMap[tid] == nullptr) // trying to kill non-existent thread
    {
        return -1;
    }

    if (_currentThread->getId() == tid)
    {
        return terminateSelf(tid);
    }

    _killThread(tid);
    return 0;
}

int Scheduler::terminateSelf(int tid)
{
    Thread *newThread = _popNextThread();
    manager->_nextThread = newThread;
    manager->_tidTBT = tid;

    _currentThread = _nextThread;

//    stopTimer();

    address_t sp = (address_t) _extraStack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) gKillThreadWithID;
    sigsetjmp(_extraBuf, 1);
    _extraBuf->__jmpbuf[JB_SP] = translate_address(sp);
    _extraBuf->__jmpbuf[JB_PC] = translate_address(pc);
    sigemptyset(&(_extraBuf->__saved_mask)); // should check for failure?
    startTimer();
    siglongjmp(_extraBuf, 1);
}

/*
 * thread delete and free synced threads
 */
void Scheduler::_killThread(int tid)
{
    Thread *threadToTerminate = _tidMap[tid];
    //remove the TBK from any thread that might be blocking it
    if (threadToTerminate->amIwaiting())
    {
        threadToTerminate->get_imWaitingForTP()->removeDelayedByMe(tid);
    }
    //un-sync all threads that are waiting for thread TBK
    if (threadToTerminate->get_imDelaying())
    {
        std::vector<int> delayedList = threadToTerminate->getDelayedByMeTids();
        for (auto it = delayedList.cbegin(); it != delayedList.cend(); it++)
        {
            Thread *tp = _tidMap[*it];
            tp->setImWaiting(false, nullptr);
            if (tp->getState() != BLOCKED)
            {
                _readyFreddie.push_back(tp);
            }
        }

    }
    //check if in ready list and delete
    for (auto it = _readyFreddie.cbegin(); it != _readyFreddie.cend(); it++)
    {
        Thread *tp = *it;
        if (tid == tp->getId())
        {
            _readyFreddie.erase(it);
            delete threadToTerminate;
            _tidMap[tid] = nullptr;
            return;
        }
    }
    //delete anyways
    _tidMap[tid] = nullptr;
    delete threadToTerminate;

}

int Scheduler::resumeThread(int tid)
{
    if (_tidMap[tid] == nullptr)
    { return -1; }

    Thread *threadToResume = _tidMap[tid];

    if (threadToResume->getState() == BLOCKED)
    {
        threadToResume->setState(READY);
        if (!threadToResume->amIwaiting())
        {
            _readyFreddie.push_back(threadToResume);
        }
    }
    return 0;
}

//block running thread by tid
int Scheduler::syncThread(int tid)
{
    Thread *delayingTp = _tidMap[tid];
    if (delayingTp == nullptr) // there is no thread with tid
    { return -1; }

    Thread *newThread = _popNextThread(); // we assume there are other threads
    if (newThread == nullptr)
    {
        return -1; // ERRORRRRR
    }
    newThread->setState(RUNNING);
    _currentThread->setState(READY); // we assume it isn't blocked because it's running..
    _currentThread->setImWaiting(true, delayingTp);
    delayingTp->addDelayedByMe(_currentThread->getId());
    Thread *currRunnning = _currentThread;
    _currentThread = newThread;
    _currentThread->incQuants();
    _quantumsPassed++;

    // switch
    int retVal = sigsetjmp(*getEnvById(currRunnning->getId()), 1);
    if (retVal != 1)
    {
//        stopTimer();
        startTimer();
        siglongjmp(*getEnvById(_currentThread->getId()), 1);
    }

    return 0; // what happens when a synced thread continues?
}


int Scheduler::getCurrentTid()
{
    return _currentThread->getId();
}

int Scheduler::getTotalQuants()
{
    return _quantumsPassed;
}

int Scheduler::getThreadQuants(int tid)
{
    if (_tidMap[tid] == nullptr)
    { return -1; }
    return _tidMap[tid]->getQuants();
}

void Scheduler::startTimer()
{
    _timer.it_value.tv_sec = _quantumSecs;
    _timer.it_value.tv_usec = _quantumUSecs;
    _timer.it_interval.tv_sec = _quantumSecs;
    _timer.it_interval.tv_usec = _quantumUSecs;
    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer(ITIMER_VIRTUAL, &_timer, NULL))
    {
        std::cerr << SYS_ERROR << TIMER_ERR << std::endl;
    }
}

void Scheduler::stopTimer()
{
    _timer.it_value.tv_sec = 0;        // first time interval, seconds part
    _timer.it_value.tv_usec = 0;        // first time interval, microseconds part
    _timer.it_interval.tv_sec = 0;    // following time intervals, seconds part
    _timer.it_interval.tv_usec = 0;    // following time intervals, microseconds part
    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer(ITIMER_VIRTUAL, &_timer, NULL))
    {
        std::cerr << SYS_ERROR << TIMER_ERR << std::endl;
    }
}

int Scheduler::get_tidTBT() const
{
    return _tidTBT;
}

Thread *Scheduler::get_nextThread() const
{
    return _nextThread;
}

sigjmp_buf *Scheduler::getEnvById(int tid)
{
    return &_env[tid];
}