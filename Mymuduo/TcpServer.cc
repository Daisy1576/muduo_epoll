#include "TcpServer.h"
#include "Logger.h"

#include <functional>

EventLoop* checkLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainloop is null \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, 
                    const InetAddress &listenAddr,
                    const std::string &nameArg,
                    Option option = kNoReusePort)
                    : loop_(checkLoopNotNull(loop))
                    , ipPor_(listenAddr.toIpPort())
                    , name_(nameArg)
                    , acceptor_(new acceptor(loop, listenAddr, option == kReusePort))
                    , threadPool_(new EventLoopThradPool(loop, name_))
                    , connectionCallback_()
                    , nextConnId_(1)
{
    //当有新用户连接时，会执行TcpServer::newConnection，根据轮询算法选择一个subloop,唤醒subloop,
    //把当前connfd封装为channel分发给subloop
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                        std::placeholders::_1, std::placeholders::_2));
}
TcpServer::~TcpServer()
{
    // 遍历所有存活的连接
    for (auto &item : connections_)
    {
        // 1. 用局部shared_ptr接管连接，保证连接在当前作用域内存活
        TcpConnectionPtr conn(item.second);
        // 2. 从connections_容器中移除该连接（减少引用计数）
        item.second.reset();

        // 3. 投递到连接所属的IO线程，执行最终的连接销毁清理
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}


// 处理新客户端连接（由Acceptor回调触发）
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 1. 轮询选择一个subLoop（IO线程）来管理新连接（负载均衡）
    EventLoop *ioLoop = threadPool_->getNextLoop();

    // 2. 生成唯一连接名称（格式：name_-ip:port#connId）
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    // 3. 打印新连接日志
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 4. 通过sockfd获取本地绑定的IP和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 5. 创建TcpConnection对象（智能指针管理生命周期）
    TcpConnectionPtr conn(new TcpConnection(
        ioLoop,
        connName,
        sockfd,
        localAddr,
        peerAddr
    ));

    // 6. 将新连接加入服务器的连接管理容器
    connections_[connName] = conn;

    // 7. 绑定用户设置的各类回调到TcpConnection
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 绑定关闭回调：TcpConnection关闭时通知TcpServer移除连接
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );

    // 8. 投递到IO线程，执行连接建立初始化（注册读事件、触发连接回调）
    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished, conn)
    );
}

    //开启服务器的监听
    void TcpServer::start()
    {
        if(started_++ == 0)   //防止一个tcpserver对象被start多次
        {
            threadPool_->start(threadInitCallback_);  //启动底层的loop线程
            loop_->runInLoop(std::bind(&Acceptor::listen, Acceptor_.get()));
        }
    }

    void TcpServer::setThreadNum(int numThreads)
    {
        threadPool_ ->setThreadNum(numThreads);
    }

// 移除连接（由TcpConnection的closeCallback_回调触发）
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    // 投递到mainLoop（TcpServer所属的EventLoop）中执行，保证线程安全
    loop_->runInLoop(
        std::bind(
            &TcpServer::removeConnectionInLoop,
            this,
            conn
        )
    );
}
    // mainLoop中执行的连接移除逻辑（核心）
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    // 打印连接移除日志
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());

    // 1. 从TcpServer的连接管理容器中移除该连接
    connections_.erase(conn->name());

    // 2. 获取该连接所属的IO线程（subLoop）
    EventLoop *ioLoop = conn->getLoop();

    // 3. 投递到该连接的IO线程，执行最终的连接销毁清理
    ioLoop->queueInLoop(
        std::bind(
            &TcpConnection::connectDestroyed,
            conn
        )
    );
}