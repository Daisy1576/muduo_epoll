#include "Poller.h"

#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if(::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; //生成实例化poll
    }
    else
    {
        return nullptr; //生成epoll的实例
    }
}