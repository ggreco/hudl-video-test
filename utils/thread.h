#pragma once

#include <string>
#include <thread>
#include <atomic>

class Thread
{
    std::string name_;
    std::atomic<bool> running_;
    std::atomic<bool> done_;
    std::thread th_;
protected:
	virtual void worker_thread() = 0;
public:
    void Name(const std::string &n) { name_ = n; }
    const std::string &Name() const { return name_; }
    static void SetCurrentThreadName(const std::string &name);
    static std::string CurrentThreadName();
    static Thread *CurrentThread();
	Thread(const std::string &name = "child thread");
	virtual ~Thread();
	void terminate(int total_wait = 100);
    void detach();
    void stop() { running_ = false; }
	bool running() const { return running_; }
	bool start();
	void join() { if (th_.joinable()) th_.join(); }
    static void *threadfunc(void *);
};