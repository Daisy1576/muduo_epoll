#pragma once

#include "noncopyable.h"

#include "Timestamp.h"

#include <vector>

#include "Channel.h"

#include <unordered_map>

class Channel;
class EventLoop;

//muduo库中多路事件分发器的核心IO复用模块
class Poller:noncopyable
{

public:
    using Channellist = std::vector<Channel*>;

    Poller(EventLoop *Loop);
    virtual ~Poller() = default;

    //保留统一的io复用的接口
    virtual Timestamp poll(int timeoutMs, Channellist *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *Channel) = 0;

    //判断参数channel是否在当前Poller当中
    bool hasChannel(Channel *channel) const;

    //事件循环可以通过该接口获取默认的io复用具体实现 
    static Poller* newDefaultPoller(EventLoop *loop);

protected: 
//map的key:sockfd value； sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap Channels_;
private:
    EventLoop *ownerLoop_;  //定义poller所属的事件循环EventLoop
};