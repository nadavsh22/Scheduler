//------------------includes--------------------
#include "Thread.h"

extern sigjmp_buf _env[MAX_THREAD_NUM];

Thread::Thread(int tid, void (*f)(void)) : _tid(tid),
                                           _state(READY),
                                           _quants(0),
                                           _imWaiting(false),
                                           _imWaitingForTP(nullptr)
{
    try
    {
        _tStack = new char[STACK_SIZE];
    }
    catch (...)
    {
        std::cerr << SYS_ERROR << ALLOC_FAIL << std::endl;
        exit(SYS_ERR_CODE);
    }
    address_t sp, pc;

    sp = (address_t) _tStack + STACK_SIZE - sizeof(address_t);
    pc = (address_t) f;
    sigsetjmp(_env[tid], 1);
    _env[tid]->__jmpbuf[JB_SP] = translate_address(sp);
    _env[tid]->__jmpbuf[JB_PC] = translate_address(pc);
    sigemptyset(&_env[tid]->__saved_mask); // should check for failure?



}

Thread::~Thread()
{

    delete (_tStack);
    delayedByMeTids.clear();

}

sigjmp_buf *Thread::getEnv()
{
    return &_env[_tid];
}

int Thread::getId()
{
    return _tid;
}


void Thread::setState(int newState)
{
    _state = newState;
}

int Thread::getState() const
{
    return _state;
}

int Thread::getQuants() const
{
    return _quants;
}

void Thread::incQuants()
{
    Thread::_quants++;
}

void Thread::setImWaiting(bool syncState, Thread *tpSyncer)
{
    Thread::_imWaiting = syncState;
    if (syncState)
    {
        _imWaitingForTP = tpSyncer;
    }
    else
    {
        _imWaitingForTP = nullptr;
    }
}

bool Thread::amIwaiting() const
{
    return _imWaiting;
}

// add a thread waiting for this
void Thread::addDelayedByMe(int tid)
{
    if (!_imDelaying)
    { _imDelaying = true; }
    delayedByMeTids.push_back(tid);
}

// remove a thread that was waiting for this
void Thread::removeDelayedByMe(int tid)
{
    for (auto it = delayedByMeTids.cbegin(); it != delayedByMeTids.cend(); it++)
    {
        if (tid == *it)
        {
            delayedByMeTids.erase(it);
            if (delayedByMeTids.empty())
            {
                _imDelaying = false;
            }
            return;
        }
    }
}

bool Thread::get_imDelaying() const
{
    return _imDelaying;
}
// is anyone waiting for me

Thread *Thread::get_imWaitingForTP() const
{
    return _imWaitingForTP;
}


const std::vector<int> &Thread::getDelayedByMeTids() const
{
    return delayedByMeTids;
}
