#include "thread.h"
#include <string>
#include <iostream>
#include "logger.h"
#include <string.h>

thread_local Thread *ptr = NULL;
thread_local char thread_name[40] = "Main";

void Thread::detach()
{
    if (running_ && th_.joinable())
        th_.detach();
}

Thread::Thread(const std::string &name) : name_(name),
    running_(false), done_(true)
{
}

Thread *Thread::
CurrentThread()
{
    if (ptr)
        return ptr;
    else
        return nullptr;
}

std::string Thread::
CurrentThreadName()
{
    if (ptr)
        return ptr->name_;
    else if (*thread_name)
        return thread_name;
    else
        return Logger::Instance().appname();
}


void Thread::
SetCurrentThreadName(const std::string &n)
{
    if (ptr)
        ptr->name_ = n;
    else
        ::strncpy(thread_name, n.c_str(), sizeof(thread_name) - 1);
}

void *Thread::threadfunc(void *param)
{
    Thread *pThread = static_cast < Thread * >(param);

    ptr = pThread;
    ::strncpy(thread_name, pThread->name_.c_str(), sizeof(thread_name) - 1);

    try {
        pThread->worker_thread();
    }
    catch(std::string &e) {
        ELOG << "Exception in threadfunc: " << e;
    }
    catch(...) {
        // se e' una cancellazione
        if (!pThread->running_) throw;
        ELOG << "(thread " << pThread->name_  << ") unhandled exception caught: (fatal)";
    }

    pThread->done_ = true;
    pThread->running_ = false;

    return NULL;
}
Thread::
~Thread() {
    if (th_.joinable()) {
        ILOG << "Joining unjoined thread " << name_ << " in distructor";
        th_.join();
    };
}
void Thread::terminate(int total_wait)
{
    int die_wait = 0;

    if (done_) {
        if (th_.joinable())
            th_.join();
        return;
    }

    running_ = false;

    while ((!done_) && (die_wait++ < total_wait))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    if (die_wait >= 100) {
        done_ = true;
        WLOG << "thread " << Name() << " refused to die!";
        return;
    }

    if (th_.joinable())
        th_.join();
}

bool Thread::start()
{
    if (running_)
        return true;

    running_ = true;
    done_ = false;

    try {
        if (th_.joinable())
            th_.join();

        th_ = std::thread(threadfunc, this);
    }
    catch(...) {
        running_ = false;
        done_ = true;
        ELOG << "***Unable to create the thread";
        return false;
    }

    return true;
}
