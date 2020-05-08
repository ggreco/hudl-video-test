#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

/** A threadsafe-queue implementation */
template <class T>
class SafeQueue
{
public:
  /** Add an element to the tail of the queue. */
  void enqueue(T t)
  {
    std::lock_guard<std::mutex> lock(m);
    q.push(t);
    c.notify_one();
  }
  /** Add multiple elements to the tail of the queue. */
  void enqueue(const std::vector<T> &t)
  {
    std::lock_guard<std::mutex> lock(m);
    for (auto &e: t)
      q.push(e);

    c.notify_all();
  }
  /** Get the "front"-element from the queue, if it's empty, wait till an element is available. */
  T dequeue(void)
  {
    std::unique_lock<std::mutex> lock(m);
    while(q.empty())
    {
      // release lock as long as the wait and reaquire it afterwards.
      c.wait(lock);
    }
    T val = q.front();
    q.pop();
    return val;
  }
  T next() const {
      std::lock_guard<std::mutex> lock(m);
      if (!q.empty())
        return q.front();
      
      return T();
  }
   /** Get multiple elements from the queue, if the queue is empty wait at most msecs milliseconds, the vector may be returned empty.*/
  void dequeue(std::vector<T> &results, int msecs) {
    results.clear();
    std::unique_lock<std::mutex> lock(m);
    if (q.empty()) {
        if (c.wait_for(lock, std::chrono::milliseconds(msecs)) != std::cv_status::no_timeout || q.empty())
            return;
    }
    while (!q.empty()) {
      results.push_back(q.front());
      q.pop();
    }
  }
  /** Get an element from the queue, if the queue is empty wait at most msecs milliseconds, then returns false if no item was available.*/
  bool dequeue(T &val, int msecs) {
    std::unique_lock<std::mutex> lock(m);
    if (q.empty()) {
        if (c.wait_for(lock, std::chrono::milliseconds(msecs)) != std::cv_status::no_timeout || q.empty())
            return false;
    }
    val = q.front();
    q.pop();
    return true;
  }
  void pop() {
    std::unique_lock<std::mutex> lock(m);
    if (!q.empty())
        q.pop();
  }
  /** returns if the queue is empty */
  bool empty() const {
    std::unique_lock<std::mutex> lock(m);
    return q.empty();
  }
  /** returns the number of elements in the queue */
  size_t size() const {
     std::unique_lock<std::mutex> lock(m);
     return q.size();
  }
  /** clear the queue */
  void clear() {
     std::unique_lock<std::mutex> lock(m);
     while (!q.empty())
         q.pop();
  }
  /** Copy the contents of the queue, needs a mutex */
  SafeQueue<T> &operator=(const SafeQueue<T> &src) {
    if (this != &src) {
        std::lock(m, src.m);
        std::lock_guard<std::mutex> lhs_lk(m, std::adopt_lock);
        std::lock_guard<std::mutex> rhs_lk(src.m, std::adopt_lock);
        q = src.q;
    }
    return *this;
  }
  /** Dumps the queue to a vector */
  void dump(std::vector<T> &out) {
    m.lock();
    std::queue<T> temp = q;
    m.unlock();
    out.clear();
    if (!temp.empty()) {
      out.reserve(temp.size());
      while (!temp.empty()) {
        out.push_back(temp.front());
        temp.pop();
      }
    }
  }
private:
  std::queue<T> q;
  mutable std::mutex m;
  std::condition_variable c;
};
