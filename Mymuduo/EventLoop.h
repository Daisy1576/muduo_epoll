#pragma conce

#include "noncopyable.h"

#include <functional>

#include <atomic>

#include "Timestamp.h"

#include <memory>

#include <mutex>

#include "CurrentThread.h"

#include <vector>

class Channel;
class Poller;



//事件循环类 主要包含了channel,poller(epoll的抽象)
class EventLoop : noncopyable
{
public:
     using Functor = std::function<void()>;

     EventLoop();
     ~EventLoop();

     //开启事件循环
     void loop();
     //退出事件循环
     void quit();

     Timestamp pollReturnTime() const {return pollReturnTime_; }

     //在当前loop执行
     void runInLoop(Functor cb);
     //把cb 放入队列中，唤醒loop所在的线程，执行cb
     void queueInLoop(Functor cb);

     //唤醒loop所在的xianc
     void wakeup();

     //eventloop的方法就是调用poller 的方法
     void updateChannel(Channel *channel);
     void removeChannel(Channel *channel);
     bool hasChannel(Channel *channel);

     //判断eventloop是否在自己的线程中
     bool isInLoopThread() const {return threadId_ == CurrentThread::tid();  }

private:

    void handleRead();
    void doPendingFunctors();

    using Channellist = std::vector<Channel*>;

    std::atomic_bool looping_;  //原子操作，通过cas实现
    std::atomic_bool quit_;  //退出loop循环
   
    const pid_t threadId_;  //记录当前loop所在线程id
     
    Timestamp pollReturnTime_; //poller返回发生事件的channels的时间
    std::unique_ptr<Poller> poller_; 

    int wakeupFd_;  //主要作用，当mianloop获取一个新用户的channel,通过轮询算法选怎一个subloop,通过该成员来唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_;

    Channellist activeChannels_;
 

     std::atomic_bool callingPendingFunctors_;  //标识当前loop是否需要执行的回调操作
     std::vector<Functor> pendingFunctors_; //存储loop需要执行的所有回调操作
     std::mutex mutex_; //互斥锁，用来保护上面vector容器的线程安全操作



};
