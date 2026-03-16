#pragma once

#include "noncopyable.h"

#include <functional>

#include <string>

#include <vector>

#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThradPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThradPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThradPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    //如果工作在多线程中，baseloop_默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoop();

    bool started() const { return started_; }
    const std::string name() const { return name_; }
private:
    EventLoop *baseLoop_;
    std::string  name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;  //事件的线程
    std::vector<EventLoop*> loops_;  //事件线程的eventloop的指针

}