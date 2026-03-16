#include "Acceptor.h"

#include <sys/types.h>

#include <sys/socket.h>

#include "Logger.h"

#include <unistd.h>

static int creatNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listeren socket create err:%d \n", -__FILE__, _FUNCTION_, __LINE__, errno);
    }
}

 Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr;, bool reuseport)
    : loop_(loop)
    , acceptSocket_(creatNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAdde(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);   //绑定套接字

    //Tcpserver::start()
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

    Acceptor:: ~Acceptor()
    {
        acceptChannel_.disableAll();
        acceptChannel_.remove();
    }




void  Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_enableReading();
}
//listenfd有事件发生了，就有新用户连接了
void  Acceptor::headleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept  err:%d \n", -__FILE__, _FUNCTION_, __LINE__, errno);
        if(errno == EMFILE)
        {
             LOG_ERROR("%s:%s:%d sockfd reach   \n", -__FILE__, _FUNCTION_, __LINE__);
        }
    }
}