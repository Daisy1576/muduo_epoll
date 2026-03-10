#pragma once

#include "Poller.h"  // 必须包含，继承 Poller
#include <sys/epoll.h>

class EPollPoller : public Poller  // 继承后，ChannelList 就在作用域内
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 直接用 ChannelList，不需要 Poller:: 前缀
    Timestamp poll(int timeoutMs, Channellist *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;
    
private:
    static const int KInitEventListSize = 16;

    void fillActiveChannels(int numEvents, Channellist *activateChannels) const;
    void update(int operation, Channel *channel);

    using EventList = std::vector<struct epoll_event>;

    int epollfd_;
    EventList events_;
};