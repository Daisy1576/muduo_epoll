#include "Channel.h"

#include "EventLoop.h"

#include <sys/epoll.h>

#include "Logger.h"

const int Channel::KNoneEvent = 0;
const int Channel::KReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::KWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{
}

Channel::~Channel()
{
}

//tie在channel什么时候被调用
 void Channel::tie(const std::shared_ptr<void> &obj)
 {
    tie_ = obj;
    tied_ = true;
 }

 //当改变channnel所表示的fd的events事件后，update负责在poller里面更改fd相应的事件epoll-ctl
  void Channel::update()
  {
    //通过cahnnel所属的eventloop，调用poller的相应方法，注册fd的events事件
    //add code
    //loop_->updateChannel(this);
  }

  //在channel所属的eventloop中，把当前的channel删除
 void Channel::remove()
 {
    //add code 
    //loop_->removeChannel(this);
 }

//fd得到poller通知以后，处理事件
void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
          handleEventWithGuard(receiveTime);
    }
}

//根据poller通知的channel发生的具体事件，由channel负责具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d", revents_);
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if(revents_ *(EPOLLERR))
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI))
    {
        if(readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT)
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }
}