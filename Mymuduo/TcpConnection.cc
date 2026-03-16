#include "TcpServer.h
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include <functional>
#include <errno.h>
#include "EventLoop.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "InetAddress.h"
#include <string.h>
#include <netinet/tcp.h>

static EventLoop* checkLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d tcpconnection loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
        return loop;
    }
}


TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
  : loop_(CheckLoopNotNull(loop))
  , name_(nameArg)
  , state_(kConnecting)
  , reading_(true)
  , socket_(new Socket(sockfd))  
  , channel_(new Channel(loop, sockfd))
  , localAddr_(localAddr)
  , peerAddr_(peerAddr)
  , highWaterMark_(64*1024*1024)  //64m 
{
//下面channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会调用相应的操作函数
  channel_->setReadCallback(
    std::bind(&TcpConnection::handleRead, this, std::placeholders::_1);
  );
  channel_->setWriteCallback(
    std::bind(&TcpConnection::handleWrite, this);
  );
  channel_->setCloseCallback(
    std::bind(&TcpConnection::handleClose, this);
  );
  channel_->setErrorCallback(
    std::bind(&TcpConnection::handleError, this);

    LOG_INFO("tcpconnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
  );
}

TcpConnection::~TcpConnection()
{
     LOG_INFO("TcpConnection::dtor[%s] at fd = %d state=%d\n",
         name.c_str(), channel_->fd(), state_);
}

void TcpConnection::send(const std::string &buf)
{
    // 仅当连接处于「已连接」状态时，才处理发送逻辑
    if (state_ == kConnected)
    {
        // 检查当前调用线程是否是EventLoop的IO线程
        if (loop_->isInLoopThread())
        {
            // 同线程：直接调用sendInLoop发送数据
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            // 跨线程：把发送任务投递到EventLoop的IO线程执行
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,  // 要执行的发送函数
                this,                        // 当前TcpConnection对象
                buf.c_str(),                 // 待发送数据的指针
                buf.size()                   // 待发送数据长度
            ));
        }
    }
    // 非kConnected状态：静默丢弃（muduo源码中可扩展日志提示）
}
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrite = 0;          // 本次实际写入的字节数
    size_t remaining = len;      // 剩余待发送的字节数
    bool faultError = false;     // 是否发生致命错误（如连接重置）

    // 前置检查：连接已断开，直接放弃发送
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    // 场景1：Channel未监听写事件 + 输出缓冲区无数据 → 尝试直接写socket
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrite = ::write(channel_->fd(), data, len);
        
        if (nwrite >= 0)
        {
            remaining = len - nwrite;
            // 数据全部发送完成 → 触发写完成回调（无需监听写事件）
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, std::shared_from_this())
                );
            }
        }
        else  // nwrite < 0，写操作出错
        {
            nwrite = 0;
            // 非阻塞写的正常"错误"（EAGAIN/EWOULDBLOCK）：socket发送缓冲区满，后续走缓冲区逻辑
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                // 致命错误（SIGPIPE/连接重置）：标记faultError，不再处理
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    // 场景2：无致命错误 + 仍有数据未发送 → 缓存到输出缓冲区 + 启用写事件
    if (!faultError && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes();
        // 高水位检测：缓冲区总数据量超过阈值 → 触发高水位回调
        if (oldLen + remaining >= highWaterMark_    // 总数据≥高水位
            && oldLen < highWaterMark_              // 之前未触发过高水位
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, std::shared_from_this(), oldLen + remaining)
            );
        }
        // 把剩余数据追加到输出缓冲区
        outputBuffer_.append(static_cast<const char*>(message) + nwrite, remaining);
        // 启用Channel的写事件监听 → Poller检测到socket可写时触发handleWrite
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}
// 主动关闭连接（对外暴露的接口）
void TcpConnection::shutdown()
{
    // 仅当连接处于「已连接」状态时，执行关闭逻辑
    if (state_ == kConnected)
    {
        // 1. 更新连接状态为「正在断开」（避免重复关闭）
        setState(kDisconnecting);
        // 2. 把关闭任务投递到IO线程执行（保证线程安全）
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this)
        );
    }
}
// IO线程中执行的关闭逻辑（核心）
void TcpConnection::shutdownInLoop()
{
    // 关键判断：Channel未监听写事件 → 说明outputBuffer_中无待发送数据
    if (!channel_->isWriting())
    {
        // 调用socket的shutdown关闭写端（TCP FIN包）
        // 注意：仅关闭写端，读端仍可接收数据，实现「优雅关闭」
        socket_->shutdownWrite();
    }
    // 若Channel仍监听写事件（说明outputBuffer_有数据）→ 不关闭写端，等handleWrite发完数据后再处理
}
// 连接建立（新TCP连接成功建立后调用）
void TcpConnection::connectEstablished()
{
    // 1. 更新连接状态为「已连接」
    setState(kConnected);
    // 2. 绑定Channel的生命周期到当前TcpConnection（shared_ptr，避免野指针）
    channel_->tie(shared_from_this());
    // 3. 向Poller注册Channel的读事件（EPOLLIN）→ 监听socket可读
    channel_->enableReading();
    // 4. 触发连接建立回调 → 通知上层（TcpServer/用户）新连接建立
    connectionCallback_(shared_from_this());
}
// 连接销毁（TCP连接最终清理时调用）
void TcpConnection::connectDestroyed()
{
    // 1. 仅当连接处于「已连接」状态时，执行清理
    if (state_ == kConnected)
    {
        // 2. 更新连接状态为「已断开」
        setState(kDisconnected);
        // 3. 禁用Channel所有事件（读/写/错误等）→ 从Poller中移除事件监听
        channel_->disableAll();
        // 4. 触发连接断开回调 → 通知上层（TcpServer/用户）连接销毁
        connectionCallback_(shared_from_this());
    }
    // 5. 把Channel从Poller中彻底删除 → 释放fd相关的事件监听资源
    channel_->remove();
}
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    // 从fd读取数据到输入缓冲区inputBuffer_，保存错误码到savedErrno
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    
    if (n > 0)
    {
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        messageCallback_(std::shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        // n=0表示对端关闭连接（TCP FIN包），处理连接关闭
        handleClose();
    }
    else
    {
        // n<0表示读数据出错，处理错误
        errno = savedErrno;
        LOG_ERROR("tcpconnection");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    // 检查当前Channel是否关注了写事件（避免重复处理）
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        // 把输出缓冲区的数据写入socket fd，保存错误码
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        
        if (n > 0)
        {
            // 从输出缓冲区移除已成功写入的n个字节
            outputBuffer_.retrieve(n);
            
            // 如果缓冲区中已无待发送数据
            if (outputBuffer_.readableBytes() == 0)
            {
                // 禁用Channel的写事件监听（避免epoll持续触发写事件）
                channel_->disableWriting();
                
                // 如果用户设置了写完成回调
                if (writeCompleteCallback_)
                {
                    // 把写完成回调放入EventLoop的任务队列，异步执行
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, std::shared_from_this())
                    );
                }
                if(state_ = kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }  
        else
        {
            // 写数据出错的处理（muduo源码中会补充错误日志+错误处理）
           
             LOG_ERROR("tcpconnection2");
            // 可选：触发错误回调或关闭连接
            // handleError();
        }
    }
    else
    {
         LOG_ERROR("tcpconnection3");
    }
}
void TcpConnection::handleClose()
{
    // 1. 打印关闭日志（fd/状态）
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), state_);
    // 2. 更新连接状态为「已断开」
    setState(kDisconnected);
    // 3. 禁用Channel所有事件监听（读/写），避免后续事件触发
    channel_->disableAll();
    // 4. 获取当前连接的shared_ptr（避免对象提前析构）
    TcpConnectionPtr connPtr(std::shared_from_this());
    // 5. 触发连接关闭的上层回调（通知TcpServer/用户）
    connectionCallback_(connPtr);  // 连接状态变更回调
    closeCallback_(connPtr);       // 连接关闭专属回调
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    // 调用系统函数获取socket的错误状态（SO_ERROR）
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        // 获取失败时，使用全局errno作为错误码
        err = errno;
    }
    else
    {
        // 获取成功时，使用socket的SO_ERROR值作为错误码
        err = optval;
    }
    // 打印错误日志（连接名称 + 错误码）
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}