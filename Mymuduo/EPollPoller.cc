#include "EPollPoller.h"

#include "Logger.h"

#include <errno.h>

#include <unistd.h>

#include "Channel.h"

#include <cstring>

//channel未添加到poller中
const int KNew = -1;   //channel的index_=-1
//channel已添加到poller中
const int KAdded = 1;
//channel从poller中删除 
const int KDeleted = 2;


EPollPoller::EPollPoller(EventLoop *loop)
    :Poller(loop)
    ,epollfd_(::epoll_create(EPOLL_CLOEXEC))  
    ,events_(KInitEventListSize)
{
    if(epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, Channellist *activeChannels) 
{
    LOG_INFO("func=%s => fd total count: %lu\n", __func__, Channels_.size());
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if(numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == events_.size())
        {
            events_.resize(events_.size()*2);
        }
    }
    else if(numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", _FUNCTION_);
    }
    else
    {
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

/*
                eventloop
            channellist     poller
                           channnelmap
*/
void EPollPoller::updateChannel(Channel *channel) 
{
    const int index = channel->index();
    LOG_INFO("fd = %d events=%d index=%d \n",channel->fd(), channel->events(), index);

    if(index == KNew || KDeleted)
    {
        if(index == KNew)
        {
            int fd = channel->fd();
            Channels_[fd] = channel;
        }
        channel->set_index(KAdded);
        update(EPOLL_CTL_ADD, channel); 
    }
    else  // channel在poller上已经注册过了
    {
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(KDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);

        }
    }


}

void EPollPoller::removeChannel(Channel *channel) 
{
    int fd = channel->fd();
    Channels_.erase(fd);

    int index = channel->index();
    if(index == KAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(KNew);
}

void EPollPoller::fillActiveChannels(int numEvents, Channellist *activateChannels) const  
{
    for(int i=0; i<numEvents;++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activateChannels->push_back(channel); //eventloop就拿到了他的poller给他返回的所有发生事件的channel列表

    }
}

//更新epoll的mod,del操作
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    memset(&event, 0, sizeof event);
    int fd = channel->fd();
    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;
   

    if(::epoll_ctl(epollfd_, operation, fd, &event)<0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_ERROR("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}